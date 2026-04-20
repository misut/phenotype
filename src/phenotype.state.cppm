module;
#include <cstdint>
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

struct FocusedInputSnapshot {
    bool valid = false;
    unsigned int callback_id = 0xFFFFFFFFu;
    InteractionRole role = InteractionRole::None;
    float x = 0.0f;
    float y = 0.0f;
    float width = 0.0f;
    float height = 0.0f;
    float font_size = 0.0f;
    float line_height = 0.0f;
    bool mono = false;
    float padding[4] = {};
    Color background = {255, 255, 255, 255};
    Color foreground = {0, 0, 0, 255};
    Color placeholder_color = {0, 0, 0, 255};
    Color accent = {0, 0, 0, 255};
    unsigned int caret_pos = 0xFFFFFFFFu;
    unsigned int selection_anchor = 0xFFFFFFFFu;
    bool selection_active = false;
    unsigned int selection_start = 0xFFFFFFFFu;
    unsigned int selection_end = 0xFFFFFFFFu;
    bool caret_visible = true;
    std::string value;
    std::string placeholder;
};

struct FocusedInputCaretLayout {
    bool valid = false;
    bool visible = false;
    bool composition_active = false;
    std::size_t caret_bytes = 0;
    float content_x = 0.0f;
    float content_y = 0.0f;
    float draw_x = 0.0f;
    float draw_y = 0.0f;
    float height = 0.0f;
};

struct AppState {
    Theme theme;
    Arena arena;
    Arena prev_arena;       // previous frame's tree, kept alive for diff
    NodeHandle root = NodeHandle::null();
    NodeHandle prev_root = NodeHandle::null();
    float scroll_y = 0;
    unsigned int hovered_id = 0xFFFFFFFF;
    unsigned int focused_id = 0xFFFFFFFF;
    unsigned int caret_pos = 0xFFFFFFFFu;
    unsigned int selection_anchor = 0xFFFFFFFFu;
    bool caret_visible = true;
    // Click callbacks indexed by callback_id, registered by Button<Msg>.
    std::vector<std::function<void()>> callbacks;
    std::vector<InteractionRole> callback_roles;
    std::vector<unsigned int> focusable_ids;
    // Tracks input nodes by callback_id for focus / handle_key lookup.
    std::vector<std::pair<unsigned int, NodeHandle>> input_nodes;
    // Installed by phenotype::run<State, Msg>(...). trigger_rebuild calls it.
    void (*app_runner)() = nullptr;
    // Sparse map of typed text-input dispatchers, registered by TextField<Msg>.
    std::vector<std::pair<unsigned int, InputHandler>> input_handlers;
    // FNV-1a 64-bit hash of the previous frame's cmd buffer. Used by
    // phenotype::flush_if_changed() to skip the JS↔WASM phenotype_flush
    // round-trip when the new paint pass produces the exact same bytes
    // (e.g. caret-blink frames where the canvas no longer paints the
    // caret because of the HTML overlay from PR #31). 0 sentinel means
    // "no previous frame yet" — the first paint always flushes.
    std::uint64_t last_paint_hash = 0;
    float debug_viewport_width = 0.0f;
    float debug_viewport_height = 0.0f;
    diag::InputDebugSnapshot input_debug;
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

    // Function pointer for URL opening — set by the backend module
    // (phenotype.wasm or phenotype.native) at initialization time.
    // Keeps widget::link non-templated.
    inline void (*g_open_url)(char const*, unsigned int) = nullptr;

    inline FocusedInputSnapshot focused_input_snapshot() {
        FocusedInputSnapshot snapshot{};
        if (g_app.focused_id == 0xFFFFFFFFu)
            return snapshot;

        NodeHandle input_h = NodeHandle::null();
        for (auto const& [id, node_h] : g_app.input_nodes) {
            if (id == g_app.focused_id) {
                input_h = node_h;
                break;
            }
        }
        if (!input_h.valid())
            return snapshot;

        auto* input = g_app.arena.get(input_h);
        if (!input || !input->is_input)
            return snapshot;

        std::string current_value;
        for (auto const& [id, handler] : g_app.input_handlers) {
            if (id == g_app.focused_id) {
                current_value = handler.current;
                break;
            }
        }

        float ax = 0.0f;
        float ay = 0.0f;
        auto cursor = input_h;
        while (cursor.valid()) {
            auto* node = g_app.arena.get(cursor);
            if (!node) break;
            ax += node->x;
            ay += node->y;
            cursor = node->parent;
        }

        snapshot.valid = true;
        snapshot.callback_id = g_app.focused_id;
        snapshot.role = input->interaction_role;
        snapshot.x = ax;
        snapshot.y = ay;
        snapshot.width = input->width;
        snapshot.height = input->height;
        snapshot.font_size = input->font_size;
        snapshot.line_height = input->font_size * g_app.theme.line_height_ratio;
        snapshot.mono = input->mono;
        for (int i = 0; i < 4; ++i)
            snapshot.padding[i] = input->style.padding[i];
        snapshot.background = input->background;
        snapshot.foreground = g_app.theme.foreground;
        snapshot.placeholder_color = g_app.theme.muted;
        snapshot.accent = g_app.theme.accent;
        snapshot.caret_pos = g_app.caret_pos;
        snapshot.selection_anchor = g_app.selection_anchor;
        snapshot.caret_visible = g_app.caret_visible;
        snapshot.value = std::move(current_value);
        snapshot.placeholder = input->placeholder;
        auto clamp_boundary = [](std::string const& value, std::size_t pos) {
            if (pos > value.size())
                pos = value.size();
            while (pos > 0
                   && pos < value.size()
                   && (static_cast<unsigned char>(value[pos]) & 0xC0u) == 0x80u) {
                --pos;
            }
            return pos;
        };
        auto caret = clamp_boundary(snapshot.value, snapshot.caret_pos);
        auto anchor = (snapshot.selection_anchor == 0xFFFFFFFFu)
            ? caret
            : clamp_boundary(snapshot.value, snapshot.selection_anchor);
        snapshot.selection_active = anchor != caret;
        snapshot.selection_start = static_cast<unsigned int>((anchor < caret) ? anchor : caret);
        snapshot.selection_end = static_cast<unsigned int>((anchor > caret) ? anchor : caret);
        return snapshot;
    }

    inline bool is_utf8_continuation_byte(unsigned char ch) {
        return (ch & 0xC0u) == 0x80u;
    }

    inline std::size_t clamp_caret_utf8_boundary(std::string const& value, std::size_t pos) {
        if (pos > value.size())
            pos = value.size();
        while (pos > 0
               && pos < value.size()
               && is_utf8_continuation_byte(static_cast<unsigned char>(value[pos]))) {
            --pos;
        }
        return pos;
    }

    template<typename MeasurePrefix>
    inline FocusedInputCaretLayout compute_single_line_caret_layout(
            FocusedInputSnapshot const& snapshot,
            float scroll_y,
            bool composition_active,
            MeasurePrefix&& measure_prefix) {
        FocusedInputCaretLayout layout{};
        if (!snapshot.valid)
            return layout;

        layout.valid = true;
        layout.visible = snapshot.caret_visible;
        layout.composition_active = composition_active;
        if (snapshot.caret_pos == 0xFFFFFFFFu) {
            layout.caret_bytes = snapshot.value.size();
        } else {
            layout.caret_bytes = clamp_caret_utf8_boundary(snapshot.value, snapshot.caret_pos);
        }

        float prefix_width = measure_prefix(snapshot, layout.caret_bytes);
        layout.content_x = snapshot.x + snapshot.padding[3] + prefix_width;
        layout.content_y = snapshot.y + snapshot.padding[0];
        layout.draw_x = layout.content_x;
        layout.draw_y = layout.content_y - scroll_y;
        layout.height = snapshot.line_height;
        return layout;
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
