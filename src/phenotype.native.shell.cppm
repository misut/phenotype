module;
#ifndef __wasi__
#include <concepts>
#include <cstdio>
#include <optional>
#include <utility>

#include <GLFW/glfw3.h>
#endif

export module phenotype.native.shell;

#ifndef __wasi__
import phenotype;
import phenotype.native.platform;

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

inline std::optional<unsigned int> hit_test(float x, float y, float scroll_y) {
    if (!g_app_state.host || !g_app_state.host->platform
        || !g_app_state.host->platform->renderer.hit_test)
        return std::nullopt;
    return g_app_state.host->platform->renderer.hit_test(x, y, scroll_y);
}

inline void repaint_current() {
    if (g_app_state.host) {
        ::phenotype::detail::repaint(*g_app_state.host, g_app_state.scroll_y);
        sync_platform_input();
    }
}

inline void on_mouse_button(GLFWwindow* window, int button, int action, int) {
    double mx = 0.0;
    double my = 0.0;
    glfwGetCursorPos(window, &mx, &my);
    if (g_app_state.host && g_app_state.host->platform
        && g_app_state.host->platform->input.handle_mouse_button
        && g_app_state.host->platform->input.handle_mouse_button(
            static_cast<float>(mx),
            static_cast<float>(my),
            button,
            action,
            0)) {
        repaint_current();
        return;
    }
    if (button != GLFW_MOUSE_BUTTON_LEFT || action != GLFW_PRESS) return;
    if (auto h = hit_test(static_cast<float>(mx), static_cast<float>(my),
                          g_app_state.scroll_y)) {
        ::phenotype::detail::set_focus_id(*h);
        ::phenotype::detail::handle_event(*h);
        repaint_current();
    } else {
        ::phenotype::detail::set_focus_id(invalid_callback_id);
        repaint_current();
    }
}

inline void on_cursor_pos(GLFWwindow* window, double mx, double my) {
    if (g_app_state.host && g_app_state.host->platform
        && g_app_state.host->platform->input.handle_cursor_pos
        && g_app_state.host->platform->input.handle_cursor_pos(
            static_cast<float>(mx),
            static_cast<float>(my))) {
        g_app_state.hovered_id = invalid_callback_id;
        if (::phenotype::detail::set_hover_id(invalid_callback_id))
            repaint_current();
        static GLFWcursor* arrow = nullptr;
        if (!arrow) arrow = glfwCreateStandardCursor(GLFW_ARROW_CURSOR);
        glfwSetCursor(window, arrow);
        return;
    }
    auto h = hit_test(static_cast<float>(mx), static_cast<float>(my),
                      g_app_state.scroll_y);
    unsigned int id = h.value_or(invalid_callback_id);
    if (id != g_app_state.hovered_id) {
        g_app_state.hovered_id = id;
        if (::phenotype::detail::set_hover_id(id))
            repaint_current();
    }

    static GLFWcursor* arrow = nullptr;
    static GLFWcursor* hand = nullptr;
    if (!arrow) arrow = glfwCreateStandardCursor(GLFW_ARROW_CURSOR);
    if (!hand) hand = glfwCreateStandardCursor(GLFW_POINTING_HAND_CURSOR);
    glfwSetCursor(window, h ? hand : arrow);
}

inline void on_scroll(GLFWwindow* window, double, double dy) {
    float total = ::phenotype::detail::get_total_height();
    int w = 0;
    int h = 0;
    glfwGetWindowSize(window, &w, &h);
    float max_scroll = total - static_cast<float>(h);
    if (max_scroll < 0) max_scroll = 0;
    g_app_state.scroll_y += static_cast<float>(dy);
    if (g_app_state.scroll_y < 0) g_app_state.scroll_y = 0;
    if (g_app_state.scroll_y > max_scroll) g_app_state.scroll_y = max_scroll;
    repaint_current();
}

inline void on_framebuffer_size(GLFWwindow*, int, int) {
    repaint_current();
}

inline void on_key(GLFWwindow*, int key, int, int action, int mods) {
    if (action != GLFW_PRESS && action != GLFW_REPEAT) return;
    if (key == GLFW_KEY_TAB) {
        ::phenotype::detail::handle_tab((mods & GLFW_MOD_SHIFT) ? 1 : 0);
        repaint_current();
        return;
    }
    if (key == GLFW_KEY_BACKSPACE) {
        ::phenotype::detail::handle_key(1, 0);
        repaint_current();
        return;
    }
    if (key == GLFW_KEY_ENTER) {
        unsigned int focused_id = ::phenotype::detail::get_focused_id();
        if (focused_id != invalid_callback_id) {
            ::phenotype::detail::handle_event(focused_id);
            repaint_current();
        }
    }
}

inline void on_char(GLFWwindow*, unsigned int codepoint) {
    ::phenotype::detail::handle_key(0, codepoint);
    if (::phenotype::detail::focused_is_input())
        repaint_current();
}

inline void install_callbacks(GLFWwindow* window) {
    glfwSetMouseButtonCallback(window, on_mouse_button);
    glfwSetCursorPosCallback(window, on_cursor_pos);
    glfwSetScrollCallback(window, on_scroll);
    glfwSetFramebufferSizeCallback(window, on_framebuffer_size);
    glfwSetKeyCallback(window, on_key);
    glfwSetCharCallback(window, on_char);
}

} // namespace detail

namespace detail {

template<typename State, typename Msg, typename View, typename Update>
    requires std::invocable<View, State const&>
          && std::invocable<Update, State&, Msg>
void run_host(native_host& host, View view, Update update) {
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

    auto* window = glfwCreateWindow(width, height, title, nullptr, nullptr);
    if (!window) {
        std::fprintf(stderr, "Failed to create GLFW window\n");
        glfwTerminate();
        return 1;
    }

    native_host host;
    host.window = window;
    host.platform = &platform;
    detail::g_app_state = {&host, 0.0f, invalid_callback_id};
    detail::install_callbacks(window);

    run_host<State, Msg>(host, std::move(view), std::move(update));

    while (!glfwWindowShouldClose(window))
        glfwPollEvents();

    shutdown_host(host);
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}

} // namespace detail

} // namespace phenotype::native
#endif
