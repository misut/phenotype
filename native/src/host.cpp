#include "renderer.h"
#include <GLFW/glfw3.h>

// Global window pointer — set by main before phenotype::run().
static GLFWwindow* g_window = nullptr;

namespace native {
void set_window(GLFWwindow* w) { g_window = w; }
GLFWwindow* get_window() { return g_window; }
}

// Implement the 5 host functions declared in phenotype_host.h.
// On native, these are regular C functions resolved at link time.

extern "C" void phenotype_flush() {
    native::renderer::flush();
}

extern "C" float phenotype_measure_text(float font_size, unsigned int /*mono*/,
                                         char const* /*text*/, unsigned int len) {
    // Phase A stub: fixed-width approximation (same as test stubs).
    // Phase C will replace this with real font metrics.
    return static_cast<float>(len) * font_size * 0.6f;
}

extern "C" float phenotype_get_canvas_width() {
    if (!g_window) return 800.0f;
    int w, h;
    glfwGetWindowSize(g_window, &w, &h);
    return static_cast<float>(w);
}

extern "C" float phenotype_get_canvas_height() {
    if (!g_window) return 600.0f;
    int w, h;
    glfwGetWindowSize(g_window, &w, &h);
    return static_cast<float>(h);
}

extern "C" void phenotype_open_url(char const* /*url*/, unsigned int /*len*/) {
    // Phase D will implement via system shell (open / xdg-open / ShellExecute)
}
