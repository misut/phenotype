module;
#if !defined(__wasi__) && !defined(__ANDROID__)
#include <cmath>
#include <chrono>
#include <concepts>
#include <cstdio>
#include <optional>
#include <utility>

#include <GLFW/glfw3.h>
#endif

export module phenotype.native.shell;

#if !defined(__wasi__) && !defined(__ANDROID__)
import phenotype;
import phenotype.native.platform;

export namespace phenotype::native {

// Neutral input enums. Values mirror GLFW so the GLFW-side driver can
// pass-through with `static_cast` while non-GLFW drivers (Android, future
// iOS) translate their platform constants to these values. static_asserts
// below catch drift at compile time.
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

static_assert(static_cast<int>(KeyAction::Release) == GLFW_RELEASE);
static_assert(static_cast<int>(KeyAction::Press)   == GLFW_PRESS);
static_assert(static_cast<int>(KeyAction::Repeat)  == GLFW_REPEAT);

static_assert(static_cast<int>(Modifier::Shift)   == GLFW_MOD_SHIFT);
static_assert(static_cast<int>(Modifier::Control) == GLFW_MOD_CONTROL);
static_assert(static_cast<int>(Modifier::Alt)     == GLFW_MOD_ALT);
static_assert(static_cast<int>(Modifier::Super)   == GLFW_MOD_SUPER);

static_assert(static_cast<int>(MouseButton::Left)   == GLFW_MOUSE_BUTTON_LEFT);
static_assert(static_cast<int>(MouseButton::Right)  == GLFW_MOUSE_BUTTON_RIGHT);
static_assert(static_cast<int>(MouseButton::Middle) == GLFW_MOUSE_BUTTON_MIDDLE);

static_assert(static_cast<int>(Key::Tab)       == GLFW_KEY_TAB);
static_assert(static_cast<int>(Key::Backspace) == GLFW_KEY_BACKSPACE);
static_assert(static_cast<int>(Key::Enter)     == GLFW_KEY_ENTER);
static_assert(static_cast<int>(Key::KpEnter)   == GLFW_KEY_KP_ENTER);
static_assert(static_cast<int>(Key::Space)     == GLFW_KEY_SPACE);
static_assert(static_cast<int>(Key::Left)      == GLFW_KEY_LEFT);
static_assert(static_cast<int>(Key::Right)     == GLFW_KEY_RIGHT);
static_assert(static_cast<int>(Key::Up)        == GLFW_KEY_UP);
static_assert(static_cast<int>(Key::Down)      == GLFW_KEY_DOWN);
static_assert(static_cast<int>(Key::PageUp)    == GLFW_KEY_PAGE_UP);
static_assert(static_cast<int>(Key::PageDown)  == GLFW_KEY_PAGE_DOWN);
static_assert(static_cast<int>(Key::Home)      == GLFW_KEY_HOME);
static_assert(static_cast<int>(Key::End)       == GLFW_KEY_END);
static_assert(static_cast<int>(Key::Escape)    == GLFW_KEY_ESCAPE);
static_assert(static_cast<int>(Key::A)         == GLFW_KEY_A);

} // namespace phenotype::native

export namespace phenotype::native {

struct native_host {
    GLFWwindow* window = nullptr;
    platform_api const* platform = nullptr;

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
        if (!window) return 800.0f;
        int w = 0;
        int h = 0;
        glfwGetWindowSize(window, &w, &h);
        return static_cast<float>(w);
    }

    float canvas_height() const {
        if (!window) return 600.0f;
        int w = 0;
        int h = 0;
        glfwGetWindowSize(window, &w, &h);
        return static_cast<float>(h);
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

inline float glfw_backing_scale(GLFWwindow* window) {
    if (!window) return 1.0f;
#if defined(GLFW_VERSION_MAJOR) \
    && ((GLFW_VERSION_MAJOR > 3) || (GLFW_VERSION_MAJOR == 3 && GLFW_VERSION_MINOR >= 3))
    float sx = 1.0f;
    float sy = 1.0f;
    glfwGetWindowContentScale(window, &sx, &sy);
#else
    int fbw = 0;
    int fbh = 0;
    int winw = 0;
    int winh = 0;
    glfwGetFramebufferSize(window, &fbw, &fbh);
    glfwGetWindowSize(window, &winw, &winh);
    float sx = (winw > 0) ? static_cast<float>(fbw) / static_cast<float>(winw) : 1.0f;
    float sy = (winh > 0) ? static_cast<float>(fbh) / static_cast<float>(winh) : 1.0f;
#endif
    float scale = (sx > sy) ? sx : sy;
    return (scale > 0.0f && std::isfinite(scale)) ? scale : 1.0f;
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
    return (mods & GLFW_MOD_SUPER) != 0;
#else
    return (mods & GLFW_MOD_CONTROL) != 0;
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

inline void update_cursor(GLFWwindow* window, bool pointing) {
    if (!window)
        return;
    static GLFWcursor* arrow = nullptr;
    static GLFWcursor* hand = nullptr;
    if (!arrow) arrow = glfwCreateStandardCursor(GLFW_ARROW_CURSOR);
    if (!hand) hand = glfwCreateStandardCursor(GLFW_POINTING_HAND_CURSOR);
    glfwSetCursor(window, pointing ? hand : arrow);
}

inline float viewport_height(GLFWwindow* window) {
    if (window) {
        int w = 0;
        int h = 0;
        glfwGetWindowSize(window, &w, &h);
        return (h > 0) ? static_cast<float>(h) : 1.0f;
    }
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
                                  int button, int action, int mods,
                                  GLFWwindow* window = nullptr) {
    if (g_app_state.host && g_app_state.host->platform
        && g_app_state.host->platform->input.handle_mouse_button
        && g_app_state.host->platform->input.handle_mouse_button(
            mx, my, button, action, mods)) {
        repaint_current();
        ::phenotype::detail::note_input_event(
            "click", "shell", "pointer-click", "platform-consumed", invalid_callback_id);
        return true;
    }

    if (button != GLFW_MOUSE_BUTTON_LEFT) {
        ::phenotype::detail::note_input_event(
            "click", "shell", "pointer-click", "ignored", invalid_callback_id);
        return false;
    }

    if (action == GLFW_RELEASE) {
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

    if (action != GLFW_PRESS) {
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

inline bool dispatch_cursor_pos(float mx, float my,
                                GLFWwindow* window = nullptr) {
    if (g_app_state.host && g_app_state.host->platform
        && g_app_state.host->platform->input.handle_cursor_pos
        && g_app_state.host->platform->input.handle_cursor_pos(mx, my)) {
        g_app_state.hovered_id = invalid_callback_id;
        ::phenotype::detail::set_hover_id_without_event(invalid_callback_id);
        repaint_current();
        update_cursor(window, false);
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
        update_cursor(window, hit.has_value());
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
    update_cursor(window, hit.has_value());
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

inline char const* key_detail_name(int key, bool shift) {
    switch (key) {
        case GLFW_KEY_TAB: return shift ? "shift-tab" : "tab";
        case GLFW_KEY_BACKSPACE: return "backspace";
        case GLFW_KEY_ENTER:
        case GLFW_KEY_KP_ENTER: return "enter";
        case GLFW_KEY_SPACE: return "space";
        case GLFW_KEY_LEFT: return shift ? "shift-arrow-left" : "arrow-left";
        case GLFW_KEY_RIGHT: return shift ? "shift-arrow-right" : "arrow-right";
        case GLFW_KEY_UP: return "arrow-up";
        case GLFW_KEY_DOWN: return "arrow-down";
        case GLFW_KEY_PAGE_UP: return "page-up";
        case GLFW_KEY_PAGE_DOWN: return "page-down";
        case GLFW_KEY_HOME: return shift ? "shift-home" : "home";
        case GLFW_KEY_END: return shift ? "shift-end" : "end";
        case GLFW_KEY_ESCAPE: return "escape";
        case GLFW_KEY_A: return "select-all";
        default: return "key";
    }
}

inline bool dispatch_key(int key, int action, int mods) {
    bool shift = (mods & GLFW_MOD_SHIFT) != 0;
    auto detail = key_detail_name(key, shift);
    auto repeat_allowed = key == GLFW_KEY_BACKSPACE
        || key == GLFW_KEY_LEFT
        || key == GLFW_KEY_RIGHT
        || key == GLFW_KEY_UP
        || key == GLFW_KEY_DOWN
        || key == GLFW_KEY_PAGE_UP
        || key == GLFW_KEY_PAGE_DOWN
        || key == GLFW_KEY_HOME
        || key == GLFW_KEY_END;

    if (action != GLFW_PRESS && action != GLFW_REPEAT) {
        ::phenotype::detail::note_input_event(
            "key", "shell", detail, "ignored", ::phenotype::detail::get_focused_id());
        return false;
    }
    if (action == GLFW_REPEAT && !repeat_allowed) {
        ::phenotype::detail::note_input_event(
            "key", "shell", detail, "ignored", ::phenotype::detail::get_focused_id());
        return false;
    }
    if (key == GLFW_KEY_A && action == GLFW_PRESS && has_select_all_modifier(mods)) {
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
        case GLFW_KEY_TAB:
            if (::phenotype::detail::handle_tab(shift ? 1u : 0u, "shell", detail)) {
                sync_platform_input();
                reset_caret_blink_timer();
                repaint_current();
                return true;
            }
            return false;
        case GLFW_KEY_BACKSPACE:
            if (::phenotype::detail::handle_key(1, 0, false, "shell", detail)) {
                reset_caret_blink_timer();
                repaint_current();
                return true;
            }
            return false;
        case GLFW_KEY_ENTER:
        case GLFW_KEY_KP_ENTER:
            return dispatch_activation(detail, true);
        case GLFW_KEY_SPACE:
            if (::phenotype::detail::focused_is_input()) {
                ::phenotype::detail::note_input_event(
                    "key", "shell", detail, "ignored", ::phenotype::detail::get_focused_id());
                return false;
            }
            return dispatch_activation(detail, false);
        case GLFW_KEY_LEFT:
            if (::phenotype::detail::handle_key(2, 0, shift, "shell", detail)) {
                reset_caret_blink_timer();
                repaint_current();
                return true;
            }
            return false;
        case GLFW_KEY_RIGHT:
            if (::phenotype::detail::handle_key(3, 0, shift, "shell", detail)) {
                reset_caret_blink_timer();
                repaint_current();
                return true;
            }
            return false;
        case GLFW_KEY_UP:
            if (::phenotype::detail::focused_is_input()) {
                ::phenotype::detail::note_input_event(
                    "key", "shell", detail, "ignored", ::phenotype::detail::get_focused_id());
                return false;
            }
            return scroll_by_pixels(-scroll_line_height(), viewport_height(nullptr), detail);
        case GLFW_KEY_DOWN:
            if (::phenotype::detail::focused_is_input()) {
                ::phenotype::detail::note_input_event(
                    "key", "shell", detail, "ignored", ::phenotype::detail::get_focused_id());
                return false;
            }
            return scroll_by_pixels(scroll_line_height(), viewport_height(nullptr), detail);
        case GLFW_KEY_PAGE_UP: {
            float page = viewport_height(nullptr) - scroll_line_height();
            if (page < scroll_line_height())
                page = scroll_line_height();
            return scroll_by_pixels(-page, viewport_height(nullptr), detail);
        }
        case GLFW_KEY_PAGE_DOWN: {
            float page = viewport_height(nullptr) - scroll_line_height();
            if (page < scroll_line_height())
                page = scroll_line_height();
            return scroll_by_pixels(page, viewport_height(nullptr), detail);
        }
        case GLFW_KEY_HOME:
            if (::phenotype::detail::focused_is_input()) {
                if (::phenotype::detail::handle_key(4, 0, shift, "shell", detail)) {
                    reset_caret_blink_timer();
                    repaint_current();
                    return true;
                }
                return false;
            }
            return set_scroll_position(0.0f, viewport_height(nullptr), detail);
        case GLFW_KEY_END:
            if (::phenotype::detail::focused_is_input()) {
                if (::phenotype::detail::handle_key(5, 0, shift, "shell", detail)) {
                    reset_caret_blink_timer();
                    repaint_current();
                    return true;
                }
                return false;
            }
            return set_scroll_position(max_scroll_for_viewport(viewport_height(nullptr)),
                                       viewport_height(nullptr),
                                       detail);
        case GLFW_KEY_ESCAPE: {
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

inline void on_mouse_button(GLFWwindow* window, int button, int action, int mods) {
    double mx = 0.0;
    double my = 0.0;
    glfwGetCursorPos(window, &mx, &my);
    dispatch_mouse_button(static_cast<float>(mx),
                          static_cast<float>(my),
                          button,
                          action,
                          mods,
                          window);
}

inline void on_cursor_pos(GLFWwindow* window, double mx, double my) {
    dispatch_cursor_pos(static_cast<float>(mx),
                        static_cast<float>(my),
                        window);
}

inline void on_scroll(GLFWwindow* window, double, double dy) {
    dispatch_scroll(dy, viewport_height(window));
}

inline void on_framebuffer_size(GLFWwindow*, int, int) {
    repaint_current();
}

inline void on_window_content_scale(GLFWwindow*, float, float) {
    repaint_current();
}

inline void on_key(GLFWwindow*, int key, int, int action, int mods) {
    dispatch_key(key, action, mods);
}

inline void on_char(GLFWwindow*, unsigned int codepoint) {
    dispatch_char(codepoint);
}

inline void install_callbacks(GLFWwindow* window) {
    glfwSetMouseButtonCallback(window, on_mouse_button);
    glfwSetCursorPosCallback(window, on_cursor_pos);
    glfwSetScrollCallback(window, on_scroll);
    glfwSetFramebufferSizeCallback(window, on_framebuffer_size);
#if defined(GLFW_VERSION_MAJOR) \
    && ((GLFW_VERSION_MAJOR > 3) || (GLFW_VERSION_MAJOR == 3 && GLFW_VERSION_MINOR >= 3))
    glfwSetWindowContentScaleCallback(window, on_window_content_scale);
#endif
    glfwSetKeyCallback(window, on_key);
    glfwSetCharCallback(window, on_char);
}

} // namespace detail

namespace detail {

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

template<typename State, typename Msg, typename View, typename Update>
    requires std::invocable<View, State const&>
          && std::invocable<Update, State&, Msg>
int run_app_with_platform(platform_api const& platform,
                          int width, int height, char const* title,
                          View view, Update update) {
    if (!platform.enabled) {
        std::fprintf(stderr, "Platform backend '%s' is disabled\n", platform.name);
        return 1;
    }

    if (platform.startup_message)
        std::fprintf(stderr, "%s\n", platform.startup_message);

    if (!glfwInit()) {
        std::fprintf(stderr, "Failed to initialize GLFW\n");
        return 1;
    }
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
#ifdef GLFW_SCALE_TO_MONITOR
    glfwWindowHint(GLFW_SCALE_TO_MONITOR, GLFW_TRUE);
#endif

    auto* window = glfwCreateWindow(width, height, title, nullptr, nullptr);
    if (!window) {
        std::fprintf(stderr, "Failed to create GLFW window\n");
        glfwTerminate();
        return 1;
    }

    native_host host;
    host.window = window;
    host.platform = &platform;
    detail::bind_host(host, 0.0f);
    detail::install_callbacks(window);

    run_host<State, Msg>(host, std::move(view), std::move(update));

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        tick_caret_blink();
        sync_platform_input();
    }

    shutdown_host(host);
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}

} // namespace detail

} // namespace phenotype::native
#endif
