// GLFW-specific driver for phenotype's native shell. Owns the GLFW window
// + event loop + callback wiring and translates GLFW constants into the
// neutral enums exposed by `phenotype.native.shell`. The core shell module
// is GLFW-unaware; future drivers (Android's GameActivity, future UIKit)
// slot in as siblings of this module.

module;

#if !defined(__wasi__) && !defined(__ANDROID__)
#include <cmath>
#include <chrono>
#include <concepts>
#include <cstdio>
#include <utility>

#include <GLFW/glfw3.h>
#endif

export module phenotype.native.shell.glfw;

#if !defined(__wasi__) && !defined(__ANDROID__)
export import phenotype.native.shell;
import phenotype.native.platform;

export namespace phenotype::native {

// Compatibility with the neutral enums: every value in shell.cppm has to
// match its GLFW counterpart exactly, otherwise the `static_cast`
// pass-through used by `on_*` below quietly misclassifies input.
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

namespace detail {

inline Key translate_glfw_key(int glfw_key) {
    switch (glfw_key) {
        case GLFW_KEY_TAB: return Key::Tab;
        case GLFW_KEY_BACKSPACE: return Key::Backspace;
        case GLFW_KEY_ENTER: return Key::Enter;
        case GLFW_KEY_KP_ENTER: return Key::KpEnter;
        case GLFW_KEY_SPACE: return Key::Space;
        case GLFW_KEY_LEFT: return Key::Left;
        case GLFW_KEY_RIGHT: return Key::Right;
        case GLFW_KEY_UP: return Key::Up;
        case GLFW_KEY_DOWN: return Key::Down;
        case GLFW_KEY_PAGE_UP: return Key::PageUp;
        case GLFW_KEY_PAGE_DOWN: return Key::PageDown;
        case GLFW_KEY_HOME: return Key::Home;
        case GLFW_KEY_END: return Key::End;
        case GLFW_KEY_ESCAPE: return Key::Escape;
        case GLFW_KEY_A: return Key::A;
        default: return Key::Other;
    }
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

inline void glfw_update_cursor(GLFWwindow* window, bool pointing) {
    if (!window)
        return;
    static GLFWcursor* arrow = nullptr;
    static GLFWcursor* hand = nullptr;
    if (!arrow) arrow = glfwCreateStandardCursor(GLFW_ARROW_CURSOR);
    if (!hand) hand = glfwCreateStandardCursor(GLFW_POINTING_HAND_CURSOR);
    glfwSetCursor(window, pointing ? hand : arrow);
}

// Window currently bound to the active host. Captured so the static
// cursor callback (`glfw_set_hover_cursor`) can update the GLFW cursor
// for whichever window is currently installed.
inline GLFWwindow* g_active_glfw_window = nullptr;

inline void glfw_set_hover_cursor(bool pointing) {
    glfw_update_cursor(g_active_glfw_window, pointing);
}

inline void refresh_cached_canvas_size(::phenotype::native::native_host& host,
                                       GLFWwindow* window) {
    if (!window)
        return;
    int w = 0;
    int h = 0;
    glfwGetWindowSize(window, &w, &h);
    host.cached_width_px = w;
    host.cached_height_px = h;
}

// Backward-compatible int-typed dispatch adapters so test fixtures and any
// GLFW-centric callers can keep passing raw `GLFW_KEY_*` / `GLFW_PRESS`
// etc. The enum-typed versions in `phenotype.native.shell` are the
// primary API; these exist only so the refactor stays purely additive
// for existing call sites.
inline bool dispatch_mouse_button(float mx, float my,
                                   int button, int action, int mods) {
    return ::phenotype::native::detail::dispatch_mouse_button(
        mx, my,
        static_cast<MouseButton>(button),
        static_cast<KeyAction>(action),
        mods);
}

inline bool dispatch_key(int key, int action, int mods) {
    return ::phenotype::native::detail::dispatch_key(
        translate_glfw_key(key),
        static_cast<KeyAction>(action),
        mods);
}

inline void on_mouse_button(GLFWwindow* window, int button, int action, int mods) {
    double mx = 0.0;
    double my = 0.0;
    glfwGetCursorPos(window, &mx, &my);
    ::phenotype::native::detail::dispatch_mouse_button(
        static_cast<float>(mx),
        static_cast<float>(my),
        static_cast<MouseButton>(button),
        static_cast<KeyAction>(action),
        mods);
}

inline void on_cursor_pos(GLFWwindow* /*window*/, double mx, double my) {
    ::phenotype::native::detail::dispatch_cursor_pos(
        static_cast<float>(mx),
        static_cast<float>(my));
}

inline void on_scroll(GLFWwindow* window, double dx, double dy) {
    int w = 0;
    int h = 0;
    glfwGetWindowSize(window, &w, &h);
    float vw = (w > 0) ? static_cast<float>(w) : 1.0f;
    float vh = (h > 0) ? static_cast<float>(h) : 1.0f;
    ::phenotype::native::detail::dispatch_scroll_xy(dx, dy, vw, vh);
}

inline void on_framebuffer_size(GLFWwindow* /*window*/, int w, int h) {
    if (auto* host = ::phenotype::native::detail::active_host()) {
        host->cached_width_px = w;
        host->cached_height_px = h;
    }
    ::phenotype::native::detail::repaint_current();
}

inline void on_window_content_scale(GLFWwindow*, float, float) {
    ::phenotype::native::detail::repaint_current();
}

inline void on_key(GLFWwindow*, int key, int, int action, int mods) {
    ::phenotype::native::detail::dispatch_key(
        translate_glfw_key(key),
        static_cast<KeyAction>(action),
        mods);
}

inline void on_char(GLFWwindow*, unsigned int codepoint) {
    ::phenotype::native::detail::dispatch_char(codepoint);
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
    host.set_hover_cursor = &glfw_set_hover_cursor;
    g_active_glfw_window = window;
    refresh_cached_canvas_size(host, window);

    ::phenotype::native::detail::bind_host(host, 0.0f);
    install_callbacks(window);

    ::phenotype::native::detail::run_host<State, Msg>(host, std::move(view), std::move(update));

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        ::phenotype::native::detail::tick_caret_blink();
        ::phenotype::native::detail::sync_platform_input();
    }

    ::phenotype::native::detail::shutdown_host(host);
    g_active_glfw_window = nullptr;
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}

} // namespace detail

} // namespace phenotype::native
#endif
