module;
#if !defined(__wasi__)
#include <cmath>
#include <chrono>
#include <concepts>
#include <cstdio>
#include <functional>
#include <optional>
#include <utility>
#include <vector>
#endif

export module phenotype.native.shell;

#if !defined(__wasi__)
import phenotype;
import phenotype.native.platform;

export namespace phenotype::native {

// Neutral input enums. Values intentionally keep the original desktop numeric
// assignments for source compatibility, while platform drivers translate
// AppKit, Win32, Android, or future toolkit constants into this small set.
enum class KeyAction : int {
    Release = 0,
    Press   = 1,
    Repeat  = 2,
};

enum class Modifier : int {
    Shift   = 0x0001,
    Control = 0x0002,
    Alt     = 0x0004,
    Super   = 0x0008,
};

enum class MouseButton : int {
    Left   = 0,
    Right  = 1,
    Middle = 2,
};

// `GestureEvent` and `GestureKind` are exported from the core
// `phenotype` module — see phenotype.types.cppm — so apps can take
// them as parameters without depending on the shell.

// Only the keys the shell actually switches on. `Other` is used for keys
// the shell does not care about, so drivers can safely funnel anything
// unrecognised there without creating new enumerators.
enum class Key : int {
    Tab       = 258,
    Backspace = 259,
    Delete    = 261,
    Enter     = 257,
    KpEnter   = 335,
    Space     = 32,
    Left      = 263,
    Right     = 262,
    Up        = 265,
    Down      = 264,
    PageUp    = 266,
    PageDown  = 267,
    Home      = 268,
    End       = 269,
    Escape    = 256,
    A         = 65,
    D         = 68,
    F         = 70,
    N         = 78,
    F12       = 301,
    Other     = -1,
};

inline Key key_from_legacy_code(int key) {
    switch (key) {
        case static_cast<int>(Key::Tab): return Key::Tab;
        case static_cast<int>(Key::Backspace): return Key::Backspace;
        case static_cast<int>(Key::Delete): return Key::Delete;
        case static_cast<int>(Key::Enter): return Key::Enter;
        case static_cast<int>(Key::KpEnter): return Key::KpEnter;
        case static_cast<int>(Key::Space): return Key::Space;
        case static_cast<int>(Key::Left): return Key::Left;
        case static_cast<int>(Key::Right): return Key::Right;
        case static_cast<int>(Key::Up): return Key::Up;
        case static_cast<int>(Key::Down): return Key::Down;
        case static_cast<int>(Key::PageUp): return Key::PageUp;
        case static_cast<int>(Key::PageDown): return Key::PageDown;
        case static_cast<int>(Key::Home): return Key::Home;
        case static_cast<int>(Key::End): return Key::End;
        case static_cast<int>(Key::Escape): return Key::Escape;
        case static_cast<int>(Key::A): return Key::A;
        case static_cast<int>(Key::D): return Key::D;
        case static_cast<int>(Key::F): return Key::F;
        case static_cast<int>(Key::N): return Key::N;
        case static_cast<int>(Key::F12): return Key::F12;
        default: return Key::Other;
    }
}

inline KeyAction key_action_from_legacy_code(int action) {
    switch (action) {
        case static_cast<int>(KeyAction::Release): return KeyAction::Release;
        case static_cast<int>(KeyAction::Repeat): return KeyAction::Repeat;
        case static_cast<int>(KeyAction::Press):
        default:
            return KeyAction::Press;
    }
}

inline MouseButton mouse_button_from_legacy_code(int button) {
    switch (button) {
        case static_cast<int>(MouseButton::Right): return MouseButton::Right;
        case static_cast<int>(MouseButton::Middle): return MouseButton::Middle;
        case static_cast<int>(MouseButton::Left):
        default:
            return MouseButton::Left;
    }
}

struct native_host {
    // Opaque native surface handle. Desktop shells pass a
    // NativeSurfaceDescriptor* carrying the OS window/view and viewport
    // metadata; Android keeps passing its ANativeWindow* directly.
    native_surface_handle window = nullptr;
    platform_api const* platform = nullptr;

    // Cached framebuffer size, populated by the driver layer. Used by the host_platform concept so
    // shell dispatch code never has to query the windowing toolkit
    // directly.
    int cached_width_px = 800;
    int cached_height_px = 600;

    // Cached HiDPI scale (framebuffer_pixels / window_points). Driver
    // updates this at startup and on every viewport/content-scale callback so renderers can
    // read it without polling the windowing toolkit each frame.
    float cached_content_scale = 1.0f;

    // Type-erased viewport-change hook installed by the templated
    // `run_app` overload. Called by the driver after cached_*_px /
    // cached_content_scale are updated; its body is a captured lambda
    // that posts the user-supplied Msg into the global queue. Stays
    // empty when the app uses the 5-arg run_app entry point.
    std::function<void(int /*w*/, int /*h*/, float /*scale*/)>
        on_viewport_changed;

    // Optional driver hook for hardware-cursor updates. Pointer-based desktop drivers set
    // this; touch-only drivers leave it null.
    void (*set_hover_cursor)(bool pointing) = nullptr;

    float measure_text(float font_size, ::phenotype::FontSpec font,
                       char const* text, unsigned int len) const {
        if (!platform || !platform->text.measure) return 0.0f;
        unsigned int flags =
            (font.mono ? 1u : 0u)
            | (font.weight == ::phenotype::FontWeight::Bold  ? 2u : 0u)
            | (font.style  == ::phenotype::FontStyle::Italic ? 4u : 0u);
        return platform->text.measure(
            font_size, flags,
            font.family.data(),
            static_cast<unsigned int>(font.family.size()),
            text, len);
    }

    // Vertical metrics for the resolved face at `font_size`. Backends
    // without metric support return a zero `FontMetrics{}` — caller
    // falls back to a `font_size`-based heuristic.
    ::phenotype::FontMetrics font_metrics(
            float font_size, ::phenotype::FontSpec font) const {
        ::phenotype::FontMetrics out{};
        if (!platform || !platform->text.metrics) return out;
        unsigned int flags =
            (font.mono ? 1u : 0u)
            | (font.weight == ::phenotype::FontWeight::Bold  ? 2u : 0u)
            | (font.style  == ::phenotype::FontStyle::Italic ? 4u : 0u);
        platform->text.metrics(
            font_size, flags,
            font.family.data(),
            static_cast<unsigned int>(font.family.size()),
            &out.ascent, &out.descent, &out.leading,
            &out.cap_height);
        return out;
    }

    // Growable command stream. Initial capacity matches the legacy
    // fixed-size buffer (65 536 bytes) so single-canvas apps stay on
    // the same allocation profile they had before. `reserve()`
    // doubles past that, capped at MAX_BUF_SIZE so a runaway emit_*
    // still trips the diagnostic overflow path instead of exhausting
    // RSS on a dense scene.
    //
    // Cap raised from 4 MiB → 64 MiB after `cad++`'s VIEWPORT-driven
    // sheet renderer started multiplying model entities by the
    // viewport count (each paper-space VIEWPORT walks the full model
    // BLOCK_HEADER under its own affine). colorwh.dwg's True Color
    // sheet has 3 content viewports × 36 095 HATCHes ≈ 6.5 MiB of
    // cmd bytes before cap; under 4 MiB the buffer hit cap mid-paint
    // and the auto-flush dropped the surrounding UI (heading,
    // sidebar) from the visible frame. Modern desktops handle 64 MiB
    // comfortably and dense CAD frames now stay in one contiguous
    // buffer end-to-end.
    static constexpr unsigned int INIT_BUF_SIZE = 65536;
    static constexpr unsigned int MAX_BUF_SIZE  = 64 * 1024 * 1024;
    std::vector<unsigned char> buffer_ = std::vector<unsigned char>(INIT_BUF_SIZE);
    unsigned int len_ = 0;

    unsigned char* buf() { return buffer_.data(); }
    unsigned int& buf_len() { return len_; }
    unsigned int buf_size() {
        return static_cast<unsigned int>(buffer_.size());
    }
    void flush() {
        if (len_ == 0) return;
        if (platform && platform->renderer.flush)
            platform->renderer.flush(buffer_.data(), len_);
        len_ = 0;
    }
    [[nodiscard]] bool reserve(unsigned int needed) {
        if (needed > MAX_BUF_SIZE) return false;
        if (needed <= buffer_.size()) return true;
        std::size_t new_size = buffer_.size();
        while (new_size < needed) new_size *= 2;
        if (new_size > MAX_BUF_SIZE) new_size = MAX_BUF_SIZE;
        buffer_.resize(new_size);
        return needed <= buffer_.size();
    }

    float canvas_width() const {
        return (cached_width_px > 0) ? static_cast<float>(cached_width_px) : 800.0f;
    }

    float canvas_height() const {
        return (cached_height_px > 0) ? static_cast<float>(cached_height_px) : 600.0f;
    }

    void open_url(char const* url, unsigned int len) {
        if (platform && platform->open_url)
            platform->open_url(url, len);
    }
};

static_assert(host_platform<native_host>);

namespace detail {

struct AppState {
    native_host* host = nullptr;
    float scroll_x = 0.0f;
    float scroll_y = 0.0f;
    // Last cursor position seen by `dispatch_cursor_pos` — wheel events
    // arrive without an attached cursor location, so the scroll
    // dispatcher uses these to hit-test against `g_app.scroll_targets`
    // before falling back to the root document scroll.
    float last_mouse_x = 0.0f;
    float last_mouse_y = 0.0f;
    unsigned int hovered_id = invalid_callback_id;
    bool drag_selecting = false;
    unsigned int drag_selection_id = invalid_callback_id;
    std::chrono::steady_clock::time_point next_caret_blink{};
    bool caret_blink_armed = false;
    bool repaint_in_progress = false;
    bool repaint_requested = false;
    std::chrono::steady_clock::time_point last_input_frame{};
    std::chrono::steady_clock::time_point deferred_input_frame_deadline{};
    bool deferred_input_paint = false;
    bool deferred_input_rebuild = false;
    char const* deferred_input_action = "input";
};

inline AppState g_app_state;
inline native_host* g_active_host = nullptr;

inline native_host* active_host() {
    return g_active_host;
}

inline void notify_viewport_changed(native_host* host,
                                    int width_px, int height_px,
                                    float content_scale) {
    if (host && host->on_viewport_changed)
        host->on_viewport_changed(width_px, height_px, content_scale);
}

inline void open_url_bridge(char const* url, unsigned int len) {
    if (g_active_host)
        g_active_host->open_url(url, len);
}

inline void sync_platform_input() {
    if (!g_app_state.host || !g_app_state.host->platform
        || !g_app_state.host->platform->input.sync)
        return;
    g_app_state.host->platform->input.sync();
}

inline bool platform_uses_shared_caret_blink() {
    auto const* platform = g_app_state.host ? g_app_state.host->platform : nullptr;
    if (!platform || !platform->input.uses_shared_caret_blink)
        return true;
    return platform->input.uses_shared_caret_blink();
}

inline std::optional<std::chrono::milliseconds>
platform_caret_blink_interval() {
    auto const* platform = g_app_state.host ? g_app_state.host->platform : nullptr;
    if (!platform || !platform->input.caret_blink_interval_ms)
        return std::chrono::milliseconds{530};
    int const interval = platform->input.caret_blink_interval_ms();
    if (interval == 0)
        return std::nullopt;
    if (interval < 100 || interval > 5000)
        return std::chrono::milliseconds{530};
    return std::chrono::milliseconds{interval};
}

inline std::optional<unsigned int> hit_test(float x, float y,
                                            float scroll_x, float scroll_y) {
    if (!g_app_state.host || !g_app_state.host->platform
        || !g_app_state.host->platform->renderer.hit_test)
        return std::nullopt;
    return g_app_state.host->platform->renderer.hit_test(x, y, scroll_x, scroll_y);
}

inline float scroll_line_height() {
    auto const& theme = ::phenotype::current_theme();
    float line_height = theme.body_font_size * theme.line_height_ratio;
    return (line_height > 0.0f) ? line_height : 1.0f;
}

inline float normalize_scroll_delta(platform_api const* platform,
                                    double dy,
                                    float line_height,
                                    float viewport_height) {
    if (dy == 0.0) return 0.0f;
    if (line_height <= 0.0f)
        line_height = 1.0f;
    float delta = platform && platform->input.scroll_delta_y
        ? platform->input.scroll_delta_y(dy, line_height, viewport_height)
        : static_cast<float>(dy) * line_height * 3.0f;
    auto const& theme = ::phenotype::current_theme();
    float multiplier = theme.scroll_delta_multiplier;
    if (!(multiplier > 0.0f) || !std::isfinite(multiplier))
        multiplier = 1.0f;
    return delta * multiplier;
}

inline float normalize_scroll_delta_x(platform_api const* platform,
                                      double dx,
                                      float line_height,
                                      float viewport_width) {
    if (dx == 0.0) return 0.0f;
    if (line_height <= 0.0f)
        line_height = 1.0f;
    float delta = platform && platform->input.scroll_delta_x
        ? platform->input.scroll_delta_x(dx, line_height, viewport_width)
        : static_cast<float>(dx) * line_height * 3.0f;
    auto const& theme = ::phenotype::current_theme();
    float multiplier = theme.scroll_horizontal_delta_multiplier;
    if (!(multiplier > 0.0f) || !std::isfinite(multiplier))
        multiplier = 1.0f;
    return delta * multiplier;
}

inline void repaint_current() {
    if (!g_app_state.host)
        return;
    if (g_app_state.repaint_in_progress) {
        g_app_state.repaint_requested = true;
        return;
    }

    g_app_state.repaint_in_progress = true;
    do {
        g_app_state.repaint_requested = false;
        ::phenotype::detail::repaint(*g_app_state.host,
                                     g_app_state.scroll_x,
                                     g_app_state.scroll_y);
        sync_platform_input();
    } while (g_app_state.repaint_requested);
    g_app_state.repaint_in_progress = false;
}

inline void repaint_current_after_surface_presented() {
    // The initial run() paint can occur before a native drawable is visible.
    // Force one startup repaint once the platform confirms the surface is shown.
    ::phenotype::detail::g_app.last_paint_hash = 0;
    repaint_current();
}

inline constexpr auto input_frame_interval = std::chrono::milliseconds{16};

inline bool input_frame_due(std::chrono::steady_clock::time_point now) {
    return g_app_state.last_input_frame.time_since_epoch().count() == 0
        || now - g_app_state.last_input_frame >= input_frame_interval;
}

inline void remember_input_frame(std::chrono::steady_clock::time_point start) {
    g_app_state.last_input_frame = start;
    g_app_state.deferred_input_paint = false;
    g_app_state.deferred_input_rebuild = false;
    g_app_state.deferred_input_action = "input";
}

inline void run_input_frame(char const* action,
                            bool rebuild,
                            std::chrono::steady_clock::time_point start) {
    bool const previous_input_motion =
        ::phenotype::detail::g_app.has_active_input_motion;
    ::phenotype::detail::g_app.has_active_input_motion = true;
    if (rebuild)
        ::phenotype::detail::trigger_rebuild();
    else
        repaint_current();
    ::phenotype::detail::g_app.has_active_input_motion =
        previous_input_motion;
    auto const end = std::chrono::steady_clock::now();
    auto const elapsed =
        std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);
    ::phenotype::detail::record_action_duration(
        action,
        static_cast<std::uint64_t>(elapsed.count()));
    remember_input_frame(start);
}

inline void request_input_frame(char const* action, bool rebuild) {
    auto const now = std::chrono::steady_clock::now();
    if (input_frame_due(now)) {
        run_input_frame(action, rebuild, now);
        return;
    }
    g_app_state.deferred_input_paint = true;
    g_app_state.deferred_input_rebuild =
        g_app_state.deferred_input_rebuild || rebuild;
    g_app_state.deferred_input_action = action ? action : "input";
    g_app_state.deferred_input_frame_deadline =
        g_app_state.last_input_frame + input_frame_interval;
}

inline bool service_deferred_input_frame(
        std::chrono::steady_clock::time_point now) {
    if (!g_app_state.deferred_input_paint
        || now < g_app_state.deferred_input_frame_deadline) {
        return false;
    }
    run_input_frame(
        g_app_state.deferred_input_action,
        g_app_state.deferred_input_rebuild,
        now);
    return true;
}

template<typename F>
inline bool measure_input_action(char const* action, F&& fn) {
    auto const start = std::chrono::steady_clock::now();
    bool const handled = std::invoke(std::forward<F>(fn));
    auto const end = std::chrono::steady_clock::now();
    auto const elapsed =
        std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);
    ::phenotype::detail::record_action_duration(
        action,
        static_cast<std::uint64_t>(elapsed.count()));
    return handled;
}

inline void bind_host(native_host& host,
                      float scroll_x = 0.0f,
                      float scroll_y = 0.0f) {
    g_app_state = {
        &host,
        scroll_x,
        scroll_y,
        0.0f,                     // last_mouse_x
        0.0f,                     // last_mouse_y
        invalid_callback_id,
        false,
        invalid_callback_id,
        {},
        false,
        false,
        false,
    };
    ::phenotype::detail::set_scroll_x(scroll_x);
    ::phenotype::detail::set_scroll_y(scroll_y);
    ::phenotype::detail::set_hover_id_without_event(invalid_callback_id);
}

inline float measure_utf8_prefix(::phenotype::FocusedInputSnapshot const& snapshot,
                                 std::size_t bytes) {
    if (!g_app_state.host)
        return 0.0f;
    bytes = ::phenotype::detail::clamp_utf8_boundary(snapshot.value, bytes);
    ::phenotype::FontSpec const font{
        {}, ::phenotype::FontWeight::Regular,
        ::phenotype::FontStyle::Upright, snapshot.mono };
    return g_app_state.host->measure_text(
        snapshot.font_size, font,
        snapshot.value.data(),
        static_cast<unsigned int>(bytes));
}

inline std::size_t caret_pos_from_pointer_x(::phenotype::FocusedInputSnapshot const& snapshot,
                                            float pointer_x) {
    if (snapshot.value.empty())
        return 0;

    float base_x = snapshot.x + snapshot.padding[3];
    if (pointer_x <= base_x)
        return 0;

    float full_width = measure_utf8_prefix(snapshot, snapshot.value.size());
    if (pointer_x >= base_x + full_width)
        return snapshot.value.size();

    float target = pointer_x - base_x;
    std::size_t prev = 0;
    float prev_width = 0.0f;
    while (prev < snapshot.value.size()) {
        auto next = static_cast<std::size_t>(
            ::phenotype::detail::next_utf8_boundary(snapshot.value, prev));
        float next_width = measure_utf8_prefix(snapshot, next);
        float midpoint = prev_width + (next_width - prev_width) * 0.5f;
        if (target < midpoint)
            return prev;
        if (target <= next_width)
            return next;
        prev = next;
        prev_width = next_width;
    }
    return snapshot.value.size();
}

inline bool move_focused_caret_from_pointer_x(float pointer_x) {
    auto snapshot = ::phenotype::detail::focused_input_snapshot();
    if (!snapshot.valid)
        return false;
    auto next_caret = caret_pos_from_pointer_x(snapshot, pointer_x);
    auto previous_caret = ::phenotype::detail::get_caret_pos();
    auto previous_visible = ::phenotype::detail::get_caret_visible();
    if (!::phenotype::detail::set_focused_input_caret_pos(next_caret, true))
        return false;
    return previous_caret != ::phenotype::detail::get_caret_pos() || !previous_visible;
}

inline bool move_focused_selection_from_pointer_x(float pointer_x, bool extend_selection) {
    auto snapshot = ::phenotype::detail::focused_input_snapshot();
    if (!snapshot.valid)
        return false;

    auto next_caret = caret_pos_from_pointer_x(snapshot, pointer_x);
    auto previous_caret = ::phenotype::detail::get_caret_pos();
    auto previous_anchor = ::phenotype::detail::get_selection_anchor();
    auto previous_visible = ::phenotype::detail::get_caret_visible();

    bool changed = false;
    if (extend_selection) {
        auto anchor = previous_anchor == invalid_callback_id
            ? previous_caret
            : previous_anchor;
        if (!::phenotype::detail::set_focused_input_selection(anchor, next_caret, true))
            return false;
        changed = previous_anchor != ::phenotype::detail::get_selection_anchor()
            || previous_caret != ::phenotype::detail::get_caret_pos();
    } else {
        if (!::phenotype::detail::set_focused_input_selection(next_caret, next_caret, true))
            return false;
        changed = previous_anchor != ::phenotype::detail::get_selection_anchor()
            || previous_caret != ::phenotype::detail::get_caret_pos();
    }
    return changed || !previous_visible;
}

inline bool has_select_all_modifier(int mods) {
#ifdef __APPLE__
    return (mods & static_cast<int>(Modifier::Super)) != 0;
#else
    return (mods & static_cast<int>(Modifier::Control)) != 0;
#endif
}

inline int normalized_key_command_modifiers(int mods) {
    return mods
        & (static_cast<int>(Modifier::Shift)
           | static_cast<int>(Modifier::Control)
           | static_cast<int>(Modifier::Alt)
           | static_cast<int>(Modifier::Super));
}

inline void reset_caret_blink_timer() {
    if (!platform_uses_shared_caret_blink()) {
        g_app_state.caret_blink_armed = false;
        return;
    }
    if (!::phenotype::detail::focused_is_input()) {
        g_app_state.caret_blink_armed = false;
        return;
    }
    auto const interval = platform_caret_blink_interval();
    if (!interval) {
        g_app_state.caret_blink_armed = false;
        if (!::phenotype::detail::get_caret_visible())
            ::phenotype::detail::toggle_caret();
        return;
    }
    g_app_state.caret_blink_armed = true;
    g_app_state.next_caret_blink = std::chrono::steady_clock::now() + *interval;
}

inline void tick_caret_blink() {
    if (!platform_uses_shared_caret_blink()) {
        g_app_state.caret_blink_armed = false;
        return;
    }
    if (!::phenotype::detail::focused_is_input()) {
        g_app_state.caret_blink_armed = false;
        return;
    }
    auto const interval = platform_caret_blink_interval();
    if (!interval) {
        g_app_state.caret_blink_armed = false;
        return;
    }
    if (!g_app_state.caret_blink_armed) {
        reset_caret_blink_timer();
        return;
    }
    auto now = std::chrono::steady_clock::now();
    if (now < g_app_state.next_caret_blink)
        return;
    ::phenotype::detail::toggle_caret();
    g_app_state.next_caret_blink = now + *interval;
    repaint_current();
}

inline void update_hover_cursor(bool pointing) {
    if (g_app_state.host && g_app_state.host->set_hover_cursor)
        g_app_state.host->set_hover_cursor(pointing);
}

inline float viewport_height() {
    if (g_app_state.host)
        return g_app_state.host->canvas_height();
    return 600.0f;
}

inline float viewport_width() {
    if (g_app_state.host)
        return g_app_state.host->canvas_width();
    return 800.0f;
}

inline float max_scroll_for_viewport(float viewport_height) {
    float total = ::phenotype::detail::get_total_height();
    float max_scroll = total - viewport_height;
    return (max_scroll > 0.0f) ? max_scroll : 0.0f;
}

inline float max_scroll_x_for_viewport(float viewport_width) {
    float total = ::phenotype::detail::get_total_width();
    float max_scroll = total - viewport_width;
    return (max_scroll > 0.0f) ? max_scroll : 0.0f;
}

// Scroll dispatch usually only needs a paint pass — the layout tree
// is unchanged and just `scroll_y/x` shifts. While a view-time
// animation is in flight (e.g. `progress_indeterminate`), though,
// each scroll event lands between auto-tick rebuilds and would
// otherwise repaint the same stale slug position before the next
// tick rebuilds the view; that reads as a visible stutter on the
// animation. Promote scroll to a rebuild while the runner is asking
// for ticks so the animation advances in lockstep with scroll
// repaints.
inline void schedule_post_scroll_paint() {
    // The runner reads `g_app.scroll_*` directly during paint; in the
    // paint-only path `repaint_current` mirrors `g_app_state.scroll_*`
    // onto it as a side effect. The rebuild path bypasses that mirror,
    // so propagate explicitly here before either branch fires.
    ::phenotype::detail::set_scroll_x(g_app_state.scroll_x);
    ::phenotype::detail::set_scroll_y(g_app_state.scroll_y);
    if (::phenotype::detail::g_app.has_active_animations)
        request_input_frame("scroll", true);
    else
        request_input_frame("scroll", false);
}

inline bool set_scroll_position(float next_scroll,
                                float viewport_height,
                                char const* detail) {
    float max_scroll = max_scroll_for_viewport(viewport_height);
    if (next_scroll < 0.0f) next_scroll = 0.0f;
    if (next_scroll > max_scroll) next_scroll = max_scroll;
    if (next_scroll == g_app_state.scroll_y) {
        ::phenotype::detail::note_input_event(
            "scroll", "shell", detail, "ignored", invalid_callback_id);
        return false;
    }
    g_app_state.scroll_y = next_scroll;
    schedule_post_scroll_paint();
    ::phenotype::detail::note_input_event(
        "scroll", "shell", detail, "handled", invalid_callback_id);
    return true;
}

inline bool set_scroll_position_x(float next_scroll,
                                  float viewport_width,
                                  char const* detail) {
    float max_scroll = max_scroll_x_for_viewport(viewport_width);
    if (next_scroll < 0.0f) next_scroll = 0.0f;
    if (next_scroll > max_scroll) next_scroll = max_scroll;
    if (next_scroll == g_app_state.scroll_x) {
        ::phenotype::detail::note_input_event(
            "scroll", "shell", detail, "ignored", invalid_callback_id);
        return false;
    }
    g_app_state.scroll_x = next_scroll;
    schedule_post_scroll_paint();
    ::phenotype::detail::note_input_event(
        "scroll", "shell", detail, "handled", invalid_callback_id);
    return true;
}

inline bool scroll_by_pixels(float delta_pixels,
                             float viewport_height,
                             char const* detail) {
    return set_scroll_position(g_app_state.scroll_y + delta_pixels,
                               viewport_height,
                               detail);
}

inline bool dispatch_scroll_pixels(float delta_pixels,
                                   float viewport_height_value,
                                   char const* detail) {
    return set_scroll_position(g_app_state.scroll_y - delta_pixels,
                               viewport_height_value,
                               detail);
}

inline bool dispatch_scroll_pixels_x(float delta_pixels,
                                     float viewport_width_value,
                                     char const* detail) {
    return set_scroll_position_x(g_app_state.scroll_x - delta_pixels,
                                 viewport_width_value,
                                 detail);
}

inline bool dispatch_scroll_lines(double delta_lines,
                                  float viewport_height_value,
                                  char const* detail) {
    return dispatch_scroll_pixels(
        static_cast<float>(delta_lines) * scroll_line_height(),
        viewport_height_value,
        detail);
}

inline bool dispatch_scroll_lines_x(double delta_lines,
                                    float viewport_width_value,
                                    char const* detail) {
    return dispatch_scroll_pixels_x(
        static_cast<float>(delta_lines) * scroll_line_height(),
        viewport_width_value,
        detail);
}

inline bool dismiss_platform_transient() {
    auto const* platform = g_app_state.host ? g_app_state.host->platform : nullptr;
    return platform && platform->input.dismiss_transient
        && platform->input.dismiss_transient();
}

// Dispatch a translated gesture event from the platform input layer.
// `focus_x` / `focus_y` are absolute surface coords (post-scroll); the
// shell translates to canvas-local before invoking the registered
// callback, and rejects events whose focal point falls outside the
// active canvas's painted bounds. Returns true when the event was
// consumed by a canvas.
inline bool dispatch_gesture(::phenotype::GestureEvent ev) {
    return measure_input_action("gesture", [&] {
    using ::phenotype::detail::g_app;
    auto id = g_app.gesture_target_id;
    if (id == 0xFFFFFFFFu) return false;
    if (id >= g_app.gesture_callbacks.size()) return false;

    float lx = ev.focus_x - g_app.gesture_target_x;
    float ly = ev.focus_y - g_app.gesture_target_y;
    if (lx < 0.0f || ly < 0.0f
        || lx > g_app.gesture_target_w
        || ly > g_app.gesture_target_h) {
        return false;
    }
    ev.focus_x = lx;
    ev.focus_y = ly;

    auto const& fn = g_app.gesture_callbacks[id];
    if (!fn) return false;
    fn(ev);
    repaint_current();
    return true;
    });
}

inline bool dispatch_mouse_button(float mx, float my,
                                  MouseButton button, KeyAction action, int mods) {
    return measure_input_action("click", [&] {
    bool const pointer_changed =
        ::phenotype::detail::set_pointer_position(mx, my);
    bool focus_visibility_cleared = false;
    if (button == MouseButton::Left && action == KeyAction::Press) {
        focus_visibility_cleared =
            ::phenotype::detail::clear_focus_visible_for_pointer(
                "shell",
                "pointer-focus-visible-reset");
        if (focus_visibility_cleared)
            sync_platform_input();
    }

    if (g_app_state.host && g_app_state.host->platform
        && g_app_state.host->platform->input.handle_mouse_button
        && g_app_state.host->platform->input.handle_mouse_button(
            mx, my, static_cast<int>(button), static_cast<int>(action), mods)) {
        bool pressed_changed = false;
        if (button == MouseButton::Left && action == KeyAction::Release) {
            pressed_changed = ::phenotype::detail::set_pressed_id(
                invalid_callback_id, "shell", "platform-release");
        }
        if (focus_visibility_cleared || pressed_changed)
            ::phenotype::detail::trigger_rebuild();
        else if (pointer_changed || action == KeyAction::Press)
            repaint_current();
        ::phenotype::detail::note_input_event(
            "click", "shell", "pointer-click", "platform-consumed", invalid_callback_id);
        return true;
    }

    if (button != MouseButton::Left) {
        ::phenotype::detail::note_input_event(
            "click", "shell", "pointer-click", "ignored", invalid_callback_id);
        if (pointer_changed)
            repaint_current();
        return pointer_changed;
    }

    if (action == KeyAction::Release) {
        bool had_drag = g_app_state.drag_selecting;
        g_app_state.drag_selecting = false;
        g_app_state.drag_selection_id = invalid_callback_id;
        bool pressed_changed = ::phenotype::detail::set_pressed_id(
            invalid_callback_id, "shell", "pointer-release");
        ::phenotype::detail::note_input_event(
            "click",
            "shell",
            "pointer-release",
            (had_drag || pressed_changed) ? "handled" : "ignored",
            ::phenotype::detail::get_focused_id());
        if (pressed_changed)
            ::phenotype::detail::trigger_rebuild();
        else if (pointer_changed)
            repaint_current();
        return had_drag || pressed_changed || pointer_changed;
    }

    if (action != KeyAction::Press) {
        ::phenotype::detail::note_input_event(
            "click", "shell", "pointer-click", "ignored", invalid_callback_id);
        if (pointer_changed)
            repaint_current();
        return pointer_changed;
    }

    if (auto hit = hit_test(mx, my, g_app_state.scroll_x, g_app_state.scroll_y)) {
        bool pressed_changed = ::phenotype::detail::set_pressed_id(
            *hit, "shell", "pointer-press");
        bool focus_changed = ::phenotype::detail::set_focus_id(
            *hit,
            "shell",
            "pointer-focus",
            false,
            ::phenotype::InputModality::Pointer);
        bool caret_changed = false;
        if (::phenotype::detail::focused_is_input()
            && ::phenotype::detail::get_focused_id() == *hit) {
            caret_changed = move_focused_selection_from_pointer_x(mx, false);
            g_app_state.drag_selecting = true;
            g_app_state.drag_selection_id = *hit;
        } else {
            g_app_state.drag_selecting = false;
            g_app_state.drag_selection_id = invalid_callback_id;
        }
        if (focus_changed || caret_changed)
            sync_platform_input();
        if (::phenotype::detail::focused_is_input())
            reset_caret_blink_timer();
        bool activated = ::phenotype::detail::handle_event(
            *hit, "shell", "pointer-click");
        // Focus changes feed view-time animations (the focus ring
        // grow/fade in widget::*); take the rebuild path so the next
        // view evaluates `is_focused` for the new id. Activation
        // already triggers its own rebuild from inside the registered
        // callback, but routing focus through trigger_rebuild here
        // keeps the no-callback paths (text_field click) animated.
        // Caret-only updates stay on `repaint_current` since the caret
        // is drawn at paint time.
        if (focus_visibility_cleared || pressed_changed || focus_changed
            || activated)
            ::phenotype::detail::trigger_rebuild();
        else if (caret_changed || pointer_changed)
            repaint_current();
        return focus_visibility_cleared || pressed_changed || focus_changed
            || caret_changed || activated || pointer_changed;
    }

    g_app_state.drag_selecting = false;
    g_app_state.drag_selection_id = invalid_callback_id;
    bool cleared = ::phenotype::detail::set_focus_id(
        invalid_callback_id, "shell", "pointer-clear");
    bool pressed_cleared = ::phenotype::detail::set_pressed_id(
        invalid_callback_id, "shell", "pointer-clear");
    if (cleared)
        sync_platform_input();
    if (cleared)
        reset_caret_blink_timer();
    if (focus_visibility_cleared || cleared || pressed_cleared)
        ::phenotype::detail::trigger_rebuild();
    else if (pointer_changed)
        repaint_current();
    return focus_visibility_cleared || cleared || pressed_cleared
        || pointer_changed;
    });
}

inline bool dispatch_mouse_button(float mx, float my,
                                  int button, int action, int mods) {
    return dispatch_mouse_button(
        mx,
        my,
        mouse_button_from_legacy_code(button),
        key_action_from_legacy_code(action),
        mods);
}

inline bool dispatch_cursor_pos(float mx, float my) {
    g_app_state.last_mouse_x = mx;
    g_app_state.last_mouse_y = my;
    bool const pointer_changed =
        ::phenotype::detail::set_pointer_position(mx, my);
    if (g_app_state.host && g_app_state.host->platform
        && g_app_state.host->platform->input.handle_cursor_pos
        && g_app_state.host->platform->input.handle_cursor_pos(mx, my)) {
        g_app_state.hovered_id = invalid_callback_id;
        ::phenotype::detail::set_hover_id_without_event(invalid_callback_id);
        request_input_frame("hover", false);
        update_hover_cursor(false);
        ::phenotype::detail::note_input_event(
            "hover", "shell", "pointer-move", "platform-consumed", invalid_callback_id);
        return true;
    }

    if (g_app_state.drag_selecting
        && ::phenotype::detail::focused_is_input()
        && ::phenotype::detail::get_focused_id() == g_app_state.drag_selection_id) {
        auto hit = hit_test(mx, my, g_app_state.scroll_x, g_app_state.scroll_y);
        auto hovered_id = hit.value_or(invalid_callback_id);
        g_app_state.hovered_id = hovered_id;
        ::phenotype::detail::set_hover_id_without_event(hovered_id);
        bool handled = move_focused_selection_from_pointer_x(mx, true);
        if (handled)
            request_input_frame("hover", false);
        else if (pointer_changed)
            request_input_frame("hover", false);
        update_hover_cursor(hit.has_value());
        ::phenotype::detail::note_input_event(
            "click",
            "shell",
            "pointer-drag",
            handled ? "handled" : "ignored",
            g_app_state.drag_selection_id);
        return handled;
    }

    auto hit = hit_test(mx, my, g_app_state.scroll_x, g_app_state.scroll_y);
    auto hovered_id = hit.value_or(invalid_callback_id);
    g_app_state.hovered_id = hovered_id;
    bool handled = ::phenotype::detail::set_hover_id(
        hovered_id, "shell", "pointer-move");
    // Hover transitions are now view-time animations (animate_color in
    // widget::button etc.), so the new hovered_id has to feed back into
    // the next view rebuild — `repaint_current` alone leaves the arena
    // showing the previous frame's hover sample.
        if (handled)
            request_input_frame("hover", true);
        else if (pointer_changed)
            request_input_frame("hover", false);
        update_hover_cursor(hit.has_value());
        return handled || pointer_changed;
    }

// Try to route `delta_pixels` into the topmost scroll_view whose
// painted rect contains the cursor. Walk in reverse because paint_node
// pushes scroll_targets in z-order ascending (innermost / latest =
// last entry), so the reverse pass picks the visually-frontmost one
// first. Returns true when a scroll_view consumed the delta — the
// caller suppresses the root-scroll fallback in that case.
inline bool dispatch_scroll_in_view(float mx, float my, float delta_pixels) {
    auto& targets = ::phenotype::detail::g_app.scroll_targets;
    if (targets.empty() || delta_pixels == 0.0f) return false;
    for (std::size_t i = targets.size(); i-- > 0; ) {
        auto& tgt = targets[i];
        if (mx < tgt.x || mx > tgt.x + tgt.w
            || my < tgt.y || my > tgt.y + tgt.h)
            continue;
        if (!tgt.state) continue;
        float max_off = tgt.content_height - tgt.h;
        if (max_off < 0.0f) max_off = 0.0f;
        float prev = tgt.state->offset_y;
        // Sign convention mirrors `dispatch_scroll_pixels` for the
        // root scroll: a positive `delta_pixels` (wheel rolled forward,
        // platform-normalised) moves us toward the top, so the offset
        // decreases.
        float next = prev - delta_pixels;
        if (next < 0.0f) next = 0.0f;
        if (next > max_off) next = max_off;
        if (next == prev) {
            ::phenotype::detail::note_input_event(
                "scroll", "shell", "wheel-view", "ignored", invalid_callback_id);
            return true;
        }
        tgt.state->offset_y = next;
        schedule_post_scroll_paint();
        ::phenotype::detail::note_input_event(
            "scroll", "shell", "wheel-view", "handled", invalid_callback_id);
        return true;
    }
    return false;
}

inline bool dispatch_scroll(double dy, float viewport_height_value) {
    auto const* platform = g_app_state.host ? g_app_state.host->platform : nullptr;
    float scroll_delta = normalize_scroll_delta(
        platform,
        dy,
        scroll_line_height(),
        viewport_height_value);
    if (scroll_delta == 0.0f) {
        ::phenotype::detail::note_input_event(
            "scroll", "shell", "wheel", "ignored", invalid_callback_id);
        return false;
    }
    if (dispatch_scroll_in_view(g_app_state.last_mouse_x,
                                g_app_state.last_mouse_y,
                                scroll_delta)) {
        return true;
    }
    return dispatch_scroll_pixels(scroll_delta,
                                  viewport_height_value,
                                  "wheel");
}

// Two-axis wheel dispatch. dx > 0 scrolls content to the right (so the
// viewport moves left), dy > 0 scrolls content down (viewport moves up)
// — matching the existing scroll_y sign convention.
inline bool dispatch_scroll_xy(double dx, double dy,
                               float viewport_width_value,
                               float viewport_height_value) {
    bool changed = false;
    if (dy != 0.0)
        changed |= dispatch_scroll(dy, viewport_height_value);
    if (dx != 0.0) {
        float line_h = scroll_line_height();
        float dxp = normalize_scroll_delta_x(
            g_app_state.host ? g_app_state.host->platform : nullptr,
            dx,
            line_h,
            viewport_width_value);
        if (dxp != 0.0f)
            changed |= dispatch_scroll_pixels_x(dxp,
                                                viewport_width_value,
                                                "wheel-x");
    }
    return changed;
}

inline bool dispatch_activation(char const* detail, bool include_links) {
    auto focused_id = ::phenotype::detail::get_focused_id();
    if (focused_id == invalid_callback_id) {
        ::phenotype::detail::note_input_event(
            "key", "shell", detail, "ignored", focused_id);
        return false;
    }

    auto role = ::phenotype::detail::focused_role();
    bool allowed = role == ::phenotype::InteractionRole::Button
        || role == ::phenotype::InteractionRole::Checkbox
        || role == ::phenotype::InteractionRole::Radio
        || (include_links && role == ::phenotype::InteractionRole::Link);
    if (!allowed) {
        ::phenotype::detail::note_input_event(
            "key", "shell", detail, "ignored", focused_id);
        return false;
    }

    bool handled = ::phenotype::detail::handle_event(focused_id, "shell", detail);
    if (handled)
        repaint_current();
    return handled;
}

inline char const* key_detail_name(Key key, bool shift) {
    switch (key) {
        case Key::Tab: return shift ? "shift-tab" : "tab";
        case Key::Backspace: return "backspace";
        case Key::Delete: return "delete";
        case Key::Enter:
        case Key::KpEnter: return "enter";
        case Key::Space: return "space";
        case Key::Left: return shift ? "shift-arrow-left" : "arrow-left";
        case Key::Right: return shift ? "shift-arrow-right" : "arrow-right";
        case Key::Up: return "arrow-up";
        case Key::Down: return "arrow-down";
        case Key::PageUp: return "page-up";
        case Key::PageDown: return "page-down";
        case Key::Home: return shift ? "shift-home" : "home";
        case Key::End: return shift ? "shift-end" : "end";
        case Key::Escape: return "escape";
        case Key::A: return "select-all";
        case Key::D: return "d";
        case Key::F: return "f";
        case Key::N: return "n";
        case Key::F12: return "f12";
        case Key::Other: return "key";
    }
    return "key";
}

inline bool dispatch_registered_key_command(Key key,
                                            int mods,
                                            char const* detail) {
    if (::phenotype::detail::handle_key_command(
            static_cast<unsigned int>(key),
            normalized_key_command_modifiers(mods),
            ::phenotype::detail::focused_is_input(),
            "shell",
            detail)) {
        repaint_current();
        return true;
    }
    return false;
}

inline int debug_panel_shortcut_modifiers() {
#ifdef __APPLE__
    return static_cast<int>(Modifier::Super);
#else
    return static_cast<int>(Modifier::Control);
#endif
}

inline bool dispatch_debug_panel_shortcut(Key key,
                                          KeyAction action,
                                          int mods,
                                          char const* detail) {
#ifndef NDEBUG
    if (action == KeyAction::Press
        && key == Key::F12
        && normalized_key_command_modifiers(mods)
            == debug_panel_shortcut_modifiers()) {
        return ::phenotype::detail::toggle_debug_panel("shell", detail);
    }
#else
    (void)key;
    (void)action;
    (void)mods;
    (void)detail;
#endif
    return false;
}

inline bool dispatch_key(Key key, KeyAction action, int mods) {
    return measure_input_action("key", [&] {
    bool shift = (mods & static_cast<int>(Modifier::Shift)) != 0;
    auto detail = key_detail_name(key, shift);
    auto repeat_allowed = key == Key::Backspace
        || key == Key::Delete
        || key == Key::Left
        || key == Key::Right
        || key == Key::Up
        || key == Key::Down
        || key == Key::PageUp
        || key == Key::PageDown
        || key == Key::Home
        || key == Key::End;

    if (action != KeyAction::Press && action != KeyAction::Repeat) {
        ::phenotype::detail::note_input_event(
            "key", "shell", detail, "ignored", ::phenotype::detail::get_focused_id());
        return false;
    }
    if (action == KeyAction::Repeat && !repeat_allowed) {
        ::phenotype::detail::note_input_event(
            "key", "shell", detail, "ignored", ::phenotype::detail::get_focused_id());
        return false;
    }
    if (dispatch_debug_panel_shortcut(key, action, mods, detail))
        return true;
    if (key == Key::A && action == KeyAction::Press && has_select_all_modifier(mods)) {
        if (!::phenotype::detail::focused_is_input()
            || !::phenotype::detail::select_all_focused_input()) {
            ::phenotype::detail::note_input_event(
                "key", "shell", detail, "ignored", ::phenotype::detail::get_focused_id());
            return false;
        }
        reset_caret_blink_timer();
        repaint_current();
        ::phenotype::detail::note_input_event(
            "key", "shell", detail, "handled", ::phenotype::detail::get_focused_id());
        return true;
    }

    bool const command_chord =
        (normalized_key_command_modifiers(mods)
         & (static_cast<int>(Modifier::Control)
            | static_cast<int>(Modifier::Alt)
            | static_cast<int>(Modifier::Super))) != 0;
    if (command_chord
        && dispatch_registered_key_command(key, mods, detail)) {
        return true;
    }

    switch (key) {
        case Key::Tab:
            if (::phenotype::detail::handle_tab(shift ? 1u : 0u, "shell", detail)) {
                sync_platform_input();
                reset_caret_blink_timer();
                // Tab moves focus, which kicks off the focus-ring
                // animation; rebuild so the new focused widget's view
                // body samples `is_focused == true`.
                ::phenotype::detail::trigger_rebuild();
                return true;
            }
            return false;
        case Key::Backspace:
            if (::phenotype::detail::handle_key(1, 0, false, "shell", detail)) {
                reset_caret_blink_timer();
                repaint_current();
                return true;
            }
            if (dispatch_registered_key_command(key, mods, detail))
                return true;
            return false;
        case Key::Delete:
            if (::phenotype::detail::handle_key(6, 0, false, "shell", detail)) {
                reset_caret_blink_timer();
                repaint_current();
                return true;
            }
            if (dispatch_registered_key_command(key, mods, detail))
                return true;
            return false;
        case Key::Enter:
        case Key::KpEnter:
            if (dispatch_activation(detail, true))
                return true;
            return dispatch_registered_key_command(key, mods, detail);
        case Key::Space:
            if (::phenotype::detail::focused_is_input()) {
                ::phenotype::detail::note_input_event(
                    "key", "shell", detail, "ignored", ::phenotype::detail::get_focused_id());
                return false;
            }
            return dispatch_activation(detail, false);
        case Key::Left:
            if (::phenotype::detail::handle_key(2, 0, shift, "shell", detail)) {
                reset_caret_blink_timer();
                repaint_current();
                return true;
            }
            return false;
        case Key::Right:
            if (::phenotype::detail::handle_key(3, 0, shift, "shell", detail)) {
                reset_caret_blink_timer();
                repaint_current();
                return true;
            }
            return false;
        case Key::Up:
            if (::phenotype::detail::focused_is_input()) {
                ::phenotype::detail::note_input_event(
                    "key", "shell", detail, "ignored", ::phenotype::detail::get_focused_id());
                return false;
            }
            return scroll_by_pixels(-scroll_line_height(), viewport_height(), detail);
        case Key::Down:
            if (::phenotype::detail::focused_is_input()) {
                ::phenotype::detail::note_input_event(
                    "key", "shell", detail, "ignored", ::phenotype::detail::get_focused_id());
                return false;
            }
            return scroll_by_pixels(scroll_line_height(), viewport_height(), detail);
        case Key::PageUp: {
            float page = viewport_height() - scroll_line_height();
            if (page < scroll_line_height())
                page = scroll_line_height();
            return scroll_by_pixels(-page, viewport_height(), detail);
        }
        case Key::PageDown: {
            float page = viewport_height() - scroll_line_height();
            if (page < scroll_line_height())
                page = scroll_line_height();
            return scroll_by_pixels(page, viewport_height(), detail);
        }
        case Key::Home:
            if (::phenotype::detail::focused_is_input()) {
                if (::phenotype::detail::handle_key(4, 0, shift, "shell", detail)) {
                    reset_caret_blink_timer();
                    repaint_current();
                    return true;
                }
                return false;
            }
            return set_scroll_position(0.0f, viewport_height(), detail);
        case Key::End:
            if (::phenotype::detail::focused_is_input()) {
                if (::phenotype::detail::handle_key(5, 0, shift, "shell", detail)) {
                    reset_caret_blink_timer();
                    repaint_current();
                    return true;
                }
                return false;
            }
            return set_scroll_position(max_scroll_for_viewport(viewport_height()),
                                       viewport_height(),
                                       detail);
        case Key::Escape: {
            if (dispatch_registered_key_command(key, mods, detail))
                return true;
            bool dismissed = dismiss_platform_transient();
            bool cleared = false;
            if (::phenotype::detail::get_focused_id() != invalid_callback_id)
                cleared = ::phenotype::detail::set_focus_id(
                    invalid_callback_id, "shell", "escape-clear");
            if (cleared)
                sync_platform_input();
            if (cleared)
                reset_caret_blink_timer();
            bool handled = dismissed || cleared;
            if (cleared)
                ::phenotype::detail::trigger_rebuild();
            else if (handled)
                repaint_current();
            ::phenotype::detail::note_input_event(
                "key", "shell", detail, handled ? "handled" : "ignored", invalid_callback_id);
            return handled;
        }
        case Key::D:
        case Key::F:
        case Key::N:
        case Key::F12:
            if (dispatch_registered_key_command(key, mods, detail))
                return true;
            ::phenotype::detail::note_input_event(
                "key", "shell", detail, "ignored", ::phenotype::detail::get_focused_id());
            return false;
        case Key::Other:
        default:
            ::phenotype::detail::note_input_event(
                "key", "shell", detail, "ignored", ::phenotype::detail::get_focused_id());
            return false;
    }
    });
}

inline bool dispatch_key(int key, int action, int mods) {
    return dispatch_key(
        key_from_legacy_code(key),
        key_action_from_legacy_code(action),
        mods);
}

inline bool dispatch_char(unsigned int codepoint) {
    auto detail = (codepoint == static_cast<unsigned int>(' ')) ? "space-char" : "char";
    if (!::phenotype::detail::focused_is_input()) {
        ::phenotype::detail::note_input_event(
            "key", "shell", detail, "ignored", ::phenotype::detail::get_focused_id());
        return false;
    }
    if (::phenotype::detail::handle_key(0, codepoint, false, "shell", detail)) {
        reset_caret_blink_timer();
        repaint_current();
        return true;
    }
    return false;
}

template<typename State, typename Msg, typename View, typename Update>
    requires std::invocable<View, State const&>
          && std::invocable<Update, State&, Msg>
void run_host(native_host& host, View view, Update update) {
    bind_host(host, g_app_state.scroll_x, g_app_state.scroll_y);
    detail::g_active_host = &host;
    ::phenotype::detail::g_open_url = detail::open_url_bridge;
    if (host.platform && host.platform->input.attach)
        host.platform->input.attach(host.window, detail::repaint_current);
    if (host.platform && host.platform->text.init)
        host.platform->text.init();
    if (host.platform && host.platform->renderer.init)
        host.platform->renderer.init(host.window);
    phenotype::run<State, Msg>(host, std::move(view), std::move(update));
    sync_platform_input();
}

inline void service_host_tick(std::chrono::steady_clock::time_point& last_animation_tick) {
    tick_caret_blink();
    sync_platform_input();

    auto now = std::chrono::steady_clock::now();
    bool const serviced_input_frame = service_deferred_input_frame(now);
    if (::phenotype::detail::g_app.has_active_animations) {
        if (now - last_animation_tick >= std::chrono::milliseconds(16)) {
            if (!serviced_input_frame)
                ::phenotype::detail::trigger_rebuild();
            last_animation_tick = now;
        }
    }
}

inline void shutdown_host(native_host& host) {
    if (host.platform && host.platform->input.detach)
        host.platform->input.detach();
    if (host.platform && host.platform->renderer.shutdown)
        host.platform->renderer.shutdown();
    if (host.platform && host.platform->text.shutdown)
        host.platform->text.shutdown();
    ::phenotype::detail::g_open_url = nullptr;
    ::phenotype::diag::set_application_debug_provider(nullptr);
    detail::g_active_host = nullptr;
    detail::g_app_state = {};
}

} // namespace detail

} // namespace phenotype::native
#endif
