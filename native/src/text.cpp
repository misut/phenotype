#define STB_TRUETYPE_IMPLEMENTATION
#include "../third_party/stb_truetype.h"

#include "text.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <fstream>
#include <vector>

namespace native::text {

static constexpr int ATLAS_SIZE = 2048;

static std::vector<unsigned char> g_sans_font_data;
static std::vector<unsigned char> g_mono_font_data;
static stbtt_fontinfo g_sans_info;
static stbtt_fontinfo g_mono_info;
static bool g_initialized = false;

static std::vector<unsigned char> load_file(char const* path) {
    auto f = std::ifstream(path, std::ios::binary | std::ios::ate);
    if (!f) return {};
    auto size = f.tellg();
    f.seekg(0);
    std::vector<unsigned char> data(static_cast<size_t>(size));
    f.read(reinterpret_cast<char*>(data.data()), size);
    return data;
}

void init() {
    if (g_initialized) return;

    // macOS system fonts
    g_sans_font_data = load_file("/System/Library/Fonts/SFNS.ttf");
    g_mono_font_data = load_file("/System/Library/Fonts/SFNSMono.ttf");

    // Fallbacks
    if (g_sans_font_data.empty())
        g_sans_font_data = load_file("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf");
    if (g_mono_font_data.empty())
        g_mono_font_data = g_sans_font_data;

    if (g_sans_font_data.empty()) {
        std::fprintf(stderr, "[text] no font found\n");
        return;
    }

    stbtt_InitFont(&g_sans_info, g_sans_font_data.data(),
        stbtt_GetFontOffsetForIndex(g_sans_font_data.data(), 0));
    stbtt_InitFont(&g_mono_info, g_mono_font_data.data(),
        stbtt_GetFontOffsetForIndex(g_mono_font_data.data(), 0));

    g_initialized = true;
}

float measure(float font_size, bool mono, char const* text, unsigned int len) {
    if (!g_initialized || len == 0) return 0;

    auto& info = mono ? g_mono_info : g_sans_info;
    float scale = stbtt_ScaleForPixelHeight(&info, font_size);

    float width = 0;
    for (unsigned int i = 0; i < len; ) {
        int codepoint;
        // Simple UTF-8 decode (handle ASCII + common multi-byte)
        unsigned char c = static_cast<unsigned char>(text[i]);
        if (c < 0x80) { codepoint = c; i += 1; }
        else if (c < 0xE0) { codepoint = (c & 0x1F) << 6; if (i+1<len) codepoint |= (text[i+1] & 0x3F); i += 2; }
        else if (c < 0xF0) { codepoint = (c & 0x0F) << 12; if (i+1<len) codepoint |= (text[i+1] & 0x3F) << 6; if (i+2<len) codepoint |= (text[i+2] & 0x3F); i += 3; }
        else { codepoint = (c & 0x07) << 18; if (i+1<len) codepoint |= (text[i+1] & 0x3F) << 12; if (i+2<len) codepoint |= (text[i+2] & 0x3F) << 6; if (i+3<len) codepoint |= (text[i+3] & 0x3F); i += 4; }

        int advance, lsb;
        stbtt_GetCodepointHMetrics(&info, codepoint, &advance, &lsb);
        width += advance * scale;

        // Kerning
        if (i < len) {
            unsigned char nc = static_cast<unsigned char>(text[i]);
            int next_cp = (nc < 0x80) ? nc : '?';
            width += stbtt_GetCodepointKernAdvance(&info, codepoint, next_cp) * scale;
        }
    }
    return width;
}

TextAtlas build_atlas(std::vector<TextEntry> const& entries) {
    TextAtlas atlas;
    atlas.width = ATLAS_SIZE;
    atlas.height = ATLAS_SIZE;
    atlas.pixels.resize(ATLAS_SIZE * ATLAS_SIZE * 4, 0); // RGBA

    int ax = 0, ay = 0, row_height = 0;
    int const padding = 2;

    for (auto const& e : entries) {
        if (e.text.empty()) continue;

        auto& info = e.mono ? g_mono_info : g_sans_info;
        float scale = stbtt_ScaleForPixelHeight(&info, e.font_size);

        int ascent, descent, line_gap;
        stbtt_GetFontVMetrics(&info, &ascent, &descent, &line_gap);
        float scaled_ascent = ascent * scale;

        // Measure text width for this entry
        float tw_f = measure(e.font_size, e.mono, e.text.c_str(),
            static_cast<unsigned int>(e.text.size()));
        int tw = static_cast<int>(std::ceil(tw_f)) + padding * 2;
        int th = static_cast<int>(std::ceil(e.font_size * 1.4f)) + padding * 2;

        // Row wrap
        if (ax + tw > ATLAS_SIZE) {
            ax = 0;
            ay += row_height;
            row_height = 0;
        }
        if (ay + th > ATLAS_SIZE) break; // atlas full

        // Rasterize each glyph into the atlas
        float pen_x = static_cast<float>(ax + padding);
        for (unsigned int i = 0; i < e.text.size(); ) {
            unsigned char c = static_cast<unsigned char>(e.text[i]);
            int codepoint;
            if (c < 0x80) { codepoint = c; i += 1; }
            else if (c < 0xE0) { codepoint = (c & 0x1F) << 6; if (i+1<e.text.size()) codepoint |= (e.text[i+1] & 0x3F); i += 2; }
            else if (c < 0xF0) { codepoint = (c & 0x0F) << 12; if (i+1<e.text.size()) codepoint |= (e.text[i+1] & 0x3F) << 6; if (i+2<e.text.size()) codepoint |= (e.text[i+2] & 0x3F); i += 3; }
            else { codepoint = '?'; i += 4; }

            int gw, gh, xoff, yoff;
            unsigned char* bitmap = stbtt_GetCodepointBitmap(
                &info, scale, scale, codepoint, &gw, &gh, &xoff, &yoff);

            int gx = static_cast<int>(pen_x) + xoff;
            int gy = ay + padding + static_cast<int>(scaled_ascent) + yoff;

            if (bitmap) {
                for (int row = 0; row < gh; ++row) {
                    for (int col = 0; col < gw; ++col) {
                        int px = gx + col;
                        int py = gy + row;
                        if (px >= 0 && px < ATLAS_SIZE && py >= 0 && py < ATLAS_SIZE) {
                            int idx = (py * ATLAS_SIZE + px) * 4;
                            unsigned char alpha = bitmap[row * gw + col];
                            // Premultiplied alpha with text color
                            atlas.pixels[idx + 0] = static_cast<uint8_t>(e.r * 255 * alpha / 255);
                            atlas.pixels[idx + 1] = static_cast<uint8_t>(e.g * 255 * alpha / 255);
                            atlas.pixels[idx + 2] = static_cast<uint8_t>(e.b * 255 * alpha / 255);
                            atlas.pixels[idx + 3] = alpha;
                        }
                    }
                }
                stbtt_FreeBitmap(bitmap, nullptr);
            }

            int advance, lsb;
            stbtt_GetCodepointHMetrics(&info, codepoint, &advance, &lsb);
            pen_x += advance * scale;
        }

        // Record quad
        atlas.quads.push_back({
            e.x, e.y,
            (tw_f), (static_cast<float>(th - padding * 2)),
            static_cast<float>(ax) / ATLAS_SIZE,
            static_cast<float>(ay) / ATLAS_SIZE,
            static_cast<float>(tw) / ATLAS_SIZE,
            static_cast<float>(th) / ATLAS_SIZE,
        });

        ax += tw;
        if (th > row_height) row_height = th;
    }

    return atlas;
}

} // namespace native::text
