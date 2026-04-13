#include <cstdio>
#include <string>
#include <variant>

#include <GLFW/glfw3.h>

#include "renderer.h"

// Forward — defined in host.cpp
namespace native {
void set_window(GLFWwindow* w);
}

import phenotype;

// ---- Simple counter app (same as the docs example) ----

struct Increment {};
struct Decrement {};
using Msg = std::variant<Increment, Decrement>;

struct State {
    int count = 0;
};

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

// ---- Entry point ----

int main() {
    if (!glfwInit()) {
        std::fprintf(stderr, "Failed to initialize GLFW\n");
        return 1;
    }

    // No OpenGL context — Dawn provides the GPU surface
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

    auto* window = glfwCreateWindow(800, 600, "phenotype", nullptr, nullptr);
    if (!window) {
        std::fprintf(stderr, "Failed to create GLFW window\n");
        glfwTerminate();
        return 1;
    }

    native::set_window(window);
    native::renderer::init(window);

    // Install the phenotype app — this triggers the initial rebuild,
    // which calls view → layout → paint → phenotype_flush() → renderer.
    phenotype::run<State, Msg>(view, update);

    // Event loop — glfwPollEvents drives the window. phenotype rebuilds
    // happen in response to events (Phase D will wire GLFW callbacks
    // to phenotype_handle_event, phenotype_set_hover, etc.).
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
    }

    native::renderer::shutdown();
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
