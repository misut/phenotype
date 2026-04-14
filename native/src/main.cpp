#include <cstdio>
#include <string>
#include <variant>

#include <GLFW/glfw3.h>

#include "renderer.h"
#include "text.h"

namespace native {
void set_window(GLFWwindow* w);
}

import phenotype;

// Phenotype exports for event dispatch
extern "C" {
    void phenotype_handle_event(unsigned int callback_id);
    void phenotype_set_hover(unsigned int callback_id);
    void phenotype_set_focus(unsigned int callback_id);
    void phenotype_handle_tab(unsigned int direction);
    void phenotype_repaint(float scroll_y);
    unsigned int phenotype_get_focused_id(void);
    float phenotype_get_total_height(void);
}

// ---- Simple counter app ----

struct Increment {};
struct Decrement {};
using Msg = std::variant<Increment, Decrement>;

struct State { int count = 0; };

void update(State& state, Msg msg) {
    std::visit([&](auto const& m) {
        using T = std::decay_t<decltype(m)>;
        if constexpr (std::same_as<T, Increment>) state.count += 1;
        else if constexpr (std::same_as<T, Decrement>) state.count -= 1;
    }, msg);
}

void view(State const& state) {
    using namespace phenotype;
    layout::column([&] {
        widget::text("phenotype — native (Dawn/Metal)");
        widget::text(std::string("Count: ") + std::to_string(state.count));
        layout::row(
            [&] { widget::button<Msg>("-", Decrement{}); },
            [&] { widget::button<Msg>("+", Increment{}); }
        );
    });
}

// ---- Event state ----

static float g_scroll_y = 0;
static unsigned int g_hovered_id = 0xFFFFFFFF;

// ---- GLFW callbacks ----

static void on_mouse_button(GLFWwindow* window, int button, int action, int /*mods*/) {
    if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS) {
        double mx, my;
        glfwGetCursorPos(window, &mx, &my);
        auto hit = native::renderer::hit_test(
            static_cast<float>(mx), static_cast<float>(my), g_scroll_y);
        if (hit) {
            phenotype_set_focus(*hit);
            phenotype_handle_event(*hit);
        }
    }
}

static void on_cursor_pos(GLFWwindow* window, double mx, double my) {
    auto hit = native::renderer::hit_test(
        static_cast<float>(mx), static_cast<float>(my), g_scroll_y);
    unsigned int new_hover = hit.value_or(0xFFFFFFFF);
    if (new_hover != g_hovered_id) {
        g_hovered_id = new_hover;
        phenotype_set_hover(new_hover);
    }
    // Cursor shape
    static GLFWcursor* arrow = nullptr;
    static GLFWcursor* hand = nullptr;
    if (!arrow) arrow = glfwCreateStandardCursor(GLFW_ARROW_CURSOR);
    if (!hand) hand = glfwCreateStandardCursor(GLFW_POINTING_HAND_CURSOR);
    glfwSetCursor(window, hit ? hand : arrow);
}

static void on_scroll(GLFWwindow* window, double /*dx*/, double dy) {
    float total = phenotype_get_total_height();
    int w, h;
    glfwGetWindowSize(window, &w, &h);
    float max_scroll = total - static_cast<float>(h);
    if (max_scroll < 0) max_scroll = 0;
    g_scroll_y += static_cast<float>(dy);
    if (g_scroll_y < 0) g_scroll_y = 0;
    if (g_scroll_y > max_scroll) g_scroll_y = max_scroll;
    phenotype_repaint(g_scroll_y);
}

static void on_framebuffer_size(GLFWwindow*, int /*w*/, int /*h*/) {
    phenotype_repaint(g_scroll_y);
}

static void on_key(GLFWwindow*, int key, int /*scancode*/, int action, int mods) {
    if (action != GLFW_PRESS && action != GLFW_REPEAT) return;
    if (key == GLFW_KEY_TAB) {
        phenotype_handle_tab((mods & GLFW_MOD_SHIFT) ? 1 : 0);
    } else if (key == GLFW_KEY_ENTER) {
        unsigned int fid = phenotype_get_focused_id();
        if (fid != 0xFFFFFFFF)
            phenotype_handle_event(fid);
    }
}

// ---- Entry point ----

int main() {
    if (!glfwInit()) {
        std::fprintf(stderr, "Failed to initialize GLFW\n");
        return 1;
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

    auto* window = glfwCreateWindow(800, 600, "phenotype", nullptr, nullptr);
    if (!window) {
        std::fprintf(stderr, "Failed to create GLFW window\n");
        glfwTerminate();
        return 1;
    }

    // Init subsystems
    native::set_window(window);
    native::text::init();
    native::renderer::init(window);

    // Register GLFW event callbacks
    glfwSetMouseButtonCallback(window, on_mouse_button);
    glfwSetCursorPosCallback(window, on_cursor_pos);
    glfwSetScrollCallback(window, on_scroll);
    glfwSetFramebufferSizeCallback(window, on_framebuffer_size);
    glfwSetKeyCallback(window, on_key);

    // Install phenotype app (triggers initial rebuild + render)
    phenotype::run<State, Msg>(view, update);

    // Event loop
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
    }

    native::renderer::shutdown();
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
