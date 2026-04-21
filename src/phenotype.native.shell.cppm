module;
#if !defined(__wasi__)
#include <cmath>
#include <chrono>
#include <concepts>
#include <cstdio>
#include <optional>
#include <utility>
#endif

export module phenotype.native.shell;

#if !defined(__wasi__)
import phenotype;
import phenotype.native.platform;

export namespace phenotype::native {

// Neutral input enums. Values mirror GLFW so the GLFW-side driver can
// pass-through with `static_cast` while non-GLFW drivers (Android, future
// iOS) translate their platform constants to these values. Compatibility
// with GLFW is asserted from `phenotype.native.shell.glfw`, which can
// include the GLFW headers.
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

// Only the keys the shell actually switches on. `Other` is used for keys
// the shell does not care about, so drivers can safely funnel anything
// unrecognised there without creating new enumerators.
enum class Key : int {
    Tab       = 258,
    Backspace = 259,
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
    Other     = -1,
};

struct native_host {
    // Opaque native surface handle. Carries a GLFWwindow* on macOS /
    // Windows; an ANativeWindow* on Android; etc. The backend casts
    // back to its own type on receipt from `platform_api::init` /
    // `platform_api::attach`.
    native_surface_handle window = nullptr;
    platform_api const* platform = nullptr;

    // Cached framebuffer size, populated by the driver layer (shell.glfw
    // or a future shell.android). Used by the host_platform concept so
    // shell dispatch code never has to query the windowing toolkit
    // directly.
    int cached_width_px = 800;
    int cached_height_px = 600;

    // Optional driver hook for hardware-cursor updates. shell.glfw sets
    // this to a GLFW-based cursor updater; other drivers leave it null
    // (touch devices don't have a pointer cursor).
    void (*set_hover_cursor)(bool pointing) = nullptr;

    float measure_text(float font_size, unsigned int mono,
                       char const* text, unsigned int len) const {
        if (!platform || !platform->text.measure) return 0.0f;
        return platform->text.measure(font_size, mono != 0, text, len);
    }

    static constexpr unsigned int BUF_SIZE = 65536;
    alignas(4) unsigned char buffer[BUF_SIZE]{};
    unsigned int len_ = 0;

    unsigned char* buf() { return buffer; }
    unsigned int& buf_len() { return len_; }
    unsigned int buf_size() { return BUF_SIZE; }
    void ensure(unsigned int needed) {
        if (len_ + needed > BUF_SIZE) flush();
    }
    void flush() {
        if (len_ == 0) return;
        if (platform && platform->renderer.flush)
            platform->renderer.flush(buffer, len_);
        len_ = 0;
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
    float scroll_y = 0.0f;
    unsigned int hovered_id = invalid_callback_id;
    bool drag_selecting = false;
    unsigned int drag_selection_id = invalid_callback_id;
    std::chrono::steady_clock::time_point next_caret_blink{};
    bool caret_blink_armed = false;
    bool repaint_in_progress = false;
    bool repaint_requested = false;
};

inline AppState g_app_state;
inline native_host* g_active_host = nullptr;

inline native_host* active_host() {
    return g_active_host;
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

inline std::optional<unsigned int> hit_test(float x, float y, float scroll_y) {
    if (!g_app_state.host || !g_app_state.host->platform
        || !g_app_state.host->platform->renderer.hit_test)
        return std::nullopt;
    return g_app_state.host->platform->renderer.hit_test(x, y, scroll_y);
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
    if (platform && platform->input.scroll_delta_y)
        return platform->input.scroll_delta_y(dy, line_height, viewport_height);
    return static_cast<float>(dy) * line_height * 3.0f;
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
        ::phenotype::detail::repaint(*g_app_state.host, g_app_state.scroll_y);
        sync_platform_input();
    } while (g_app_state.repaint_requested);
    g_app_state.repaint_in_progress = false;
}

inline void bind_host(native_host& host, float scroll_y = 0.0f) {
    g_app_state = {
        &host,
        scroll_y,
        invalid_callback_id,
        false,
        invalid_callback_id,
        {},
        false,
        false,
        false,
    };
    ::phenotype::detail::set_scroll_y(scroll_y);
    ::phenotype::detail::set_hover_id_without_event(invalid_callback_id);
}

inline constexpr auto caret_blink_interval = std::chrono::milliseconds(530);

inline float measure_utf8_prefix(::phenotype::FocusedInputSnapshot const& snapshot,
                                 std::size_t bytes) {
    if (!g_app_state.host)
        return 0.0f;
    bytes = ::phenotype::detail::clamp_utf8_boundary(snapshot.value, bytes);
    return g_app_state.host->measure_text(
        snapshot.font_size,
        snapshot.mono ? 1u : 0u,
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

inline void reset_caret_blink_timer() {
    if (!platform_uses_shared_caret_blink()) {
        g_app_state.caret_blink_armed = false;
        return;
    }
    if (!::phenotype::detail::focused_is_input()) {
        g_app_state.caret_blink_armed = false;
        return;
    }
    g_app_state.caret_blink_armed = true;
    g_app_state.next_caret_blink = std::chrono::steady_clock::now() + caret_blink_interval;
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
    if (!g_app_state.caret_blink_armed) {
        reset_caret_blink_timer();
        return;
    }
    auto now = std::chrono::steady_clock::now();
    if (now < g_app_state.next_caret_blink)
        return;
    ::phenotype::detail::toggle_caret();
    g_app_state.next_caret_blink = now + caret_blink_interval;
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

inline float max_scroll_for_viewport(float viewport_height) {
    float total = ::phenotype::detail::get_total_height();
    float max_scroll = total - viewport_height;
    return (max_scroll > 0.0f) ? max_scroll : 0.0f;
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
    repaint_current();
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

inline bool dispatch_scroll_lines(double delta_lines,
                                  float viewport_height_value,
                                  char const* detail) {
    return dispatch_scroll_pixels(
        static_cast<float>(delta_lines) * scroll_line_height(),
        viewport_height_value,
        detail);
}

inline bool dismiss_platform_transient() {
    auto const* platform = g_app_state.host ? g_app_state.host->platform : nullptr;
    return platform && platform->input.dismiss_transient
        && platform->input.dismiss_transient();
}

inline bool dispatch_mouse_button(float mx, float my,
                                  MouseButton button, KeyAction action, int mods) {
    if (g_app_state.host && g_app_state.host->platform
        && g_app_state.host->platform->input.handle_mouse_button
        && g_app_state.host->platform->input.handle_mouse_button(
            mx, my, static_cast<int>(button), static_cast<int>(action), mods)) {
        repaint_current();
        ::phenotype::detail::note_input_event(
            "click", "shell", "pointer-click", "platform-consumed", invalid_callback_id);
        return true;
    }

    if (button != MouseButton::Left) {
        ::phenotype::detail::note_input_event(
            "click", "shell", "pointer-click", "ignored", invalid_callback_id);
        return false;
    }

    if (action == KeyAction::Release) {
        bool had_drag = g_app_state.drag_selecting;
        g_app_state.drag_selecting = false;
        g_app_state.drag_selection_id = invalid_callback_id;
        ::phenotype::detail::note_input_event(
            "click",
            "shell",
            "pointer-release",
            had_drag ? "handled" : "ignored",
            ::phenotype::detail::get_focused_id());
        return had_drag;
    }

    if (action != KeyAction::Press) {
        ::phenotype::detail::note_input_event(
            "click", "shell", "pointer-click", "ignored", invalid_callback_id);
        return false;
    }

    if (auto hit = hit_test(mx, my, g_app_state.scroll_y)) {
        bool focus_changed = ::phenotype::detail::set_focus_id(
            *hit, "shell", "pointer-focus");
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
        if (focus_changed || caret_changed || activated)
            repaint_current();
        return focus_changed || caret_changed || activated;
    }

    g_app_state.drag_selecting = false;
    g_app_state.drag_selection_id = invalid_callback_id;
    bool cleared = ::phenotype::detail::set_focus_id(
        invalid_callback_id, "shell", "pointer-clear");
    if (cleared)
        sync_platform_input();
    if (cleared)
        reset_caret_blink_timer();
    if (cleared)
        repaint_current();
    return cleared;
}

inline bool dispatch_cursor_pos(float mx, float my) {
    if (g_app_state.host && g_app_state.host->platform
        && g_app_state.host->platform->input.handle_cursor_pos
        && g_app_state.host->platform->input.handle_cursor_pos(mx, my)) {
        g_app_state.hovered_id = invalid_callback_id;
        ::phenotype::detail::set_hover_id_without_event(invalid_callback_id);
        repaint_current();
        update_hover_cursor(false);
        ::phenotype::detail::note_input_event(
            "hover", "shell", "pointer-move", "platform-consumed", invalid_callback_id);
        return true;
    }

    if (g_app_state.drag_selecting
        && ::phenotype::detail::focused_is_input()
        && ::phenotype::detail::get_focused_id() == g_app_state.drag_selection_id) {
        auto hit = hit_test(mx, my, g_app_state.scroll_y);
        auto hovered_id = hit.value_or(invalid_callback_id);
        g_app_state.hovered_id = hovered_id;
        ::phenotype::detail::set_hover_id_without_event(hovered_id);
        bool handled = move_focused_selection_from_pointer_x(mx, true);
        if (handled)
            repaint_current();
        update_hover_cursor(hit.has_value());
        ::phenotype::detail::note_input_event(
            "click",
            "shell",
            "pointer-drag",
            handled ? "handled" : "ignored",
            g_app_state.drag_selection_id);
        return handled;
    }

    auto hit = hit_test(mx, my, g_app_state.scroll_y);
    auto hovered_id = hit.value_or(invalid_callback_id);
    g_app_state.hovered_id = hovered_id;
    bool handled = ::phenotype::detail::set_hover_id(
        hovered_id, "shell", "pointer-move");
    if (handled)
        repaint_current();
    update_hover_cursor(hit.has_value());
    return handled;
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
    return dispatch_scroll_pixels(scroll_delta,
                                  viewport_height_value,
                                  "wheel");
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
        case Key::Other: return "key";
    }
    return "key";
}

inline bool dispatch_key(Key key, KeyAction action, int mods) {
    bool shift = (mods & static_cast<int>(Modifier::Shift)) != 0;
    auto detail = key_detail_name(key, shift);
    auto repeat_allowed = key == Key::Backspace
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

    switch (key) {
        case Key::Tab:
            if (::phenotype::detail::handle_tab(shift ? 1u : 0u, "shell", detail)) {
                sync_platform_input();
                reset_caret_blink_timer();
                repaint_current();
                return true;
            }
            return false;
        case Key::Backspace:
            if (::phenotype::detail::handle_key(1, 0, false, "shell", detail)) {
                reset_caret_blink_timer();
                repaint_current();
                return true;
            }
            return false;
        case Key::Enter:
        case Key::KpEnter:
            return dispatch_activation(detail, true);
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
            if (handled)
                repaint_current();
            ::phenotype::detail::note_input_event(
                "key", "shell", detail, handled ? "handled" : "ignored", invalid_callback_id);
            return handled;
        }
        case Key::Other:
        default:
            ::phenotype::detail::note_input_event(
                "key", "shell", detail, "ignored", ::phenotype::detail::get_focused_id());
            return false;
    }
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
    bind_host(host, g_app_state.scroll_y);
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

inline void shutdown_host(native_host& host) {
    if (host.platform && host.platform->input.detach)
        host.platform->input.detach();
    if (host.platform && host.platform->renderer.shutdown)
        host.platform->renderer.shutdown();
    if (host.platform && host.platform->text.shutdown)
        host.platform->text.shutdown();
    ::phenotype::detail::g_open_url = nullptr;
    detail::g_active_host = nullptr;
    detail::g_app_state = {};
}

} // namespace detail

} // namespace phenotype::native
#endif
