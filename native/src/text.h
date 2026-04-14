#pragma once

#include <string>
#include <vector>
#include <cstdint>

namespace native::text {

struct TextQuad {
    float x, y, w, h;   // screen rect (logical pixels)
    float u, v, uw, vh;  // UV rect in atlas [0,1]
};

struct TextAtlas {
    std::vector<uint8_t> pixels; // RGBA
    int width = 0, height = 0;
    std::vector<TextQuad> quads;
};

void init();
float measure(float font_size, bool mono, char const* text, unsigned int len);

struct TextEntry {
    float x, y, font_size;
    bool mono;
    float r, g, b, a; // color
    std::string text;
};

TextAtlas build_atlas(std::vector<TextEntry> const& entries);

} // namespace native::text
