module;
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <new>
#include <source_location>
#include <typeindex>
#include <utility>
#include <vector>
export module phenotype.state;

import phenotype.types;

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
            std::fprintf(stderr,
                "phenotype: arena node count exhausted (max %u)\n",
                MAX_NODES);
            std::abort();
        }
        new (&nodes[node_count]) LayoutNode();
        generations[node_count] = current_gen;
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
            std::fprintf(stderr,
                "phenotype: stale NodeHandle (index=%u gen=%u current_gen=%u)\n",
                h.index, h.generation, current_gen);
            std::abort();
        }
        return *p;
    }

    void reset() {
        for (std::uint32_t i = 0; i < node_count; ++i) nodes[i].~LayoutNode();
        node_count = 0;
        ++current_gen; // invalidate all outstanding handles
    }
};

// ============================================================
// StateSlot — type-erased slot for hook-style Trait persistence
// ============================================================

struct StateSlot {
    void* ptr;
    void (*deleter)(void*);
    std::type_index type;          // recorded at first encode<T>(), checked on reuse
    std::source_location origin;   // call site of the first encode<T>()

    StateSlot(void* p, void (*d)(void*),
              std::type_index t, std::source_location o)
        : ptr(p), deleter(d), type(t), origin(o) {}
    StateSlot(StateSlot&& o) noexcept
        : ptr(o.ptr), deleter(o.deleter),
          type(o.type), origin(o.origin) {
        o.ptr = nullptr; o.deleter = nullptr;
    }
    StateSlot& operator=(StateSlot&& o) noexcept {
        if (this != &o) {
            if (deleter) deleter(ptr);
            ptr = o.ptr; deleter = o.deleter;
            type = o.type; origin = o.origin;
            o.ptr = nullptr; o.deleter = nullptr;
        }
        return *this;
    }
    ~StateSlot() { if (deleter) deleter(ptr); }

    StateSlot(StateSlot const&) = delete;
    StateSlot& operator=(StateSlot const&) = delete;
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
    std::vector<std::function<void()>> callbacks;
    std::vector<StateSlot> states;
    std::vector<unsigned int> focusable_ids;
    std::vector<std::pair<unsigned int, NodeHandle>> input_nodes;
    std::function<void()> app_builder;
    unsigned int encode_index = 0; // hook-style index for encode() across rebuilds
    // Installed by express() in the umbrella module so the engine layer
    // (Trait::set / Scope::mutate) can re-enter rebuild without
    // depending on layout/paint at the type level.
    void (*rebuild_fn)() = nullptr;
    // Installed by phenotype::run<State, Msg>(...) for the message DSL.
    // When set, trigger_rebuild() invokes this instead of rebuild_fn so the
    // runner can drain the message queue, fold via update, and re-view.
    void (*app_runner)() = nullptr;
    // New message DSL: typed text-input dispatchers, sparse by callback id.
    // Only TextField<Msg> registers entries here; lookups walk the vector.
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
        msg_queue().emplace_back(
            p,
            [](void* ptr) { delete static_cast<Msg*>(ptr); }
        );
    }

    template<typename Msg>
    std::vector<Msg> drain() {
        auto& q = msg_queue();
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
        // Prefer the new typed runner if installed; fall back to the legacy
        // rebuild_fn so the old DSL still works during step 1+2.
        if (g_app.app_runner) g_app.app_runner();
        else if (g_app.rebuild_fn) g_app.rebuild_fn();
    }
} // namespace detail

// ============================================================
// Scope — implicit parent tracking for declarative DSL
// ============================================================

struct Scope {
    NodeHandle node;
    std::function<void()> builder;

    Scope(NodeHandle n) : node(n) {}
    Scope(NodeHandle n, std::function<void()> b) : node(n), builder(std::move(b)) {}

    void mutate() {
        if (detail::g_app.rebuild_fn) detail::g_app.rebuild_fn();
    }

    static Scope*& current() {
        static Scope* s = nullptr;
        return s;
    }

    static void set_current(Scope* s) { current() = s; }
};

// ============================================================
// Trait<T> — reactive state with full-rebuild on mutation
// ============================================================

template<typename T>
class Trait {
    T value_;

public:
    explicit Trait(T initial) : value_(std::move(initial)) {}

    T const& value() noexcept { return value_; }

    void set(T new_val) {
        if (value_ == new_val) return;
        value_ = std::move(new_val);
        if (detail::g_app.rebuild_fn) detail::g_app.rebuild_fn();
    }
};

template<typename T>
Trait<T>* encode(T initial,
                 std::source_location loc = std::source_location::current()) {
    auto& app = detail::g_app;
    // Hook-style: reuse existing Trait on rebuild (preserves state).
    // Validates type AND call site so a reordered or conditional encode()
    // chain trips an immediate abort instead of silently aliasing slots.
    // Mirrors React's "Rendered fewer/more hooks than expected" guard,
    // strengthened with std::source_location.
    if (app.encode_index < app.states.size()) {
        auto& slot = app.states[app.encode_index];
        auto want = std::type_index(typeid(Trait<T>));
        if (slot.type != want) {
            std::fprintf(stderr,
                "phenotype: encode() type mismatch at index %u "
                "(slot is %s, encode<T> got %s) — hook order changed at %s:%u "
                "(slot first registered at %s:%u)\n",
                app.encode_index, slot.type.name(), want.name(),
                loc.file_name(), loc.line(),
                slot.origin.file_name(), slot.origin.line());
            std::abort();
        }
        if (slot.origin.line() != loc.line() ||
            std::strcmp(slot.origin.file_name(), loc.file_name()) != 0) {
            std::fprintf(stderr,
                "phenotype: encode() call site moved at index %u "
                "(was %s:%u, now %s:%u)\n",
                app.encode_index,
                slot.origin.file_name(), slot.origin.line(),
                loc.file_name(), loc.line());
            std::abort();
        }
        ++app.encode_index;
        return static_cast<Trait<T>*>(slot.ptr);
    }
    auto* p = new Trait<T>(std::move(initial));
    app.states.push_back(StateSlot{
        p,
        [](void* ptr) { delete static_cast<Trait<T>*>(ptr); },
        std::type_index(typeid(Trait<T>)),
        loc,
    });
    app.encode_index++;
    return p;
}

} // namespace phenotype
