module;
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <map>
#include <new>
#include <source_location>
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
    float scroll_x = 0;
    float scroll_y = 0;
    unsigned int hovered_id = 0xFFFFFFFF;
    unsigned int focused_id = 0xFFFFFFFF;
    unsigned int caret_pos = 0xFFFFFFFFu;
    unsigned int selection_anchor = 0xFFFFFFFFu;
    bool caret_visible = true;
    // Click callbacks indexed by callback_id, registered by Button<Msg>.
    std::vector<std::function<void()>> callbacks;
    std::vector<InteractionRole> callback_roles;
    // Gesture callbacks indexed by gesture_callback_id, registered by
    // widget::canvas when the builder passes an `on_gesture` lambda.
    // Cleared and repopulated each frame in lockstep with `callbacks`.
    std::vector<std::function<void(::phenotype::GestureEvent const&)>> gesture_callbacks;
    // Active gesture target — set by paint_node when it walks a canvas
    // node whose `gesture_callback_id` is valid. The shell reads these
    // during `dispatch_gesture` to (a) reject events whose focal point
    // falls outside the canvas bounds, and (b) translate from absolute
    // surface coords into canvas-local coords before invoking the
    // registered callback. `id == 0xFFFFFFFFu` means no canvas
    // currently exposes a gesture handler.
    unsigned int gesture_target_id = 0xFFFFFFFFu;
    float        gesture_target_x  = 0.0f;
    float        gesture_target_y  = 0.0f;
    float        gesture_target_w  = 0.0f;
    float        gesture_target_h  = 0.0f;
    // Scroll targets — populated by paint_node when it walks a node
    // whose `is_scroll_container` is set. Each entry records the
    // viewport's absolute rect (post all parent scrolls) and the
    // pointer back into framework_local where the offset lives. The
    // shell's wheel dispatcher walks this list in reverse (last entry
    // = innermost scroll_view in paint order) and routes the delta
    // into the topmost target whose rect contains the cursor.
    // Cleared at the start of every view rebuild so stale entries from
    // a prior frame can't catch a wheel event.
    struct ScrollTarget {
        float        x = 0.0f;
        float        y = 0.0f;
        float        w = 0.0f;
        float        h = 0.0f;
        ScrollState* state = nullptr;
        float        content_height = 0.0f;
    };
    std::vector<ScrollTarget> scroll_targets;
    // Root-level alternates rendered after the main tree finishes. Each
    // entry is a NodeHandle from the same per-frame arena that the
    // root tree uses, but the overlay's subtree is *not* attached to
    // root.children — the runner walks `overlays` in declaration order
    // for layout and paint after the main pass, which puts overlay
    // HitRegions last in the cmd buffer (so reverse-iteration hit_test
    // finds them first) and lets overlays render on top of the main
    // content. Cleared at the start of every view rebuild.
    std::vector<NodeHandle> overlays;
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

    // Set during view by `animate_value` whenever an interpolation
    // hasn't reached its target yet, cleared at the start of every
    // view. Native shells read this after each rebuild and schedule
    // another paint on a ~16ms cadence while it stays true, so a
    // hover fade or accordion slide keeps advancing without needing
    // input events. The runner does not read it — it's purely a
    // "wake me up next frame" signal for the host loop.
    bool has_active_animations = false;

    // Subtree paint cache — snapshot of the previous frame's command
    // buffer, kept so paint_node can memcpy a clean subtree's byte range
    // out of it instead of re-walking. Matches the 65536-byte buffer
    // size used by the WASM `phenotype_cmd_buf` and the render_backend
    // concept's `buf_size()`.
    static constexpr std::uint32_t PAINT_CACHE_BUF_SIZE = 65536;
    unsigned char prev_cmd_buf[PAINT_CACHE_BUF_SIZE]{};
    std::uint32_t prev_cmd_len = 0;
    float         prev_scroll_x = 0.0f;
    float         prev_scroll_y = 0.0f;
    unsigned int  prev_hovered_id = 0xFFFFFFFFu;
    unsigned int  prev_focused_id = 0xFFFFFFFFu;
    // Computed once per frame by the runner before paint; OR'd against
    // each node's paint_callback_mask to decide whether a blit is safe.
    // Non-zero bits correspond to callback_ids whose hover/focus state
    // transitioned between prev and current frame — those subtrees must
    // re-walk. Also includes the currently-focused input's id so text
    // input caret/selection changes always re-walk that subtree.
    std::uint64_t paint_invalidation_mask = 0;
    // Monotonic count of times the command buffer was flushed mid-paint
    // (buffer overflow inside emit_*). When this increments during a
    // node's walk the recorded paint_offset no longer points at the
    // same bytes, so paint_node must suppress paint_valid for that
    // subtree. Ticked by the ensure() helper in phenotype.paint.
    std::uint64_t paint_flush_epoch = 0;
    // Nested-scissor guard for paint_node. The four target graphics
    // APIs (Metal, D3D12, Vulkan, WebGPU) do not support scissor
    // nesting, so paint_node only emits a Scissor command when this
    // depth is zero. Incremented around the wrapped walk, decremented
    // after — always balanced within a single paint pass.
    unsigned int paint_scissor_depth = 0;

    // Currently-painting node context, snapshotted on paint_node entry
    // and restored on exit by an RAII guard inside phenotype.paint.
    // Read by report_paint_overflow so the stderr line identifies which
    // canvas / widget triggered the drop. Sentinel callback_id ==
    // 0xFFFFFFFFu means "no paint pass active" (emit_* called outside
    // the tree walk).
    unsigned int current_paint_callback_id = 0xFFFFFFFFu;
    float        current_paint_ax = 0.0f;
    float        current_paint_ay = 0.0f;
    float        current_paint_w  = 0.0f;
    float        current_paint_h  = 0.0f;

    // Test-only escape hatch: flips std::abort() inside
    // report_paint_overflow off in debug builds so a regression test
    // can verify the no-abort recovery path. Production code never
    // touches this — default true in debug, ignored in release.
    bool diag_abort_on_paint_overflow = true;
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

    // ============================================================
    // framework_local — call-site-positional widget state store
    // ============================================================
    //
    // Supplies a per-widget escape hatch from the single-AppState rule.
    // Use cases like a ScrollView's scroll offset, an Accordion's open
    // flag, a Tooltip's hover-delay timer, or a Dropdown's menu state
    // are properties of the *widget* rather than the *application*, so
    // forcing them into the user's `State` (and the Msg/update cycle
    // that mutates it) hurts ergonomics for no architectural gain. This
    // store keeps such state alive across frames keyed by the call site
    // of the `framework_local<T>()` invocation, in the same way React
    // fibers / Compose `currentCompositeKeyHash` do.
    //
    // Lifetime: entries are tagged with `local_gen()` at every access;
    // `prune_local_store()` (called by the runner after each `view`)
    // drops entries whose tag is stale, so leaving a widget out of the
    // tree implicitly destroys its local state. `bump_local_gen()`
    // monotonically advances the tag at frame start.
    //
    // Storage uses std::map keyed by widget_id rather than
    // std::unordered_map for the same reason `measure_cache` does —
    // libc++'s unordered_map's __hash_memory symbol is missing on
    // wasi-sdk's clang-22 link line. log(n) over a few dozen widget
    // states is dwarfed by the host-call savings the store enables.

    struct LocalEntry {
        std::uint32_t last_seen_gen = 0;
        void* storage = nullptr;
        void (*deleter)(void*) = nullptr;
        // Identity tag of the type T this entry was created for.
        // Compared on subsequent access to catch the "same call site,
        // different T" footgun (e.g. an in-place edit that changes
        // `framework_local<int>(0)` to `framework_local<float>(0)` —
        // raw reinterpret_cast would silently produce UB otherwise).
        void const* type_id = nullptr;

        LocalEntry() = default;
        LocalEntry(LocalEntry&& o) noexcept
            : last_seen_gen(o.last_seen_gen),
              storage(o.storage),
              deleter(o.deleter),
              type_id(o.type_id) {
            o.storage = nullptr;
            o.deleter = nullptr;
            o.type_id = nullptr;
        }
        LocalEntry& operator=(LocalEntry&& o) noexcept {
            if (this != &o) {
                if (deleter && storage) deleter(storage);
                last_seen_gen = o.last_seen_gen;
                storage = o.storage;
                deleter = o.deleter;
                type_id = o.type_id;
                o.storage = nullptr;
                o.deleter = nullptr;
                o.type_id = nullptr;
            }
            return *this;
        }
        ~LocalEntry() { if (deleter && storage) deleter(storage); }
        LocalEntry(LocalEntry const&) = delete;
        LocalEntry& operator=(LocalEntry const&) = delete;
    };

    // Heap-bound for the same wasi-sdk __cxa_finalize reason as
    // measure_cache and g_app — keep alive past _start() exit so JS-
    // driven rebuilds after termination still see consistent state.
    inline std::map<std::size_t, LocalEntry>& local_store() {
        static std::map<std::size_t, LocalEntry>& m
            = *new std::map<std::size_t, LocalEntry>();
        return m;
    }

    inline std::uint32_t& local_gen() {
        static std::uint32_t g = 1;
        return g;
    }

    inline void bump_local_gen() {
        auto& g = local_gen();
        ++g;
        if (g == 0) g = 1;  // skip zero, used as "uninitialised" sentinel
    }

    inline void prune_local_store() {
        auto cur = local_gen();
        auto& m = local_store();
        for (auto it = m.begin(); it != m.end(); ) {
            if (it->second.last_seen_gen != cur)
                it = m.erase(it);
            else
                ++it;
        }
    }

    // Splittable hash mixer (Boost-style). Used for widget_id derivation
    // from (parent seed, source_location, key, sibling counter). The
    // exact mix isn't load-bearing — collision-resistance just needs to
    // be good enough that distinct call paths land in distinct std::map
    // buckets in practice.
    inline std::size_t hash_combine(std::size_t a, std::size_t b) noexcept {
        a ^= b + 0x9e3779b97f4a7c15ULL + (a << 6) + (a >> 2);
        return a;
    }

    inline std::size_t hash_source_location(
            std::source_location const& loc) noexcept {
        // file_name is a string literal whose pointer value is stable
        // for the program's lifetime — hashing the pointer is cheaper
        // than hashing the bytes and just as discriminating in practice.
        std::size_t h = reinterpret_cast<std::size_t>(loc.file_name());
        h = hash_combine(h, static_cast<std::size_t>(loc.line()));
        h = hash_combine(h, static_cast<std::size_t>(loc.column()));
        return h;
    }

    // Returns the type-identity tag for T. Address of a function-local
    // static is unique per template instantiation and stable across TUs
    // that include the same module, which is all framework_local needs
    // to detect "same id, different T" misuses.
    template <typename T>
    inline void const* type_tag() noexcept {
        static char tag = 0;
        return &tag;
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
//
// `call_counters` keeps a small flat list of (call_site_hash, count)
// pairs visited during this scope's lifetime so framework_local<T>()
// invocations at the same source_location can resolve to distinct
// widget_ids without the caller having to thread an explicit key —
// covers the common "same line repeated twice in this builder" case.
// The vector is expected to hold a handful of entries per scope; if
// profiling ever shows it dominates, swap for std::map.
//
// `widget_id_seed` is reserved for future container-driven seeding so
// that two columns at the same source_location nested in different
// parents can produce non-colliding widget_ids without explicit keys.
// Today every container constructs its Scope with seed=0, so cross-
// scope collisions are still possible — callers that need to resolve
// them should pass an explicit key to framework_local<T>() (typically
// the loop index when iterating).
struct Scope {
    NodeHandle node;
    std::size_t widget_id_seed = 0;
    std::vector<std::pair<std::size_t, std::uint32_t>> call_counters;

    Scope(NodeHandle n) : node(n) {}
    Scope(NodeHandle n, std::size_t seed)
        : node(n), widget_id_seed(seed) {}

    static Scope*& current() {
        static Scope* s = nullptr;
        return s;
    }

    static void set_current(Scope* s) { current() = s; }

    // Returns the next sibling counter for `call_site_hash` within this
    // scope, then increments the stored count so the next call at the
    // same site sees a different value.
    std::uint32_t next_call_counter(std::size_t call_site_hash) {
        for (auto& [h, c] : call_counters) {
            if (h == call_site_hash) {
                auto v = c;
                ++c;
                return v;
            }
        }
        call_counters.push_back({call_site_hash, 1});
        return 0;
    }
};

// ============================================================
// framework_local<T> — call-site-positional widget state
// ============================================================
//
// Returns a reference to a T whose lifetime spans frames, identified by
// the call site of this invocation. Use for state that belongs to the
// widget tree position rather than the application — scroll offsets,
// open/close flags, hover-delay timers, animation progress that will
// eventually back the view-time animator.
//
// Identity derivation:
//   * `loc` (defaulted via std::source_location::current()) seeds the
//     base id from file pointer + line + column.
//   * `key` lets the caller disambiguate sibling iterations of the
//     same source_location (typically the loop index).
//   * The current Scope's per-call-site counter disambiguates
//     repeated calls at the same source_location within one scope
//     without requiring an explicit key.
//
// Limitation: two scopes constructed with seed=0 (the default for
// every container today) cannot distinguish framework_local calls at
// the same source_location nested in each. Pass an explicit key
// in such cases (e.g. `framework_local<int>(0, item_id)` inside a
// keyed list). A future PR will thread parent_id through Scope
// construction so the seed is non-zero by default.
//
// Type safety: each call site pins to a single T; reusing the same
// site with a different T will std::abort with a clear diagnostic
// rather than silently UB-cast the storage.
template <typename T>
T& framework_local(T initial = T{},
                   std::size_t key = 0,
                   std::source_location loc = std::source_location::current()) {
    auto* s = Scope::current();
    std::size_t base = detail::hash_combine(
        detail::hash_source_location(loc), key);
    std::uint32_t counter = s ? s->next_call_counter(base) : 0;
    std::size_t scope_seed = s ? s->widget_id_seed : 0;
    std::size_t widget_id = detail::hash_combine(
        scope_seed, detail::hash_combine(base, counter));

    auto& store = detail::local_store();
    auto cur = detail::local_gen();
    auto* tag = detail::type_tag<T>();

    auto it = store.find(widget_id);
    if (it == store.end()) {
        T* p = new T(std::move(initial));
        detail::LocalEntry e;
        e.last_seen_gen = cur;
        e.storage = p;
        e.deleter = [](void* ptr) { delete static_cast<T*>(ptr); };
        e.type_id = tag;
        // MSVC's modules implementation requires the tuple-like
        // specialisations for std::pair to be reachable from the
        // template's instantiation TU before structured bindings
        // are accepted; explicit `.first` member access dodges the
        // requirement so widget consumers don't transitively need
        // to import <utility>.
        auto result = store.emplace(widget_id, std::move(e));
        return *static_cast<T*>(result.first->second.storage);
    }
    if (it->second.type_id != tag) {
        log::error("phenotype.framework_local",
            "type mismatch at {}:{} — same widget_id reused with a "
            "different T. Add an explicit key or move the call to a "
            "distinct source location.",
            loc.file_name(), loc.line());
        std::abort();
    }
    it->second.last_seen_gen = cur;
    return *static_cast<T*>(it->second.storage);
}

} // namespace phenotype
