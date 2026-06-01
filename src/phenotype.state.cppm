module;
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <array>
#include <functional>
#include <map>
#include <memory>
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

struct KeyCommand {
    unsigned int key = 0;
    int modifiers = 0;
    bool allow_when_input_focused = false;
    unsigned int callback_id = 0xFFFFFFFFu;
    std::string debug_label;
};

struct ActionPerfStats {
    static constexpr std::size_t RECENT_CAPACITY = 120;

    std::uint64_t count = 0;
    std::uint64_t total_ns = 0;
    std::uint64_t last_ns = 0;
    std::uint64_t min_ns = 0;
    std::uint64_t max_ns = 0;
    std::uint64_t over_60fps_budget = 0;
    std::array<std::uint64_t, RECENT_CAPACITY> recent_ns{};
    std::size_t recent_count = 0;
    std::size_t recent_next = 0;
};

struct ActionPerfMonitor {
    ActionPerfStats hover;
    ActionPerfStats scroll;
    ActionPerfStats click;
    ActionPerfStats key;
    ActionPerfStats gesture;
    ActionPerfStats other;
};

enum class FrameTraceAction : unsigned char {
    None,
    Hover,
    Scroll,
    Click,
    Key,
    Gesture,
    Other,
};

struct FrameTraceSample {
    std::uint64_t total_ns = 0;
    std::uint64_t input_ns = 0;
    std::uint64_t update_ns = 0;
    std::uint64_t view_ns = 0;
    std::uint64_t layout_ns = 0;
    std::uint64_t paint_ns = 0;
    std::uint64_t flush_ns = 0;
    FrameTraceAction action = FrameTraceAction::None;
    bool rebuild = true;
};

struct FrameTraceMonitor {
    static constexpr std::size_t RECENT_CAPACITY = 180;

    std::uint64_t count = 0;
    std::uint64_t total_ns = 0;
    std::uint64_t last_ns = 0;
    std::uint64_t min_ns = 0;
    std::uint64_t max_ns = 0;
    std::uint64_t over_60fps_budget = 0;
    FrameTraceSample last{};
    std::array<FrameTraceSample, RECENT_CAPACITY> recent{};
    std::size_t recent_count = 0;
    std::size_t recent_next = 0;
};

struct FrameTimelineMonitor {
    static constexpr std::size_t RECENT_CAPACITY = 240;
    static constexpr std::uint64_t SAMPLE_INTERVAL_NS = 16'666'667ull;

    std::uint64_t tick = 0;
    std::uint64_t last_sample_ns = 0;
    FrameTraceSample last{};
    std::array<FrameTraceSample, RECENT_CAPACITY> recent{};
    std::size_t recent_count = 0;
    std::size_t recent_next = 0;
};

constexpr std::uint64_t k_scene_animation_tick_interval_ns = 16'666'667ull;
constexpr std::uint64_t k_scene_debug_tick_interval_ns = 100'000'000ull;

enum class DebugPanelTab {
    Performance,
    Layout,
    Console,
    Input,
    Protocol,
};

enum class InputModality {
    None,
    Keyboard,
    Pointer,
    Programmatic,
};

enum class FocusVisibilityReason {
    NoFocus,
    KeyboardFocusNavigation,
    PointerInputHidesFocusRing,
    ProgrammaticFocusHidden,
};

enum class SceneRole {
    Main,
    Window,
    Settings,
    Debug,
    Overlay,
    Custom,
};

struct SceneDescriptor {
    std::string id = "main";
    std::string title;
    SceneRole role = SceneRole::Window;
    bool visible = true;
};

struct SceneScheduleSnapshot {
    bool runner_installed = false;
    bool has_active_animations = false;
    bool scrollbar_animation_active = false;
    bool has_active_input_motion = false;
    bool debug_panel_refresh_active = false;
    bool needs_scheduled_tick = false;
    bool debug_panel_only_refresh = false;
    std::uint64_t scheduled_tick_interval_ns = 0;
    std::uint64_t last_scheduled_tick_ns = 0;
    std::uint64_t next_scheduled_tick_ns = 0;
    std::uint64_t scheduled_tick_count = 0;
    bool frame_trace_input_active = false;
    FrameTraceAction frame_trace_input_action = FrameTraceAction::None;
    std::uint64_t frame_trace_count = 0;
    std::uint64_t frame_timeline_tick = 0;
};

struct SceneSnapshot {
    std::string id;
    std::string title;
    SceneRole role = SceneRole::Window;
    bool active = false;
    bool visible = false;
    unsigned int hovered_id = 0xFFFFFFFFu;
    unsigned int focused_id = 0xFFFFFFFFu;
    unsigned int queued_messages = 0;
    unsigned int framework_local_entries = 0;
    std::uint32_t framework_local_generation = 0;
    SceneScheduleSnapshot schedule{};
};

struct SceneHandle {
    std::string id = "main";
};

enum class RenderSurfaceRole {
    MainWindow,
    Window,
    Settings,
    Debug,
    Offscreen,
    Custom,
};

struct RenderSurfaceDescriptor {
    std::string id = "main";
    std::string title;
    std::string scene_id = "main";
    RenderSurfaceRole role = RenderSurfaceRole::Window;
    bool visible = true;
    int logical_width = 0;
    int logical_height = 0;
    int framebuffer_width = 0;
    int framebuffer_height = 0;
    float content_scale = 1.0f;
};

struct RenderSurfaceSnapshot {
    std::string id;
    std::string title;
    std::string scene_id;
    RenderSurfaceRole role = RenderSurfaceRole::Window;
    bool active = false;
    bool visible = false;
    int logical_width = 0;
    int logical_height = 0;
    int framebuffer_width = 0;
    int framebuffer_height = 0;
    float content_scale = 1.0f;
    std::uint64_t frame_sequence = 0;
    std::uint64_t damage_generation = 0;
    std::uint64_t last_paint_hash = 0;
    std::uint64_t paint_flush_count = 0;
    std::uint64_t paint_skip_count = 0;
};

struct RenderSurfaceHandle {
    std::string id = "main";
};

struct ApplicationRuntimeSnapshot {
    std::string active_scene_id = "main";
    SceneRole active_scene_role = SceneRole::Main;
    bool active_scene_visible = true;
    std::string active_render_surface_id = "main";
    std::string active_render_surface_scene_id = "main";
    RenderSurfaceRole active_render_surface_role = RenderSurfaceRole::MainWindow;
    bool active_render_surface_visible = true;
    unsigned int scene_count = 0;
    unsigned int visible_scene_count = 0;
    unsigned int scene_runner_count = 0;
    unsigned int scheduled_scene_count = 0;
    unsigned int render_surface_count = 0;
    unsigned int visible_render_surface_count = 0;
    unsigned int damaged_render_surface_count = 0;
    bool open_url_handler_installed = false;
};

struct AppState {
    Theme theme;
    std::uint64_t theme_generation = 1;
    Arena arena;
    Arena prev_arena;       // previous frame's tree, kept alive for diff
    NodeHandle root = NodeHandle::null();
    NodeHandle prev_root = NodeHandle::null();
    float scroll_x = 0;
    float scroll_y = 0;
    unsigned int hovered_id = 0xFFFFFFFF;
    unsigned int focused_id = 0xFFFFFFFF;
    bool focus_visible = false;
    InputModality focus_input_modality = InputModality::None;
    FocusVisibilityReason focus_visibility_reason =
        FocusVisibilityReason::NoFocus;
    unsigned int pressed_id = 0xFFFFFFFF;
    bool pointer_valid = false;
    float pointer_x = 0.0f;
    float pointer_y = 0.0f;
    unsigned int caret_pos = 0xFFFFFFFFu;
    unsigned int selection_anchor = 0xFFFFFFFFu;
    bool caret_visible = true;
    // Click callbacks indexed by callback_id, registered by Button<Msg>.
    std::vector<std::function<void()>> callbacks;
    std::vector<InteractionRole> callback_roles;
    std::vector<KeyCommand> key_commands;
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
    bool debug_panel_open = false;
    DebugPanelTab debug_panel_tab = DebugPanelTab::Performance;
    unsigned int debug_panel_warmup_frames = 0;
    ActionPerfMonitor action_perf;
    FrameTraceMonitor frame_perf;
    FrameTimelineMonitor frame_timeline;
    bool frame_trace_input_active = false;
    FrameTraceAction frame_trace_input_action = FrameTraceAction::None;
    std::uint64_t frame_trace_input_start_ns = 0;
    bool debug_virtual_pointer_valid = false;
    float debug_virtual_pointer_x = 0.0f;
    float debug_virtual_pointer_y = 0.0f;
    unsigned int debug_virtual_hit_id = 0xFFFFFFFFu;
    std::string debug_console_copy_buffer;
    std::uint64_t debug_console_copy_serial = 0;

    // Set during view by `animate_value` whenever an interpolation
    // hasn't reached its target yet, cleared at the start of every
    // view. Native shells read this after each rebuild and schedule
    // another paint on a ~16ms cadence while it stays true, so a
    // hover fade or accordion slide keeps advancing without needing
    // input events. The runner does not read it — it's purely a
    // "wake me up next frame" signal for the host loop.
    bool has_active_animations = false;
    bool scrollbar_animation_active = false;
    float root_scrollbar_last_scroll_y = -1.0f;
    std::uint64_t root_scrollbar_active_since_ns = 0;
    // Set temporarily while the shell is painting a pointer/scroll-driven input
    // frame. Native material backends use it to prefer motion-safe quality
    // during direct manipulation without turning paint-only scroll into a full
    // view rebuild.
    bool has_active_input_motion = false;

    // Debug panel charts need a steady repaint cadence even when the app is
    // otherwise idle. Keep that separate from view-time animations so native
    // material quality policy does not disable backdrop blur for the panel.
    bool debug_panel_refresh_active = false;

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
    bool          prev_focus_visible = false;
    unsigned int  prev_pressed_id = 0xFFFFFFFFu;
    bool          prev_pointer_valid = false;
    float         prev_pointer_x = 0.0f;
    float         prev_pointer_y = 0.0f;
    // Computed once per frame by the runner before paint; OR'd against
    // each node's paint_callback_mask to decide whether a blit is safe.
    // Non-zero bits correspond to callback_ids whose hover/focus/press state
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
    // global C++ objects. Without this trick, the default AppState destructor
    // runs after _start() and clears the callbacks vector, breaking event
    // handling.
    alignas(AppState) inline unsigned char g_app_storage[sizeof(AppState)];
    inline AppState& default_app_state() {
        static AppState* app = new (&g_app_storage) AppState();
        return *app;
    }

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

    struct SceneRuntime {
        SceneDescriptor descriptor{};
        std::unique_ptr<AppState> owned_app{};
        AppState* app = nullptr;
        std::vector<DispatchedMsg> messages{};
        // Installed by phenotype::run<State, Msg>(...) or the transitional
        // runtime scene-runner API. trigger_rebuild calls this scene-local
        // root runner; the context pointer lets each scene own its eventual
        // view/update runner without falling back to a single global static.
        void (*runner)(void*) = nullptr;
        void* runner_context = nullptr;
        std::shared_ptr<void> runner_context_owner{};
        std::map<std::size_t, LocalEntry> framework_local_store{};
        std::uint32_t framework_local_gen = 1;
        std::shared_ptr<void> material_build_context_owner{};
        void* current_build_scope = nullptr;
        std::uint32_t pending_child_key = LayoutNode::unkeyed_key;
        std::uint64_t last_scheduled_tick_ns = 0;
        std::uint64_t scheduled_tick_count = 0;

        AppState& app_state() {
            if (!app) {
                owned_app = std::make_unique<AppState>();
                app = owned_app.get();
            }
            return *app;
        }
    };

    struct LegacyRunnerContext {
        void (*runner)() = nullptr;
    };

    using OpenUrlHandler = void (*)(char const*, unsigned int);

    struct ApplicationRuntimeServices {
        OpenUrlHandler open_url_handler = nullptr;
    };

    inline ApplicationRuntimeServices& application_runtime_services() {
        static ApplicationRuntimeServices& services =
            *new ApplicationRuntimeServices();
        return services;
    }

    inline void set_application_open_url_handler(OpenUrlHandler handler) noexcept {
        application_runtime_services().open_url_handler = handler;
    }

    inline OpenUrlHandler current_application_open_url_handler() noexcept {
        return application_runtime_services().open_url_handler;
    }

    inline bool application_open_url_handler_installed() noexcept {
        return current_application_open_url_handler() != nullptr;
    }

    inline bool open_application_url(char const* url, unsigned int len) {
        if (auto handler = current_application_open_url_handler()) {
            handler(url, len);
            return true;
        }
        return false;
    }

    inline std::string normalize_scene_id(std::string id) {
        return id.empty() ? std::string{"main"} : std::move(id);
    }

    inline SceneRuntime& default_scene_runtime() {
        static SceneRuntime& scene = *new SceneRuntime();
        if (!scene.app) {
            scene.descriptor = SceneDescriptor{
                .id = "main",
                .title = "Main",
                .role = SceneRole::Main,
                .visible = true,
            };
            scene.app = &default_app_state();
        }
        return scene;
    }

    inline std::vector<std::unique_ptr<SceneRuntime>>& scene_registry() {
        static std::vector<std::unique_ptr<SceneRuntime>>& scenes =
            *new std::vector<std::unique_ptr<SceneRuntime>>();
        return scenes;
    }

    struct RenderSurfaceRuntime;

    struct ActiveRuntimeBinding {
        SceneRuntime* scene = nullptr;
        RenderSurfaceRuntime* render_surface = nullptr;
    };

    inline ActiveRuntimeBinding& active_runtime_binding() {
        static ActiveRuntimeBinding& binding = *new ActiveRuntimeBinding{
            .scene = &default_scene_runtime(),
        };
        if (!binding.scene)
            binding.scene = &default_scene_runtime();
        return binding;
    }

    inline ActiveRuntimeBinding capture_active_runtime_binding() {
        return active_runtime_binding();
    }

    inline SceneRuntime* active_scene_runtime_ptr() {
        auto& binding = active_runtime_binding();
        if (!binding.scene)
            binding.scene = &default_scene_runtime();
        return binding.scene;
    }

    inline SceneRuntime& active_scene_runtime() {
        return *active_scene_runtime_ptr();
    }

    inline AppState& g_app() {
        return active_scene_runtime().app_state();
    }

    inline NodeHandle alloc_node() { return g_app().arena.alloc_node(); }
    inline LayoutNode& node_at(NodeHandle h) { return g_app().arena.must_get(h); }

    inline std::vector<DispatchedMsg>& msg_queue() {
        return active_scene_runtime().messages;
    }

    inline SceneRuntime* find_scene_runtime(std::string_view id) {
        auto normalized = normalize_scene_id(std::string{id});
        auto& main_scene = default_scene_runtime();
        if (main_scene.descriptor.id == normalized)
            return &main_scene;
        for (auto& scene : scene_registry()) {
            if (scene && scene->descriptor.id == normalized)
                return scene.get();
        }
        return nullptr;
    }

    inline SceneRuntime& ensure_scene_runtime(SceneDescriptor descriptor) {
        descriptor.id = normalize_scene_id(std::move(descriptor.id));
        if (auto* existing = find_scene_runtime(descriptor.id)) {
            existing->descriptor = std::move(descriptor);
            return *existing;
        }
        auto scene = std::make_unique<SceneRuntime>();
        scene->descriptor = std::move(descriptor);
        scene->owned_app = std::make_unique<AppState>();
        scene->app = scene->owned_app.get();
        auto* ptr = scene.get();
        scene_registry().push_back(std::move(scene));
        return *ptr;
    }

    inline void bind_scene_runtime(SceneRuntime& scene) {
        active_runtime_binding().scene = &scene;
    }

    inline void restore_active_scene(ActiveRuntimeBinding const& binding) {
        active_runtime_binding().scene =
            binding.scene ? binding.scene : &default_scene_runtime();
    }

    struct ScopedSceneActivation {
        ActiveRuntimeBinding previous{};

        explicit ScopedSceneActivation(SceneRuntime& scene)
            : previous(capture_active_runtime_binding()) {
            bind_scene_runtime(scene);
        }

        ~ScopedSceneActivation() {
            restore_active_scene(previous);
        }
    };

    inline void configure_active_scene(SceneDescriptor descriptor) {
        descriptor.id = normalize_scene_id(std::move(descriptor.id));
        auto& scene = active_scene_runtime();
        scene.descriptor = std::move(descriptor);
    }

    inline bool scene_needs_scheduled_tick(AppState const* app) {
        return app && (app->has_active_animations
            || app->scrollbar_animation_active
            || app->debug_panel_refresh_active);
    }

    inline bool scene_debug_panel_only_refresh(AppState const* app) {
        return app && app->debug_panel_refresh_active
            && !app->has_active_animations
            && !app->scrollbar_animation_active;
    }

    inline std::uint64_t scene_scheduled_tick_interval_ns(AppState const* app) {
        if (!scene_needs_scheduled_tick(app))
            return 0;
        return scene_debug_panel_only_refresh(app)
            ? k_scene_debug_tick_interval_ns
            : k_scene_animation_tick_interval_ns;
    }

    inline SceneScheduleSnapshot scene_schedule_snapshot(
            SceneRuntime const& scene) {
        auto const* app = scene.app;
        if (!app) {
            return SceneScheduleSnapshot{
                .runner_installed = scene.runner != nullptr,
                .last_scheduled_tick_ns = scene.last_scheduled_tick_ns,
                .scheduled_tick_count = scene.scheduled_tick_count,
            };
        }
        auto const needs_tick = scene_needs_scheduled_tick(app);
        auto const debug_only = scene_debug_panel_only_refresh(app);
        auto const interval_ns = scene_scheduled_tick_interval_ns(app);
        auto const next_ns = (needs_tick && scene.last_scheduled_tick_ns > 0)
            ? scene.last_scheduled_tick_ns + interval_ns
            : 0;
        return SceneScheduleSnapshot{
            .runner_installed = scene.runner != nullptr,
            .has_active_animations = app->has_active_animations,
            .scrollbar_animation_active = app->scrollbar_animation_active,
            .has_active_input_motion = app->has_active_input_motion,
            .debug_panel_refresh_active = app->debug_panel_refresh_active,
            .needs_scheduled_tick = needs_tick,
            .debug_panel_only_refresh = debug_only,
            .scheduled_tick_interval_ns = interval_ns,
            .last_scheduled_tick_ns = scene.last_scheduled_tick_ns,
            .next_scheduled_tick_ns = next_ns,
            .scheduled_tick_count = scene.scheduled_tick_count,
            .frame_trace_input_active = app->frame_trace_input_active,
            .frame_trace_input_action = app->frame_trace_input_action,
            .frame_trace_count = app->frame_perf.count,
            .frame_timeline_tick = app->frame_timeline.tick,
        };
    }

    inline SceneSnapshot scene_snapshot(SceneRuntime const& scene) {
        auto const* app = scene.app;
        return SceneSnapshot{
            .id = scene.descriptor.id,
            .title = scene.descriptor.title,
            .role = scene.descriptor.role,
            .active = &scene == active_scene_runtime_ptr(),
            .visible = scene.descriptor.visible,
            .hovered_id = app ? app->hovered_id : 0xFFFFFFFFu,
            .focused_id = app ? app->focused_id : 0xFFFFFFFFu,
            .queued_messages = static_cast<unsigned int>(scene.messages.size()),
            .framework_local_entries =
                static_cast<unsigned int>(scene.framework_local_store.size()),
            .framework_local_generation = scene.framework_local_gen,
            .schedule = scene_schedule_snapshot(scene),
        };
    }

    inline std::vector<SceneSnapshot> scene_snapshots() {
        std::vector<SceneSnapshot> out;
        out.push_back(scene_snapshot(default_scene_runtime()));
        for (auto const& scene : scene_registry()) {
            if (scene)
                out.push_back(scene_snapshot(*scene));
        }
        return out;
    }

    struct RenderSurfaceRuntime {
        RenderSurfaceDescriptor descriptor{};
        SceneRuntime* scene = nullptr;
        std::uint64_t frame_sequence = 0;
        std::uint64_t damage_generation = 0;
        std::uint64_t last_paint_hash = 0;
        std::uint64_t paint_flush_count = 0;
        std::uint64_t paint_skip_count = 0;
    };

    inline std::string normalize_render_surface_id(std::string id) {
        return id.empty() ? std::string{"main"} : std::move(id);
    }

    inline SceneRole scene_role_for_render_surface(RenderSurfaceRole role) {
        switch (role) {
        case RenderSurfaceRole::MainWindow:
            return SceneRole::Main;
        case RenderSurfaceRole::Settings:
            return SceneRole::Settings;
        case RenderSurfaceRole::Debug:
            return SceneRole::Debug;
        case RenderSurfaceRole::Offscreen:
            return SceneRole::Overlay;
        case RenderSurfaceRole::Custom:
            return SceneRole::Custom;
        case RenderSurfaceRole::Window:
        default:
            return SceneRole::Window;
        }
    }

    inline std::string render_surface_scene_id(
        RenderSurfaceDescriptor const& descriptor) {
        if (!descriptor.scene_id.empty())
            return normalize_scene_id(descriptor.scene_id);
        if (descriptor.id == "main")
            return std::string{"main"};
        return active_scene_runtime().descriptor.id;
    }

    inline SceneRuntime& scene_for_render_surface(
        RenderSurfaceDescriptor const& descriptor) {
        auto scene_id = render_surface_scene_id(descriptor);
        if (auto* existing = find_scene_runtime(scene_id)) {
            existing->descriptor.visible = descriptor.visible;
            return *existing;
        }
        auto title = descriptor.title.empty() ? scene_id : descriptor.title;
        return ensure_scene_runtime(SceneDescriptor{
            .id = std::move(scene_id),
            .title = std::move(title),
            .role = scene_role_for_render_surface(descriptor.role),
            .visible = descriptor.visible,
        });
    }

    inline RenderSurfaceRuntime& default_render_surface_runtime() {
        static RenderSurfaceRuntime& surface = *new RenderSurfaceRuntime();
        if (!surface.scene) {
            surface.descriptor = RenderSurfaceDescriptor{
                .id = "main",
                .title = "Main",
                .scene_id = "main",
                .role = RenderSurfaceRole::MainWindow,
                .visible = true,
            };
            surface.scene = &default_scene_runtime();
        }
        return surface;
    }

    inline std::vector<std::unique_ptr<RenderSurfaceRuntime>>&
    render_surface_registry() {
        static std::vector<std::unique_ptr<RenderSurfaceRuntime>>& surfaces =
            *new std::vector<std::unique_ptr<RenderSurfaceRuntime>>();
        return surfaces;
    }

    inline RenderSurfaceRuntime* active_render_surface_runtime_ptr() {
        auto& binding = active_runtime_binding();
        if (!binding.render_surface)
            binding.render_surface = &default_render_surface_runtime();
        return binding.render_surface;
    }

    inline RenderSurfaceRuntime& active_render_surface_runtime() {
        return *active_render_surface_runtime_ptr();
    }

    inline RenderSurfaceRuntime* find_render_surface_runtime(
        std::string_view id) {
        auto normalized = normalize_render_surface_id(std::string{id});
        auto& main_surface = default_render_surface_runtime();
        if (main_surface.descriptor.id == normalized)
            return &main_surface;
        for (auto& surface : render_surface_registry()) {
            if (surface && surface->descriptor.id == normalized)
                return surface.get();
        }
        return nullptr;
    }

    inline RenderSurfaceDescriptor normalize_render_surface_descriptor(
        RenderSurfaceDescriptor descriptor) {
        descriptor.id = normalize_render_surface_id(std::move(descriptor.id));
        descriptor.scene_id = render_surface_scene_id(descriptor);
        if (descriptor.content_scale <= 0.0f)
            descriptor.content_scale = 1.0f;
        return descriptor;
    }

    inline RenderSurfaceRuntime& ensure_render_surface_runtime(
        RenderSurfaceDescriptor descriptor) {
        descriptor = normalize_render_surface_descriptor(std::move(descriptor));
        auto& scene = scene_for_render_surface(descriptor);
        if (auto* existing = find_render_surface_runtime(descriptor.id)) {
            existing->descriptor = std::move(descriptor);
            existing->scene = &scene;
            return *existing;
        }
        auto surface = std::make_unique<RenderSurfaceRuntime>();
        surface->descriptor = std::move(descriptor);
        surface->scene = &scene;
        auto* ptr = surface.get();
        render_surface_registry().push_back(std::move(surface));
        return *ptr;
    }

    inline void bind_render_surface_runtime(RenderSurfaceRuntime& surface) {
        auto& binding = active_runtime_binding();
        binding.render_surface = &surface;
        if (!surface.scene)
            surface.scene = &scene_for_render_surface(surface.descriptor);
        binding.scene = surface.scene;
    }

    inline void restore_active_runtime_binding(
        ActiveRuntimeBinding const& binding) {
        auto& active = active_runtime_binding();
        active.scene = binding.scene ? binding.scene : &default_scene_runtime();
        active.render_surface = binding.render_surface
            ? binding.render_surface
            : &default_render_surface_runtime();
    }

    struct ScopedRenderSurfaceActivation {
        ActiveRuntimeBinding previous{};

        explicit ScopedRenderSurfaceActivation(RenderSurfaceRuntime& surface)
            : previous(capture_active_runtime_binding()) {
            bind_render_surface_runtime(surface);
        }

        ~ScopedRenderSurfaceActivation() {
            restore_active_runtime_binding(previous);
        }
    };

    inline void configure_active_render_surface(
        RenderSurfaceDescriptor descriptor) {
        auto& surface = active_render_surface_runtime();
        if (descriptor.id.empty())
            descriptor.id = surface.descriptor.id;
        if (descriptor.scene_id.empty()) {
            descriptor.scene_id = surface.scene
                ? surface.scene->descriptor.id
                : active_scene_runtime().descriptor.id;
        }
        descriptor = normalize_render_surface_descriptor(std::move(descriptor));
        surface.descriptor = std::move(descriptor);
        surface.scene = &scene_for_render_surface(surface.descriptor);
        bind_render_surface_runtime(surface);
    }

    inline void mark_active_render_surface_damaged() {
        ++active_render_surface_runtime().damage_generation;
    }

    inline std::uint64_t active_render_surface_paint_hash() {
        return active_render_surface_runtime().last_paint_hash;
    }

    inline void set_active_render_surface_paint_hash(std::uint64_t hash) {
        active_render_surface_runtime().last_paint_hash = hash;
    }

    inline void invalidate_active_render_surface_paint_cache() {
        set_active_render_surface_paint_hash(0);
        // Keep the legacy AppState mirror in lockstep while render surfaces own
        // the real paint cache key.
        g_app().last_paint_hash = 0;
    }

    inline void note_active_render_surface_frame() {
        ++active_render_surface_runtime().frame_sequence;
    }

    inline RenderSurfaceSnapshot render_surface_snapshot(
        RenderSurfaceRuntime const& surface) {
        return RenderSurfaceSnapshot{
            .id = surface.descriptor.id,
            .title = surface.descriptor.title,
            .scene_id = surface.descriptor.scene_id,
            .role = surface.descriptor.role,
            .active = &surface == active_render_surface_runtime_ptr(),
            .visible = surface.descriptor.visible,
            .logical_width = surface.descriptor.logical_width,
            .logical_height = surface.descriptor.logical_height,
            .framebuffer_width = surface.descriptor.framebuffer_width,
            .framebuffer_height = surface.descriptor.framebuffer_height,
            .content_scale = surface.descriptor.content_scale,
            .frame_sequence = surface.frame_sequence,
            .damage_generation = surface.damage_generation,
            .last_paint_hash = surface.last_paint_hash,
            .paint_flush_count = surface.paint_flush_count,
            .paint_skip_count = surface.paint_skip_count,
        };
    }

    inline std::vector<RenderSurfaceSnapshot> render_surface_snapshots() {
        std::vector<RenderSurfaceSnapshot> out;
        out.push_back(render_surface_snapshot(default_render_surface_runtime()));
        for (auto const& surface : render_surface_registry()) {
            if (surface)
                out.push_back(render_surface_snapshot(*surface));
        }
        return out;
    }

    inline ApplicationRuntimeSnapshot application_runtime_snapshot() {
        auto scene_items = scene_snapshots();
        auto surface_items = render_surface_snapshots();
        auto const active_scene = scene_snapshot(active_scene_runtime());
        auto const active_surface =
            render_surface_snapshot(active_render_surface_runtime());

        ApplicationRuntimeSnapshot out{};
        out.active_scene_id = active_scene.id;
        out.active_scene_role = active_scene.role;
        out.active_scene_visible = active_scene.visible;
        out.active_render_surface_id = active_surface.id;
        out.active_render_surface_scene_id = active_surface.scene_id;
        out.active_render_surface_role = active_surface.role;
        out.active_render_surface_visible = active_surface.visible;
        out.scene_count = static_cast<unsigned int>(scene_items.size());
        out.render_surface_count =
            static_cast<unsigned int>(surface_items.size());
        out.open_url_handler_installed =
            application_open_url_handler_installed();

        for (auto const& scene : scene_items) {
            if (scene.visible)
                ++out.visible_scene_count;
            if (scene.schedule.runner_installed)
                ++out.scene_runner_count;
            if (scene.schedule.needs_scheduled_tick)
                ++out.scheduled_scene_count;
        }
        for (auto const& surface : surface_items) {
            if (surface.visible)
                ++out.visible_render_surface_count;
            if (surface.damage_generation > 0)
                ++out.damaged_render_surface_count;
        }
        return out;
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

    inline SceneScheduleSnapshot active_scene_schedule_snapshot() {
        return scene_schedule_snapshot(active_scene_runtime());
    }

    inline bool active_scene_has_view_animations() {
        return g_app().has_active_animations;
    }

    inline bool active_scene_has_scrollbar_animation() {
        return g_app().scrollbar_animation_active;
    }

    inline bool active_scene_needs_debug_panel_refresh() {
        return g_app().debug_panel_refresh_active;
    }

    inline bool active_scene_needs_scheduled_tick() {
        return scene_needs_scheduled_tick(&g_app());
    }

    inline bool active_scene_debug_panel_only_refresh() {
        return scene_debug_panel_only_refresh(&g_app());
    }

    inline std::uint64_t active_scene_scheduled_tick_interval_ns() {
        return scene_scheduled_tick_interval_ns(&g_app());
    }

    inline bool active_scene_scheduled_tick_due(std::uint64_t now_ns) {
        auto const interval_ns = active_scene_scheduled_tick_interval_ns();
        if (interval_ns == 0)
            return false;
        auto const last_ns = active_scene_runtime().last_scheduled_tick_ns;
        return last_ns == 0 || now_ns < last_ns || now_ns - last_ns >= interval_ns;
    }

    inline void note_active_scene_scheduled_tick(std::uint64_t now_ns) {
        auto& scene = active_scene_runtime();
        scene.last_scheduled_tick_ns = now_ns;
        ++scene.scheduled_tick_count;
    }

    inline void reset_active_scene_schedule_clock() {
        auto& scene = active_scene_runtime();
        scene.last_scheduled_tick_ns = 0;
        scene.scheduled_tick_count = 0;
    }

    inline void install_app_runner(void (*runner)(void*),
                                   void* context,
                                   std::shared_ptr<void> context_owner) {
        auto& scene = active_scene_runtime();
        scene.runner = runner;
        scene.runner_context = context;
        scene.runner_context_owner = runner ? std::move(context_owner) : nullptr;
    }

    inline void install_app_runner(void (*runner)(void*), void* context) {
        install_app_runner(runner, context, nullptr);
    }

    inline void install_app_runner(void (*runner)()) {
        auto& scene = active_scene_runtime();
        if (!runner) {
            scene.runner = nullptr;
            scene.runner_context = nullptr;
            scene.runner_context_owner = nullptr;
            return;
        }

        auto context = std::make_shared<LegacyRunnerContext>(
            LegacyRunnerContext{.runner = runner});
        scene.runner = [](void* raw) {
            auto* context = static_cast<LegacyRunnerContext*>(raw);
            if (context && context->runner)
                context->runner();
        };
        scene.runner_context = context.get();
        scene.runner_context_owner = std::move(context);
    }

    inline void trigger_rebuild() {
        auto& scene = active_scene_runtime();
        if (scene.runner)
            scene.runner(scene.runner_context);
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
    // The store is scene-local: settings windows, debug panels, and
    // document windows must not share scroll offsets, accordion state,
    // or animation progress by accident. Lifetime: entries are tagged
    // with `local_gen()` at every access; `prune_local_store()` (called
    // by the runner after each `view`) drops entries whose tag is stale,
    // so leaving a widget out of the tree implicitly destroys its local
    // state. `bump_local_gen()` monotonically advances the tag at frame
    // start.
    //
    // Storage uses std::map keyed by widget_id rather than
    // std::unordered_map for the same reason `measure_cache` does —
    // libc++'s unordered_map's __hash_memory symbol is missing on
    // wasi-sdk's clang-22 link line. log(n) over a few dozen widget
    // states is dwarfed by the host-call savings the store enables.

    inline std::map<std::size_t, LocalEntry>& local_store() {
        return active_scene_runtime().framework_local_store;
    }

    inline std::uint32_t& local_gen() {
        return active_scene_runtime().framework_local_gen;
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

    inline FocusedInputSnapshot focused_input_snapshot() {
        FocusedInputSnapshot snapshot{};
        if (g_app().focused_id == 0xFFFFFFFFu)
            return snapshot;

        NodeHandle input_h = NodeHandle::null();
        for (auto const& [id, node_h] : g_app().input_nodes) {
            if (id == g_app().focused_id) {
                input_h = node_h;
                break;
            }
        }
        if (!input_h.valid())
            return snapshot;

        auto* input = g_app().arena.get(input_h);
        if (!input || !input->is_input)
            return snapshot;

        std::string current_value;
        for (auto const& [id, handler] : g_app().input_handlers) {
            if (id == g_app().focused_id) {
                current_value = handler.current;
                break;
            }
        }

        float ax = 0.0f;
        float ay = 0.0f;
        auto cursor = input_h;
        while (cursor.valid()) {
            auto* node = g_app().arena.get(cursor);
            if (!node) break;
            ax += node->x;
            ay += node->y;
            cursor = node->parent;
        }

        snapshot.valid = true;
        snapshot.callback_id = g_app().focused_id;
        snapshot.role = input->interaction_role;
        snapshot.x = ax;
        snapshot.y = ay;
        snapshot.width = input->width;
        snapshot.height = input->height;
        snapshot.font_size = input->font_size;
        snapshot.line_height = input->font_size * g_app().theme.line_height_ratio;
        snapshot.mono = input->mono;
        for (int i = 0; i < 4; ++i)
            snapshot.padding[i] = input->style.padding[i];
        snapshot.background = input->background;
        snapshot.foreground = g_app().theme.foreground;
        snapshot.placeholder_color = g_app().theme.muted;
        snapshot.accent = g_app().theme.accent;
        snapshot.caret_pos = g_app().caret_pos;
        snapshot.selection_anchor = g_app().selection_anchor;
        snapshot.caret_visible = g_app().caret_visible;
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
// `widget_id_seed` carries the parent-aware container seed used by
// framework_local<T>(). Structural containers derive it from their
// parent seed plus their own node identity, so the same widget helper
// called inside two sibling containers lands in two distinct slots.
// Manual scopes should use the same derived seed helper that
// open_container uses; explicit framework_local keys are still the
// right tool for data-driven list identity when item order changes.
struct Scope {
    NodeHandle node;
    std::size_t widget_id_seed = 0;
    std::vector<std::pair<std::size_t, std::uint32_t>> call_counters;

    Scope(NodeHandle n) : node(n) {}
    Scope(NodeHandle n, std::size_t seed)
        : node(n), widget_id_seed(seed) {}

    static Scope* current() {
        return static_cast<Scope*>(
            detail::active_scene_runtime().current_build_scope);
    }

    static void set_current(Scope* s) {
        detail::active_scene_runtime().current_build_scope = s;
    }

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
// Container scopes derive a non-zero parent-aware seed through
// open_container, so sibling containers can call the same helper
// source_location without sharing storage. Pass an explicit key
// when the identity comes from data rather than tree position
// (e.g. `framework_local<int>(0, item_id)` inside a keyed list).
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

namespace runtime {

inline SceneHandle main_scene() {
    return SceneHandle{.id = "main"};
}

inline SceneHandle active_scene_handle() {
    return SceneHandle{.id = detail::active_scene_runtime().descriptor.id};
}

inline ApplicationRuntimeSnapshot application_runtime() {
    return detail::application_runtime_snapshot();
}

inline SceneHandle ensure_scene(SceneDescriptor descriptor) {
    auto& scene = detail::ensure_scene_runtime(std::move(descriptor));
    return SceneHandle{.id = scene.descriptor.id};
}

inline bool scene_exists(std::string_view id) {
    return detail::find_scene_runtime(id) != nullptr;
}

inline bool scene_exists(SceneHandle const& handle) {
    return scene_exists(handle.id);
}

inline SceneSnapshot scene(SceneHandle const& handle) {
    auto* scene = detail::find_scene_runtime(handle.id);
    if (!scene) {
        log::error("phenotype.runtime",
            "scene '{}' does not exist; call runtime::ensure_scene first",
            handle.id);
        std::abort();
    }
    return detail::scene_snapshot(*scene);
}

inline SceneSnapshot scene(std::string_view id) {
    return scene(SceneHandle{.id = std::string{id}});
}

class SceneActivation {
    detail::ActiveRuntimeBinding previous_{};
    bool active_ = false;

    void activate(std::string_view id) {
        auto* scene = detail::find_scene_runtime(id);
        if (!scene) {
            log::error("phenotype.runtime",
                "scene '{}' does not exist; call runtime::ensure_scene first",
                id);
            std::abort();
        }
        previous_ = detail::capture_active_runtime_binding();
        detail::bind_scene_runtime(*scene);
        active_ = true;
    }

    void reset() {
        if (!active_)
            return;
        detail::restore_active_scene(previous_);
        active_ = false;
    }

public:
    explicit SceneActivation(SceneHandle const& handle) {
        activate(handle.id);
    }

    explicit SceneActivation(std::string_view id) {
        activate(id);
    }

    SceneActivation(SceneActivation const&) = delete;
    SceneActivation& operator=(SceneActivation const&) = delete;

    SceneActivation(SceneActivation&& other) noexcept
        : previous_(other.previous_),
          active_(other.active_) {
        other.previous_ = {};
        other.active_ = false;
    }

    SceneActivation& operator=(SceneActivation&&) = delete;

    ~SceneActivation() {
        reset();
    }
};

inline SceneActivation activate_scene(SceneHandle const& handle) {
    return SceneActivation{handle};
}

inline SceneActivation activate_scene(std::string_view id) {
    return SceneActivation{id};
}

inline SceneSnapshot active_scene() {
    return detail::scene_snapshot(detail::active_scene_runtime());
}

inline SceneScheduleSnapshot active_scene_schedule() {
    return detail::active_scene_schedule_snapshot();
}

inline SceneScheduleSnapshot scene_schedule(SceneHandle const& handle) {
    SceneActivation activate{handle};
    return active_scene_schedule();
}

inline std::vector<SceneSnapshot> scenes() {
    return detail::scene_snapshots();
}

inline void configure_active_scene(SceneDescriptor descriptor) {
    detail::configure_active_scene(std::move(descriptor));
}

inline void set_active_scene_visible(bool visible) {
    detail::active_scene_runtime().descriptor.visible = visible;
}

inline void set_scene_visible(SceneHandle const& handle, bool visible) {
    SceneActivation activate{handle};
    set_active_scene_visible(visible);
}

inline void set_scene_visible(std::string_view id, bool visible) {
    set_scene_visible(SceneHandle{.id = std::string{id}}, visible);
}

using SceneRunnerCallback = void (*)(void*);

inline void install_active_scene_runner(SceneRunnerCallback runner,
                                        void* context) {
    detail::install_app_runner(runner, context);
}

inline void install_scene_runner(SceneHandle const& handle,
                                 SceneRunnerCallback runner,
                                 void* context) {
    SceneActivation activate{handle};
    install_active_scene_runner(runner, context);
}

inline void clear_scene_runner(SceneHandle const& handle) {
    install_scene_runner(handle, nullptr, nullptr);
}

inline void clear_active_scene_runner() {
    install_active_scene_runner(nullptr, nullptr);
}

inline bool active_scene_has_runner() {
    return detail::active_scene_runtime().runner != nullptr;
}

inline bool scene_has_runner(SceneHandle const& handle) {
    SceneActivation activate{handle};
    return active_scene_has_runner();
}

inline void trigger_active_scene_rebuild() {
    detail::trigger_rebuild();
}

inline void trigger_scene_rebuild(SceneHandle const& handle) {
    SceneActivation activate{handle};
    trigger_active_scene_rebuild();
}

inline void clear_messages() {
    detail::msg_queue().clear();
}

inline void clear_scene_messages(SceneHandle const& handle) {
    SceneActivation activate{handle};
    clear_messages();
}

template<typename Msg>
inline void post(Msg msg) {
    detail::post<Msg>(std::move(msg));
}

template<typename Msg>
inline void post_to_scene(SceneHandle const& handle, Msg msg) {
    SceneActivation activate{handle};
    post<Msg>(std::move(msg));
}

template<typename Msg>
inline std::vector<Msg> drain() {
    return detail::drain<Msg>();
}

template<typename Msg>
inline std::vector<Msg> drain_scene(SceneHandle const& handle) {
    SceneActivation activate{handle};
    return drain<Msg>();
}

inline RenderSurfaceSnapshot active_render_surface() {
    return detail::render_surface_snapshot(
        detail::active_render_surface_runtime());
}

inline RenderSurfaceHandle main_render_surface() {
    return RenderSurfaceHandle{.id = "main"};
}

inline RenderSurfaceHandle active_render_surface_handle() {
    return RenderSurfaceHandle{
        .id = detail::active_render_surface_runtime().descriptor.id};
}

inline RenderSurfaceHandle ensure_render_surface(
        RenderSurfaceDescriptor descriptor) {
    auto& surface =
        detail::ensure_render_surface_runtime(std::move(descriptor));
    return RenderSurfaceHandle{.id = surface.descriptor.id};
}

inline bool render_surface_exists(std::string_view id) {
    return detail::find_render_surface_runtime(id) != nullptr;
}

inline bool render_surface_exists(RenderSurfaceHandle const& handle) {
    return render_surface_exists(handle.id);
}

inline RenderSurfaceSnapshot render_surface(
        RenderSurfaceHandle const& handle) {
    auto* surface = detail::find_render_surface_runtime(handle.id);
    if (!surface) {
        log::error("phenotype.runtime",
            "render surface '{}' does not exist; call "
            "runtime::ensure_render_surface first",
            handle.id);
        std::abort();
    }
    return detail::render_surface_snapshot(*surface);
}

inline RenderSurfaceSnapshot render_surface(std::string_view id) {
    return render_surface(RenderSurfaceHandle{.id = std::string{id}});
}

class RenderSurfaceActivation {
    detail::ActiveRuntimeBinding previous_{};
    bool active_ = false;

    void activate(std::string_view id) {
        auto* surface = detail::find_render_surface_runtime(id);
        if (!surface) {
            log::error("phenotype.runtime",
                "render surface '{}' does not exist; call "
                "runtime::ensure_render_surface first",
                id);
            std::abort();
        }
        previous_ = detail::capture_active_runtime_binding();
        detail::bind_render_surface_runtime(*surface);
        active_ = true;
    }

    void reset() {
        if (!active_)
            return;
        detail::restore_active_runtime_binding(previous_);
        active_ = false;
    }

public:
    explicit RenderSurfaceActivation(RenderSurfaceHandle const& handle) {
        activate(handle.id);
    }

    explicit RenderSurfaceActivation(std::string_view id) {
        activate(id);
    }

    RenderSurfaceActivation(RenderSurfaceActivation const&) = delete;
    RenderSurfaceActivation& operator=(RenderSurfaceActivation const&) = delete;

    RenderSurfaceActivation(RenderSurfaceActivation&& other) noexcept
        : previous_(other.previous_),
          active_(other.active_) {
        other.previous_ = {};
        other.active_ = false;
    }

    RenderSurfaceActivation& operator=(RenderSurfaceActivation&&) = delete;

    ~RenderSurfaceActivation() {
        reset();
    }
};

inline RenderSurfaceActivation activate_render_surface(
        RenderSurfaceHandle const& handle) {
    return RenderSurfaceActivation{handle};
}

inline RenderSurfaceActivation activate_render_surface(std::string_view id) {
    return RenderSurfaceActivation{id};
}

inline std::vector<RenderSurfaceSnapshot> render_surfaces() {
    return detail::render_surface_snapshots();
}

inline void configure_active_render_surface(RenderSurfaceDescriptor descriptor) {
    detail::configure_active_render_surface(std::move(descriptor));
}

inline void set_active_render_surface_visible(bool visible) {
    auto& surface = detail::active_render_surface_runtime();
    surface.descriptor.visible = visible;
    if (surface.scene)
        surface.scene->descriptor.visible = visible;
}

inline void set_render_surface_visible(RenderSurfaceHandle const& handle,
                                       bool visible) {
    RenderSurfaceActivation activate{handle};
    set_active_render_surface_visible(visible);
}

inline void set_render_surface_visible(std::string_view id, bool visible) {
    set_render_surface_visible(RenderSurfaceHandle{.id = std::string{id}},
                               visible);
}

inline void mark_active_render_surface_damaged() {
    detail::mark_active_render_surface_damaged();
}

inline void note_active_render_surface_frame() {
    detail::note_active_render_surface_frame();
}

inline std::uint64_t active_render_surface_paint_hash() {
    return detail::active_render_surface_paint_hash();
}

inline void set_active_render_surface_paint_hash(std::uint64_t hash) {
    detail::set_active_render_surface_paint_hash(hash);
}

inline void invalidate_active_render_surface_paint_cache() {
    detail::invalidate_active_render_surface_paint_cache();
}

inline void note_active_render_surface_paint_flush() {
    ++detail::active_render_surface_runtime().paint_flush_count;
}

inline void note_active_render_surface_paint_skip() {
    ++detail::active_render_surface_runtime().paint_skip_count;
}

} // namespace runtime

} // namespace phenotype
