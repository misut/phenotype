module;
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <new>
#include <string>
#include <utility>
#include <vector>
export module phenotype.state;

import phenotype.types;
import phenotype.diag;

export namespace phenotype {

// ============================================================
// Arena — generational, fixed-capacity slot pool for LayoutNode
// ============================================================
//
// Stores LayoutNodes in a contiguous typed array with a parallel
// generations array. alloc_node() returns a NodeHandle = (index, gen);
// reset() bumps current_gen so prior handles fail get() deref. Storage
// addresses are stable for the lifetime of one epoch (between resets),
// so layout/paint can hold transient `LayoutNode&` references during
// a build pass.

struct Arena {
    static constexpr std::uint32_t MAX_NODES = 4096;

    LayoutNode* nodes = nullptr;
    std::uint32_t* generations = nullptr;
    std::uint32_t node_count = 0;
    std::uint32_t current_gen = 0;

    void ensure_init() {
        if (!nodes) {
            nodes = static_cast<LayoutNode*>(
                ::operator new(sizeof(LayoutNode) * MAX_NODES));
            generations = new std::uint32_t[MAX_NODES]{};
        }
    }

    NodeHandle alloc_node() {
        ensure_init();
        if (node_count >= MAX_NODES) {
            log::error("phenotype.arena",
                "node count exhausted (max {})", MAX_NODES);
            std::abort();
        }
        new (&nodes[node_count]) LayoutNode();
        generations[node_count] = current_gen;
        metrics::inst::alloc_nodes.add();
        metrics::inst::live_nodes.set(static_cast<std::int64_t>(node_count + 1));
        metrics::inst::max_nodes_seen.update_max(static_cast<std::int64_t>(node_count + 1));
        return {node_count++, current_gen};
    }

    LayoutNode* get(NodeHandle h) noexcept {
        if (!h.valid() || h.index >= node_count) return nullptr;
        if (generations[h.index] != h.generation) return nullptr;
        return &nodes[h.index];
    }

    LayoutNode& must_get(NodeHandle h) {
        auto* p = get(h);
        if (!p) {
            log::error("phenotype.arena",
                "stale NodeHandle (index={} gen={} current_gen={})",
                h.index, h.generation, current_gen);
            std::abort();
        }
        return *p;
    }

    void reset() {
        for (std::uint32_t i = 0; i < node_count; ++i) nodes[i].~LayoutNode();
        metrics::inst::arena_resets.add();
        node_count = 0;
        metrics::inst::live_nodes.set(0);
        ++current_gen; // invalidate all outstanding handles
    }
};

// ============================================================
// AppState — consolidated global state
// ============================================================

// Type-erased text-input dispatcher used by the new message DSL.
// New TextField registers one of these per call site so that
// phenotype_handle_key can route the new value into a typed mapper +
// post<Msg>. Click callbacks still go through `callbacks` (above).
//
// `current` holds the actual input value as the view rendered it
// (separate from the displayed text, which may be the placeholder when
// empty). phenotype_handle_key reads it, applies the keystroke, and
// passes the new full string to `invoke`.
struct InputHandler {
    std::string current;                   // value as the view rendered it
    void* state;                           // captured state (e.g. mapper fn pointer storage)
    void (*invoke)(void*, std::string);   // trampoline: calls user mapper + post + trigger
    void (*deleter)(void*);                // free `state`
    InputHandler() : state(nullptr), invoke(nullptr), deleter(nullptr) {}
    InputHandler(std::string c, void* s, void (*i)(void*, std::string), void (*d)(void*))
        : current(std::move(c)), state(s), invoke(i), deleter(d) {}
    InputHandler(InputHandler&& o) noexcept
        : current(std::move(o.current)), state(o.state),
          invoke(o.invoke), deleter(o.deleter) {
        o.state = nullptr; o.invoke = nullptr; o.deleter = nullptr;
    }
    InputHandler& operator=(InputHandler&& o) noexcept {
        if (this != &o) {
            if (deleter && state) deleter(state);
            current = std::move(o.current);
            state = o.state; invoke = o.invoke; deleter = o.deleter;
            o.state = nullptr; o.invoke = nullptr; o.deleter = nullptr;
        }
        return *this;
    }
    ~InputHandler() { if (deleter && state) deleter(state); }
    InputHandler(InputHandler const&) = delete;
    InputHandler& operator=(InputHandler const&) = delete;
};

struct AppState {
    Theme theme;
    Arena arena;
    NodeHandle root = NodeHandle::null();
    float scroll_y = 0;
    unsigned int hovered_id = 0xFFFFFFFF;
    unsigned int focused_id = 0xFFFFFFFF;
    unsigned int caret_pos = 0;
    bool caret_visible = true;
    // Click callbacks indexed by callback_id, registered by Button<Msg>.
    std::vector<std::function<void()>> callbacks;
    std::vector<unsigned int> focusable_ids;
    // Tracks input nodes by callback_id for focus / handle_key lookup.
    std::vector<std::pair<unsigned int, NodeHandle>> input_nodes;
    // Installed by phenotype::run<State, Msg>(...). trigger_rebuild calls it.
    void (*app_runner)() = nullptr;
    // Sparse map of typed text-input dispatchers, registered by TextField<Msg>.
    std::vector<std::pair<unsigned int, InputHandler>> input_handlers;
};

namespace detail {
    // Placement-new into static storage to avoid global destructor.
    // wasi-sdk's crt1-command.o calls __cxa_atexit on exit, which destroys
    // global C++ objects. Without this trick, g_app's destructors run after
    // _start() and clear the callbacks vector, breaking event handling.
    alignas(AppState) inline unsigned char g_app_storage[sizeof(AppState)];
    inline AppState& g_app = *new (&g_app_storage) AppState();
    inline NodeHandle alloc_node() { return g_app.arena.alloc_node(); }
    inline LayoutNode& node_at(NodeHandle h) { return g_app.arena.must_get(h); }

    // ============================================================
    // Message dispatcher — type-erased message queue for the new DSL
    // ============================================================
    //
    // Each Button<Msg>(label, msg) registers a callback that calls
    // detail::post<Msg>(msg), then trigger_rebuild(). The runner installed
    // by run<State, Msg>(view, update) drains the queue, folds via
    // update(state, msg), and calls view(state) to rebuild the tree.
    //
    // Storage is type-erased to keep AppState free of template parameters.

    struct DispatchedMsg {
        void* storage;
        void (*deleter)(void*);
        DispatchedMsg() : storage(nullptr), deleter(nullptr) {}
        DispatchedMsg(void* s, void (*d)(void*)) : storage(s), deleter(d) {}
        DispatchedMsg(DispatchedMsg&& o) noexcept
            : storage(o.storage), deleter(o.deleter) {
            o.storage = nullptr; o.deleter = nullptr;
        }
        DispatchedMsg& operator=(DispatchedMsg&& o) noexcept {
            if (this != &o) {
                if (deleter && storage) deleter(storage);
                storage = o.storage; deleter = o.deleter;
                o.storage = nullptr; o.deleter = nullptr;
            }
            return *this;
        }
        ~DispatchedMsg() { if (deleter && storage) deleter(storage); }
        DispatchedMsg(DispatchedMsg const&) = delete;
        DispatchedMsg& operator=(DispatchedMsg const&) = delete;
    };

    inline std::vector<DispatchedMsg>& msg_queue() {
        static std::vector<DispatchedMsg> q;
        return q;
    }

    template<typename Msg>
    void post(Msg msg) {
        auto* p = new Msg(std::move(msg));
        auto& q = msg_queue();
        q.emplace_back(
            p,
            [](void* ptr) { delete static_cast<Msg*>(ptr); }
        );
        metrics::inst::messages_posted.add();
        metrics::inst::max_queue_depth.update_max(
            static_cast<std::int64_t>(q.size()));
    }

    template<typename Msg>
    std::vector<Msg> drain() {
        auto& q = msg_queue();
        metrics::inst::messages_drained.add(static_cast<std::uint64_t>(q.size()));
        std::vector<Msg> out;
        out.reserve(q.size());
        for (auto& dm : q) {
            out.push_back(std::move(*static_cast<Msg*>(dm.storage)));
            // dm's destructor will free the moved-from storage; that's fine
            // since the move leaves the Msg in a valid (empty) state.
        }
        q.clear();
        return out;
    }

    inline void install_app_runner(void (*runner)()) { g_app.app_runner = runner; }

    inline void trigger_rebuild() {
        if (g_app.app_runner) g_app.app_runner();
    }
} // namespace detail

// ============================================================
// Scope — implicit parent tracking for declarative DSL
// ============================================================

// Scope — implicit parent tracking for nested DSL builders.
// Used by attach_to_scope/open_container to thread NodeHandles through
// Column/Row/Box/Scaffold/Card builder lambdas without explicit args.
struct Scope {
    NodeHandle node;

    Scope(NodeHandle n) : node(n) {}

    static Scope*& current() {
        static Scope* s = nullptr;
        return s;
    }

    static void set_current(Scope* s) { current() = s; }
};

} // namespace phenotype
