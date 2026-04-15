// Native backend — stub (renderer not yet implemented).
// On WASM targets this compiles as an empty module.

module;
#ifndef __wasi__
#include <cstdint>
#include <cstdio>
#include <optional>
#include <string>
#include <variant>
#include <vector>

#include <GLFW/glfw3.h>
#endif // !__wasi__

export module phenotype.native;

#ifndef __wasi__
export import phenotype;

// ============================================================
// Text subsystem — stub (no-op until platform backend lands)
// ============================================================

namespace phenotype::native::text {

struct TextQuad {
    float x, y, w, h;
    float u, v, uw, vh;
};

struct TextAtlas {
    std::vector<uint8_t> pixels;
    int width = 0, height = 0;
    std::vector<TextQuad> quads;
};

struct TextEntry {
    float x, y, font_size;
    bool mono;
    float r, g, b, a;
    std::string text;
};

inline void init() {}

inline float measure(float, bool, char const*, unsigned int) { return 0.f; }

inline TextAtlas build_atlas(std::vector<TextEntry> const&) { return {}; }

} // namespace phenotype::native::text

// ============================================================
// Renderer — stub (no-op until platform backend lands)
// ============================================================

namespace phenotype::native::renderer {

static std::vector<HitRegionCmd> g_hit_regions;

inline void init(GLFWwindow*) {}

inline void flush(unsigned char const* buf, unsigned int len) {
    if (len == 0) return;
    auto cmds = parse_commands(buf, len);
    g_hit_regions.clear();
    for (auto& cmd : cmds) {
        if (auto* hr = std::get_if<HitRegionCmd>(&cmd))
            g_hit_regions.push_back(*hr);
    }
}

inline void shutdown() { g_hit_regions.clear(); }

inline std::optional<unsigned int> hit_test(float x, float y, float scroll_y) {
    float wy = y + scroll_y;
    for (int i = static_cast<int>(g_hit_regions.size()) - 1; i >= 0; --i) {
        auto& hr = g_hit_regions[i];
        if (x >= hr.x && x <= hr.x + hr.w && wy >= hr.y && wy <= hr.y + hr.h)
            return hr.callback_id;
    }
    return std::nullopt;
}

} // namespace phenotype::native::renderer

// ============================================================
// native_host — satisfies host_platform concept
// ============================================================

export namespace phenotype::native {

struct native_host {
    GLFWwindow* window = nullptr;

    // text_measurer
    float measure_text(float font_size, unsigned int mono,
                       char const* t, unsigned int len) const {
        return text::measure(font_size, mono != 0, t, len);
    }

    // render_backend
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
        if (len_ > 0) {
            renderer::flush(buffer, len_);
            len_ = 0;
        }
    }

    // canvas_source
    float canvas_width() const {
        if (!window) return 800.0f;
        int w, h;
        glfwGetWindowSize(window, &w, &h);
        return static_cast<float>(w);
    }
    float canvas_height() const {
        if (!window) return 600.0f;
        int w, h;
        glfwGetWindowSize(window, &w, &h);
        return static_cast<float>(h);
    }

    // url_opener
    void open_url(char const*, unsigned int) {
        // TODO: system shell (open / xdg-open / ShellExecute)
    }
};

static_assert(host_platform<native_host>);

// Hit test — delegates to the renderer's stored hit regions.
inline std::optional<unsigned int> hit_test(float x, float y, float scroll_y) {
    return renderer::hit_test(x, y, scroll_y);
}

// Convenience runner — initializes subsystems and calls phenotype::run.
template<typename State, typename Msg, typename View, typename Update>
    requires std::invocable<View, State const&>
          && std::invocable<Update, State&, Msg>
void run(native_host& host, View view, Update update) {
    detail::g_open_url = [](char const* url, unsigned int len) {
        // TODO: system shell
        (void)url; (void)len;
    };
    text::init();
    renderer::init(host.window);
    phenotype::run<State, Msg>(host, std::move(view), std::move(update));
}

inline void shutdown() {
    renderer::shutdown();
}

} // namespace phenotype::native

#endif // !__wasi__
