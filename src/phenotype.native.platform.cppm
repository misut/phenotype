module;
#ifndef __wasi__
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

struct GLFWwindow;
#endif

export module phenotype.native.platform;

#ifndef __wasi__
export namespace phenotype::native {

struct TextQuad {
    float x, y, w, h;
    float u, v, uw, vh;
};

struct TextAtlas {
    std::vector<uint8_t> pixels;
    int width = 0;
    int height = 0;
    std::vector<TextQuad> quads;
};

struct TextEntry {
    float x, y, font_size;
    bool mono;
    float r, g, b, a;
    std::string text;
    float line_height = 0;
};

struct text_api {
    void (*init)() = nullptr;
    void (*shutdown)() = nullptr;
    float (*measure)(float font_size, bool mono,
                     char const* text, unsigned int len) = nullptr;
    TextAtlas (*build_atlas)(std::vector<TextEntry> const& entries,
                             float backing_scale) = nullptr;
};

struct renderer_api {
    void (*init)(GLFWwindow* window) = nullptr;
    void (*flush)(unsigned char const* buf, unsigned int len) = nullptr;
    void (*shutdown)() = nullptr;
    std::optional<unsigned int> (*hit_test)(float x, float y,
                                            float scroll_y) = nullptr;
};

struct platform_api {
    char const* name = "stub";
    bool enabled = true;
    text_api text{};
    renderer_api renderer{};
    void (*open_url)(char const* url, unsigned int len) = nullptr;
    char const* startup_message = nullptr;
};

inline constexpr unsigned int invalid_callback_id = 0xFFFFFFFFu;

} // namespace phenotype::native
#endif
