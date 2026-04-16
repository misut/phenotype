module;
#ifndef __wasi__
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <limits>
#include <map>
#include <optional>
#include <string>
#include <variant>
#include <vector>

#include <GLFW/glfw3.h>

#ifdef __APPLE__
#define GLFW_EXPOSE_NATIVE_COCOA
#include <GLFW/glfw3native.h>

#define NS_PRIVATE_IMPLEMENTATION
#define MTL_PRIVATE_IMPLEMENTATION
#define CA_PRIVATE_IMPLEMENTATION
#include <Foundation/Foundation.hpp>
#include <Metal/Metal.hpp>
#include <QuartzCore/QuartzCore.hpp>

#include <CoreText/CoreText.h>
#include <CoreGraphics/CoreGraphics.h>
#include <objc/message.h>
#endif
#endif

export module phenotype.native.macos;

#ifndef __wasi__
import phenotype.commands;
import phenotype.diag;
import phenotype.state;
import phenotype.native.platform;
import phenotype.types;

#ifdef __APPLE__
namespace phenotype::native::detail {

template<typename T>
struct CFGuard {
    T ref;
    explicit CFGuard(T r) : ref(r) {}
    ~CFGuard() { if (ref) CFRelease(ref); }
    CFGuard(CFGuard const&) = delete;
    CFGuard& operator=(CFGuard const&) = delete;
    operator T() const { return ref; }
    explicit operator bool() const { return ref != nullptr; }
};

struct TextState {
    CTFontRef sans = nullptr;
    CTFontRef mono = nullptr;
    bool initialized = false;
};

inline TextState g_text;

inline float sanitize_scale(float scale) {
    return (scale > 0.0f && std::isfinite(scale)) ? scale : 1.0f;
}

inline int logical_pixels(float logical, float scale, int minimum = 1) {
    int px = static_cast<int>(std::ceil(logical * scale));
    return (px < minimum) ? minimum : px;
}

inline float snap_to_pixel_grid(float value, float scale) {
    float s = sanitize_scale(scale);
    return std::round(value * s) / s;
}

struct LineBoxMetrics {
    int slot_width = 0;
    int slot_height = 0;
    int render_top = 0;
    int render_height = 0;
    float baseline_y = 0;
};

inline LineBoxMetrics make_line_box(float logical_width,
                                    float logical_line_height,
                                    float ascent, float descent, float leading,
                                    float scale, int padding) {
    LineBoxMetrics box;
    float natural_line_height = ascent + descent + leading;
    int text_width_px = logical_pixels(logical_width, scale, 0);
    int line_height_px = logical_pixels(logical_line_height, scale);
    int natural_height_px = static_cast<int>(std::ceil(natural_line_height));
    if (natural_height_px < 1) natural_height_px = 1;

    box.slot_width = text_width_px + padding * 2;
    if (box.slot_width < padding * 2 + 1)
        box.slot_width = padding * 2 + 1;

    box.render_top = padding;
    box.render_height = (line_height_px > natural_height_px)
        ? line_height_px : natural_height_px;
    box.slot_height = box.render_height + padding * 2;

    float extra_leading = static_cast<float>(box.render_height) - natural_line_height;
    if (extra_leading < 0.0f) extra_leading = 0.0f;
    box.baseline_y = static_cast<float>(box.render_top)
        + extra_leading * 0.5f + ascent;
    return box;
}

inline void text_init() {
    if (g_text.initialized) return;
    g_text.sans = CTFontCreateUIFontForLanguage(kCTFontUIFontSystem, 16.0, nullptr);
    g_text.mono = CTFontCreateUIFontForLanguage(kCTFontUIFontUserFixedPitch, 16.0, nullptr);
    if (!g_text.sans) {
        std::fprintf(stderr, "[text] failed to create system font\n");
        return;
    }
    if (!g_text.mono)
        g_text.mono = static_cast<CTFontRef>(CFRetain(g_text.sans));
    g_text.initialized = true;
}

inline void text_shutdown() {
    if (g_text.sans) { CFRelease(g_text.sans); g_text.sans = nullptr; }
    if (g_text.mono) { CFRelease(g_text.mono); g_text.mono = nullptr; }
    g_text.initialized = false;
}

inline unsigned int decode_utf8(char const* src, unsigned int byte_len,
                                std::vector<uint32_t>& out) {
    out.clear();
    for (unsigned int i = 0; i < byte_len; ) {
        uint32_t cp = 0;
        auto c = static_cast<unsigned char>(src[i]);
        if (c < 0x80) {
            cp = c;
            i += 1;
        } else if (c < 0xE0) {
            cp = (c & 0x1F) << 6;
            if (i + 1 < byte_len) cp |= (src[i + 1] & 0x3F);
            i += 2;
        } else if (c < 0xF0) {
            cp = (c & 0x0F) << 12;
            if (i + 1 < byte_len) cp |= (src[i + 1] & 0x3F) << 6;
            if (i + 2 < byte_len) cp |= (src[i + 2] & 0x3F);
            i += 3;
        } else {
            cp = (c & 0x07) << 18;
            if (i + 1 < byte_len) cp |= (src[i + 1] & 0x3F) << 12;
            if (i + 2 < byte_len) cp |= (src[i + 2] & 0x3F) << 6;
            if (i + 3 < byte_len) cp |= (src[i + 3] & 0x3F);
            i += 4;
        }
        out.push_back(cp);
    }
    return static_cast<unsigned int>(out.size());
}

inline void codepoints_to_unichars(std::vector<uint32_t> const& cps,
                                   std::vector<UniChar>& out) {
    out.clear();
    out.reserve(cps.size());
    for (auto cp : cps) {
        if (cp <= 0xFFFF) {
            out.push_back(static_cast<UniChar>(cp));
            continue;
        }
        cp -= 0x10000;
        out.push_back(static_cast<UniChar>(0xD800 + (cp >> 10)));
        out.push_back(static_cast<UniChar>(0xDC00 + (cp & 0x3FF)));
    }
}

inline float text_measure(float font_size, bool mono,
                          char const* text_ptr, unsigned int len) {
    if (!g_text.initialized || len == 0) return 0.0f;

    CTFontRef base = mono ? g_text.mono : g_text.sans;
    CFGuard<CTFontRef> font(
        CTFontCreateCopyWithAttributes(base, font_size, nullptr, nullptr));
    if (!font) return 0.0f;

    static thread_local std::vector<uint32_t> codepoints;
    static thread_local std::vector<UniChar> unichars;
    decode_utf8(text_ptr, len, codepoints);
    codepoints_to_unichars(codepoints, unichars);

    auto count = static_cast<CFIndex>(unichars.size());
    std::vector<CGGlyph> glyphs(static_cast<std::size_t>(count));
    CTFontGetGlyphsForCharacters(font, unichars.data(), glyphs.data(), count);

    std::vector<CGSize> advances(static_cast<std::size_t>(count));
    CTFontGetAdvancesForGlyphs(font, kCTFontOrientationHorizontal,
                               glyphs.data(), advances.data(), count);

    double width = 0.0;
    for (CFIndex i = 0; i < count; ++i)
        width += advances[static_cast<std::size_t>(i)].width;
    return static_cast<float>(width);
}

inline TextAtlas text_build_atlas(std::vector<TextEntry> const& entries,
                                  float backing_scale) {
    if (entries.empty()) return {};

    float scale = sanitize_scale(backing_scale);
    static constexpr int ATLAS_SIZE = 2048;
    TextAtlas atlas;
    atlas.width = ATLAS_SIZE;
    atlas.height = ATLAS_SIZE;
    atlas.pixels.resize(static_cast<std::size_t>(ATLAS_SIZE * ATLAS_SIZE * 4), 0);

    int ax = 0;
    int ay = 0;
    int row_height = 0;
    int padding = static_cast<int>(std::ceil(scale)) + 1;
    if (padding < 2) padding = 2;

    static thread_local std::vector<uint32_t> codepoints;
    static thread_local std::vector<UniChar> unichars;

    for (auto const& entry : entries) {
        if (entry.text.empty()) continue;

        CTFontRef base = entry.mono ? g_text.mono : g_text.sans;
        CFGuard<CTFontRef> font(
            CTFontCreateCopyWithAttributes(base, entry.font_size * scale,
                                           nullptr, nullptr));
        if (!font) continue;

        float ascent = static_cast<float>(CTFontGetAscent(font));
        float descent = static_cast<float>(CTFontGetDescent(font));
        float leading = static_cast<float>(CTFontGetLeading(font));
        float logical_line_height = entry.line_height > 0
            ? entry.line_height
            : entry.font_size * 1.6f;
        float snapped_x = snap_to_pixel_grid(entry.x, scale);
        float snapped_y = snap_to_pixel_grid(entry.y, scale);

        float width = text_measure(entry.font_size, entry.mono,
                                   entry.text.c_str(),
                                   static_cast<unsigned int>(entry.text.size()));
        auto box = make_line_box(width, logical_line_height, ascent, descent,
                                 leading, scale, padding);

        if (ax + box.slot_width > ATLAS_SIZE) {
            ax = 0;
            ay += row_height;
            row_height = 0;
        }
        if (ay + box.slot_height > ATLAS_SIZE) break;

        decode_utf8(entry.text.c_str(),
                    static_cast<unsigned int>(entry.text.size()),
                    codepoints);
        codepoints_to_unichars(codepoints, unichars);
        auto count = static_cast<CFIndex>(unichars.size());
        std::vector<CGGlyph> glyphs(static_cast<std::size_t>(count));
        CTFontGetGlyphsForCharacters(font, unichars.data(), glyphs.data(), count);

        std::vector<CGSize> advances(static_cast<std::size_t>(count));
        CTFontGetAdvancesForGlyphs(font, kCTFontOrientationHorizontal,
                                   glyphs.data(), advances.data(), count);

        bool has_ink = false;
        int ink_min_x = box.slot_width;
        int ink_max_x = -1;
        float pen_x = static_cast<float>(padding);

        for (CFIndex gi = 0; gi < count; ++gi) {
            auto glyph_index = static_cast<std::size_t>(gi);
            if (glyphs[glyph_index] == 0) {
                pen_x += static_cast<float>(advances[glyph_index].width);
                continue;
            }

            CGRect bbox;
            CTFontGetBoundingRectsForGlyphs(font, kCTFontOrientationHorizontal,
                                            &glyphs[glyph_index], &bbox, 1);
            int gw = static_cast<int>(std::ceil(bbox.size.width)) + 2;
            int gh = static_cast<int>(std::ceil(bbox.size.height)) + 2;
            if (gw <= 0 || gh <= 0) {
                pen_x += static_cast<float>(advances[glyph_index].width);
                continue;
            }

            std::vector<uint8_t> glyph_buf(static_cast<std::size_t>(gw * gh), 0);
            auto* ctx = CGBitmapContextCreate(
                glyph_buf.data(), gw, gh, 8, gw, nullptr, kCGImageAlphaOnly);
            if (!ctx) {
                pen_x += static_cast<float>(advances[glyph_index].width);
                continue;
            }

            CGPoint pos{
                .x = static_cast<CGFloat>(-bbox.origin.x + 1),
                .y = static_cast<CGFloat>(-bbox.origin.y + 1),
            };
            CTFontDrawGlyphs(font, &glyphs[glyph_index], &pos, 1, ctx);
            CGContextRelease(ctx);

            int gx = static_cast<int>(std::lround(pen_x + bbox.origin.x));
            int gy = static_cast<int>(std::lround(
                box.baseline_y - bbox.origin.y - bbox.size.height));

            for (int row = 0; row < gh; ++row) {
                for (int col = 0; col < gw; ++col) {
                    int local_x = gx + col;
                    int local_y = gy + row;
                    if (local_x < 0 || local_x >= box.slot_width
                        || local_y < 0 || local_y >= box.slot_height)
                        continue;

                    int px = ax + local_x;
                    int py = ay + local_y;
                    if (px < 0 || px >= ATLAS_SIZE || py < 0 || py >= ATLAS_SIZE)
                        continue;

                    uint8_t alpha = glyph_buf[static_cast<std::size_t>(row * gw + col)];
                    if (alpha == 0) continue;
                    has_ink = true;
                    if (local_x < ink_min_x) ink_min_x = local_x;
                    if (local_x > ink_max_x) ink_max_x = local_x;

                    auto idx = static_cast<std::size_t>((py * ATLAS_SIZE + px) * 4);
                    atlas.pixels[idx + 0] = static_cast<uint8_t>(entry.r * 255.0f * alpha / 255.0f);
                    atlas.pixels[idx + 1] = static_cast<uint8_t>(entry.g * 255.0f * alpha / 255.0f);
                    atlas.pixels[idx + 2] = static_cast<uint8_t>(entry.b * 255.0f * alpha / 255.0f);
                    atlas.pixels[idx + 3] = alpha;
                }
            }

            pen_x += static_cast<float>(advances[glyph_index].width);
        }

        if (has_ink) {
            int ink_w = ink_max_x - ink_min_x + 1;
            atlas.quads.push_back({
                snapped_x + static_cast<float>(ink_min_x) / scale,
                snapped_y,
                static_cast<float>(ink_w) / scale,
                static_cast<float>(box.render_height) / scale,
                static_cast<float>(ax + ink_min_x) / ATLAS_SIZE,
                static_cast<float>(ay + box.render_top) / ATLAS_SIZE,
                static_cast<float>(ink_w) / ATLAS_SIZE,
                static_cast<float>(box.render_height) / ATLAS_SIZE,
            });
        }

        ax += box.slot_width;
        if (box.slot_height > row_height) row_height = box.slot_height;
    }

    return atlas;
}

struct ColorInstanceGPU {
    float rect[4]{};
    float color[4]{};
    float params[4]{};
};

struct TextInstanceGPU {
    float rect[4]{};
    float uv_rect[4]{};
    float color[4]{};
};

struct ParsedTextRun {
    float x = 0.0f;
    float y = 0.0f;
    float font_size = 0.0f;
    bool mono = false;
    float r = 0.0f;
    float g = 0.0f;
    float b = 0.0f;
    float a = 1.0f;
    float line_height = 0.0f;
    char const* text = nullptr;
    unsigned int len = 0;
};

struct RasterizedTextRun {
    std::vector<std::uint8_t> alpha;
    float x_offset = 0.0f;
    float width = 0.0f;
    float height = 0.0f;
    int pixel_width = 0;
    int pixel_height = 0;
    bool has_ink = false;
};

struct TextCacheEntry {
    std::string text;
    int font_key = 0;
    int line_height_key = 0;
    int scale_key = 0;
    bool mono = false;
    float x_offset = 0.0f;
    float width = 0.0f;
    float height = 0.0f;
    float u = 0.0f;
    float v = 0.0f;
    float uw = 0.0f;
    float vh = 0.0f;
    bool has_ink = false;
};

struct TextAtlasCache {
    static constexpr int atlas_size = 4096;

    std::vector<TextCacheEntry> entries;
    std::vector<std::uint8_t> pixels;
    int cursor_x = 0;
    int cursor_y = 0;
    int row_height = 0;
    bool dirty = false;
    int dirty_min_x = atlas_size;
    int dirty_min_y = atlas_size;
    int dirty_max_x = 0;
    int dirty_max_y = 0;
    int active_scale_key = 0;
};

struct FrameScratch {
    std::vector<ColorInstanceGPU> color_instances;
    std::vector<ParsedTextRun> text_runs;
    std::vector<TextInstanceGPU> text_instances;

    void clear() {
        color_instances.clear();
        text_runs.clear();
        text_instances.clear();
    }
};

inline Color unpack_color(unsigned int packed) noexcept {
    return {
        static_cast<unsigned char>((packed >> 24) & 0xFF),
        static_cast<unsigned char>((packed >> 16) & 0xFF),
        static_cast<unsigned char>((packed >>  8) & 0xFF),
        static_cast<unsigned char>( packed        & 0xFF),
    };
}

inline int quantize_metric(float value) noexcept {
    return static_cast<int>(std::lround(value * 1000.0f));
}

inline std::vector<metrics::Attribute> native_attrs(char const* phase) {
    return {{"platform", "macos"}, {"phase", phase}};
}

inline std::vector<metrics::Attribute> native_platform_attrs() {
    return {{"platform", "macos"}};
}

inline std::vector<metrics::Attribute> native_buffer_attrs(char const* buffer) {
    return {{"platform", "macos"}, {"buffer", buffer}};
}

inline void append_color_instance(std::vector<ColorInstanceGPU>& out,
                                  float x, float y, float w, float h,
                                  float r, float g, float b, float a,
                                  float p0 = 0.0f, float p1 = 0.0f,
                                  float p2 = 0.0f, float p3 = 0.0f) {
    ColorInstanceGPU inst{};
    inst.rect[0] = x;
    inst.rect[1] = y;
    inst.rect[2] = w;
    inst.rect[3] = h;
    inst.color[0] = r;
    inst.color[1] = g;
    inst.color[2] = b;
    inst.color[3] = a;
    inst.params[0] = p0;
    inst.params[1] = p1;
    inst.params[2] = p2;
    inst.params[3] = p3;
    out.push_back(inst);
}

inline void append_text_instance(std::vector<TextInstanceGPU>& out,
                                 float x, float y, float w, float h,
                                 float u, float v, float uw, float vh,
                                 float r, float g, float b, float a) {
    TextInstanceGPU inst{};
    inst.rect[0] = x;
    inst.rect[1] = y;
    inst.rect[2] = w;
    inst.rect[3] = h;
    inst.uv_rect[0] = u;
    inst.uv_rect[1] = v;
    inst.uv_rect[2] = uw;
    inst.uv_rect[3] = vh;
    inst.color[0] = r;
    inst.color[1] = g;
    inst.color[2] = b;
    inst.color[3] = a;
    out.push_back(inst);
}

struct CommandReader {
    unsigned char const* cur = nullptr;
    unsigned char const* end = nullptr;

    bool can_read(unsigned int bytes) const noexcept {
        return cur + bytes <= end;
    }

    bool read_u32(unsigned int& out) noexcept {
        if (!can_read(4)) return false;
        std::memcpy(&out, cur, 4);
        cur += 4;
        return true;
    }

    bool read_f32(float& out) noexcept {
        unsigned int bits = 0;
        if (!read_u32(bits)) return false;
        std::memcpy(&out, &bits, 4);
        return true;
    }

    bool read_text(char const*& data, unsigned int len) noexcept {
        if (!can_read(len)) return false;
        data = reinterpret_cast<char const*>(cur);
        cur += len;
        auto const padded_len = (len + 3u) & ~3u;
        if (padded_len > len) {
            if (!can_read(padded_len - len)) return false;
            cur += padded_len - len;
        }
        return true;
    }
};

inline bool decode_frame_commands(unsigned char const* buf, unsigned int len,
                                  float line_height_ratio,
                                  FrameScratch& scratch,
                                  std::vector<HitRegionCmd>& hit_regions,
                                  double& cr, double& cg,
                                  double& cb, double& ca) {
    scratch.clear();
    hit_regions.clear();
    cr = 0.98;
    cg = 0.98;
    cb = 0.98;
    ca = 1.0;

    CommandReader reader{buf, buf + len};
    while (reader.cur < reader.end) {
        unsigned int raw_cmd = 0;
        if (!reader.read_u32(raw_cmd))
            return false;

        switch (static_cast<Cmd>(raw_cmd)) {
            case Cmd::Clear: {
                unsigned int packed = 0;
                if (!reader.read_u32(packed))
                    return false;
                auto color = unpack_color(packed);
                cr = color.r / 255.0;
                cg = color.g / 255.0;
                cb = color.b / 255.0;
                ca = color.a / 255.0;
                break;
            }
            case Cmd::FillRect: {
                float x = 0.0f, y = 0.0f, w = 0.0f, h = 0.0f;
                unsigned int packed = 0;
                if (!reader.read_f32(x) || !reader.read_f32(y)
                    || !reader.read_f32(w) || !reader.read_f32(h)
                    || !reader.read_u32(packed))
                    return false;
                auto color = unpack_color(packed);
                append_color_instance(
                    scratch.color_instances,
                    x, y, w, h,
                    color.r / 255.0f, color.g / 255.0f,
                    color.b / 255.0f, color.a / 255.0f);
                break;
            }
            case Cmd::StrokeRect: {
                float x = 0.0f, y = 0.0f, w = 0.0f, h = 0.0f, line_width = 0.0f;
                unsigned int packed = 0;
                if (!reader.read_f32(x) || !reader.read_f32(y)
                    || !reader.read_f32(w) || !reader.read_f32(h)
                    || !reader.read_f32(line_width)
                    || !reader.read_u32(packed))
                    return false;
                auto color = unpack_color(packed);
                append_color_instance(
                    scratch.color_instances,
                    x, y, w, h,
                    color.r / 255.0f, color.g / 255.0f,
                    color.b / 255.0f, color.a / 255.0f,
                    0.0f, line_width, 1.0f);
                break;
            }
            case Cmd::RoundRect: {
                float x = 0.0f, y = 0.0f, w = 0.0f, h = 0.0f, radius = 0.0f;
                unsigned int packed = 0;
                if (!reader.read_f32(x) || !reader.read_f32(y)
                    || !reader.read_f32(w) || !reader.read_f32(h)
                    || !reader.read_f32(radius)
                    || !reader.read_u32(packed))
                    return false;
                auto color = unpack_color(packed);
                append_color_instance(
                    scratch.color_instances,
                    x, y, w, h,
                    color.r / 255.0f, color.g / 255.0f,
                    color.b / 255.0f, color.a / 255.0f,
                    radius, 0.0f, 2.0f);
                break;
            }
            case Cmd::DrawText: {
                float x = 0.0f, y = 0.0f, font_size = 0.0f;
                unsigned int mono = 0;
                unsigned int packed = 0;
                unsigned int text_len = 0;
                char const* text = nullptr;
                if (!reader.read_f32(x) || !reader.read_f32(y)
                    || !reader.read_f32(font_size) || !reader.read_u32(mono)
                    || !reader.read_u32(packed) || !reader.read_u32(text_len)
                    || !reader.read_text(text, text_len))
                    return false;
                auto color = unpack_color(packed);
                scratch.text_runs.push_back({
                    x,
                    y,
                    font_size,
                    mono != 0,
                    color.r / 255.0f,
                    color.g / 255.0f,
                    color.b / 255.0f,
                    color.a / 255.0f,
                    font_size * line_height_ratio,
                    text,
                    text_len,
                });
                break;
            }
            case Cmd::DrawLine: {
                float x1 = 0.0f, y1 = 0.0f, x2 = 0.0f, y2 = 0.0f, thickness = 0.0f;
                unsigned int packed = 0;
                if (!reader.read_f32(x1) || !reader.read_f32(y1)
                    || !reader.read_f32(x2) || !reader.read_f32(y2)
                    || !reader.read_f32(thickness)
                    || !reader.read_u32(packed))
                    return false;
                auto color = unpack_color(packed);
                float dx = x2 - x1;
                float dy = y2 - y1;
                float line_len = std::sqrt(dx * dx + dy * dy);
                float w = (dy == 0.0f) ? line_len : thickness;
                float h = (dx == 0.0f) ? line_len : thickness;
                float x = (dx == 0.0f)
                    ? x1 - thickness / 2.0f
                    : (x1 < x2 ? x1 : x2);
                float y = (dy == 0.0f)
                    ? y1 - thickness / 2.0f
                    : (y1 < y2 ? y1 : y2);
                append_color_instance(
                    scratch.color_instances,
                    x, y, w, h,
                    color.r / 255.0f, color.g / 255.0f,
                    color.b / 255.0f, color.a / 255.0f,
                    0.0f, 0.0f, 3.0f);
                break;
            }
            case Cmd::HitRegion: {
                HitRegionCmd hit{};
                unsigned int cursor_type = 0;
                if (!reader.read_f32(hit.x) || !reader.read_f32(hit.y)
                    || !reader.read_f32(hit.w) || !reader.read_f32(hit.h)
                    || !reader.read_u32(hit.callback_id)
                    || !reader.read_u32(cursor_type))
                    return false;
                hit.cursor_type = cursor_type;
                hit_regions.push_back(hit);
                break;
            }
            case Cmd::DrawImage: {
                float x = 0.0f, y = 0.0f, w = 0.0f, h = 0.0f;
                unsigned int url_len = 0;
                char const* ignored = nullptr;
                if (!reader.read_f32(x) || !reader.read_f32(y)
                    || !reader.read_f32(w) || !reader.read_f32(h)
                    || !reader.read_u32(url_len)
                    || !reader.read_text(ignored, url_len))
                    return false;
                break;
            }
            default:
                return false;
        }
    }
    return true;
}

inline void reset_text_cache(TextAtlasCache& cache) {
    bool had_entries = !cache.entries.empty();
    cache.entries.clear();
    cache.cursor_x = 0;
    cache.cursor_y = 0;
    cache.row_height = 0;
    if (cache.pixels.size()
        != static_cast<std::size_t>(TextAtlasCache::atlas_size * TextAtlasCache::atlas_size)) {
        cache.pixels.assign(
            static_cast<std::size_t>(TextAtlasCache::atlas_size * TextAtlasCache::atlas_size), 0);
    } else {
        std::fill(cache.pixels.begin(), cache.pixels.end(), 0);
    }
    cache.dirty = had_entries;
    cache.dirty_min_x = had_entries ? 0 : TextAtlasCache::atlas_size;
    cache.dirty_min_y = had_entries ? 0 : TextAtlasCache::atlas_size;
    cache.dirty_max_x = had_entries ? TextAtlasCache::atlas_size : 0;
    cache.dirty_max_y = had_entries ? TextAtlasCache::atlas_size : 0;
}

inline void mark_text_cache_dirty(TextAtlasCache& cache,
                                  int x, int y, int w, int h) {
    cache.dirty = true;
    if (x < cache.dirty_min_x) cache.dirty_min_x = x;
    if (y < cache.dirty_min_y) cache.dirty_min_y = y;
    if (x + w > cache.dirty_max_x) cache.dirty_max_x = x + w;
    if (y + h > cache.dirty_max_y) cache.dirty_max_y = y + h;
}

inline bool reserve_text_slot(TextAtlasCache& cache, int width, int height,
                              int& out_x, int& out_y) {
    if (width <= 0 || height <= 0
        || width > TextAtlasCache::atlas_size
        || height > TextAtlasCache::atlas_size)
        return false;

    if (cache.cursor_x + width > TextAtlasCache::atlas_size) {
        cache.cursor_x = 0;
        cache.cursor_y += cache.row_height;
        cache.row_height = 0;
    }
    if (cache.cursor_y + height > TextAtlasCache::atlas_size)
        return false;

    out_x = cache.cursor_x;
    out_y = cache.cursor_y;
    cache.cursor_x += width;
    if (height > cache.row_height) cache.row_height = height;
    return true;
}

inline bool text_cache_matches(TextCacheEntry const& entry,
                               ParsedTextRun const& run,
                               int font_key,
                               int line_height_key,
                               int scale_key) {
    return entry.font_key == font_key
        && entry.line_height_key == line_height_key
        && entry.scale_key == scale_key
        && entry.mono == run.mono
        && entry.text.size() == run.len
        && std::memcmp(entry.text.data(), run.text, run.len) == 0;
}

inline TextCacheEntry* find_text_cache_entry(TextAtlasCache& cache,
                                             ParsedTextRun const& run,
                                             int font_key,
                                             int line_height_key,
                                             int scale_key) {
    for (auto& entry : cache.entries) {
        if (text_cache_matches(entry, run, font_key, line_height_key, scale_key))
            return &entry;
    }
    return nullptr;
}

inline bool rasterize_text_run(char const* text_ptr, unsigned int len,
                               float font_size, bool mono,
                               float line_height, float scale,
                               RasterizedTextRun& out) {
    out = {};
    if (!g_text.initialized || len == 0)
        return false;

    CTFontRef base = mono ? g_text.mono : g_text.sans;
    CFGuard<CTFontRef> font(
        CTFontCreateCopyWithAttributes(base, font_size * scale, nullptr, nullptr));
    if (!font)
        return false;

    static thread_local std::vector<uint32_t> codepoints;
    static thread_local std::vector<UniChar> unichars;
    decode_utf8(text_ptr, len, codepoints);
    codepoints_to_unichars(codepoints, unichars);

    auto count = static_cast<CFIndex>(unichars.size());
    std::vector<CGGlyph> glyphs(static_cast<std::size_t>(count));
    CTFontGetGlyphsForCharacters(font, unichars.data(), glyphs.data(), count);

    std::vector<CGSize> advances(static_cast<std::size_t>(count));
    CTFontGetAdvancesForGlyphs(font, kCTFontOrientationHorizontal,
                               glyphs.data(), advances.data(), count);

    double logical_width = 0.0;
    for (CFIndex i = 0; i < count; ++i)
        logical_width += advances[static_cast<std::size_t>(i)].width / scale;

    float ascent = static_cast<float>(CTFontGetAscent(font));
    float descent = static_cast<float>(CTFontGetDescent(font));
    float leading = static_cast<float>(CTFontGetLeading(font));
    int padding = static_cast<int>(std::ceil(scale)) + 1;
    if (padding < 2) padding = 2;

    auto box = make_line_box(
        static_cast<float>(logical_width),
        line_height > 0.0f ? line_height : font_size * 1.6f,
        ascent, descent, leading, scale, padding);

    std::vector<std::uint8_t> slot_alpha(
        static_cast<std::size_t>(box.slot_width * box.slot_height), 0);

    bool has_ink = false;
    int ink_min_x = box.slot_width;
    int ink_max_x = -1;
    float pen_x = static_cast<float>(padding);

    for (CFIndex gi = 0; gi < count; ++gi) {
        auto glyph_index = static_cast<std::size_t>(gi);
        if (glyphs[glyph_index] == 0) {
            pen_x += static_cast<float>(advances[glyph_index].width);
            continue;
        }

        CGRect bbox;
        CTFontGetBoundingRectsForGlyphs(font, kCTFontOrientationHorizontal,
                                        &glyphs[glyph_index], &bbox, 1);
        int gw = static_cast<int>(std::ceil(bbox.size.width)) + 2;
        int gh = static_cast<int>(std::ceil(bbox.size.height)) + 2;
        if (gw <= 0 || gh <= 0) {
            pen_x += static_cast<float>(advances[glyph_index].width);
            continue;
        }

        std::vector<std::uint8_t> glyph_buf(static_cast<std::size_t>(gw * gh), 0);
        auto* ctx = CGBitmapContextCreate(
            glyph_buf.data(), gw, gh, 8, gw, nullptr, kCGImageAlphaOnly);
        if (!ctx) {
            pen_x += static_cast<float>(advances[glyph_index].width);
            continue;
        }

        CGPoint pos{
            .x = static_cast<CGFloat>(-bbox.origin.x + 1),
            .y = static_cast<CGFloat>(-bbox.origin.y + 1),
        };
        CTFontDrawGlyphs(font, &glyphs[glyph_index], &pos, 1, ctx);
        CGContextRelease(ctx);

        int gx = static_cast<int>(std::lround(pen_x + bbox.origin.x));
        int gy = static_cast<int>(std::lround(
            box.baseline_y - bbox.origin.y - bbox.size.height));

        for (int row = 0; row < gh; ++row) {
            for (int col = 0; col < gw; ++col) {
                int local_x = gx + col;
                int local_y = gy + row;
                if (local_x < 0 || local_x >= box.slot_width
                    || local_y < 0 || local_y >= box.slot_height)
                    continue;

                auto alpha = glyph_buf[static_cast<std::size_t>(row * gw + col)];
                if (alpha == 0)
                    continue;

                has_ink = true;
                if (local_x < ink_min_x) ink_min_x = local_x;
                if (local_x > ink_max_x) ink_max_x = local_x;
                slot_alpha[static_cast<std::size_t>(local_y * box.slot_width + local_x)] = alpha;
            }
        }

        pen_x += static_cast<float>(advances[glyph_index].width);
    }

    if (!has_ink || ink_max_x < ink_min_x)
        return false;

    out.has_ink = true;
    out.pixel_width = ink_max_x - ink_min_x + 1;
    out.pixel_height = box.render_height;
    out.x_offset = static_cast<float>(ink_min_x) / scale;
    out.width = static_cast<float>(out.pixel_width) / scale;
    out.height = static_cast<float>(out.pixel_height) / scale;
    out.alpha.resize(static_cast<std::size_t>(out.pixel_width * out.pixel_height), 0);

    for (int row = 0; row < out.pixel_height; ++row) {
        auto src = &slot_alpha[static_cast<std::size_t>(
            (box.render_top + row) * box.slot_width + ink_min_x)];
        auto* dst = out.alpha.data()
            + static_cast<std::size_t>(row * out.pixel_width);
        std::memcpy(dst, src, static_cast<std::size_t>(out.pixel_width));
    }

    return true;
}

inline constexpr char MSL_SHADERS[] = R"(
#include <metal_stdlib>
using namespace metal;

struct Uniforms { float2 viewport; float2 _pad; };

struct ColorVsOut {
    float4 pos       [[position]];
    float4 color;
    float2 local_pos;
    float2 rect_size;
    float4 params;
};

struct ColorInstance {
    float4 rect;
    float4 color;
    float4 params;
};

vertex ColorVsOut vs_color(
    uint vi [[vertex_id]],
    uint ii [[instance_id]],
    constant Uniforms& u [[buffer(0)]],
    const device ColorInstance* instances [[buffer(1)]]
) {
    constexpr float2 corners[] = {
        float2(0,0), float2(1,0), float2(0,1),
        float2(1,0), float2(1,1), float2(0,1),
    };
    float2 c = corners[vi];
    ColorInstance inst = instances[ii];
    float px = inst.rect.x + c.x * inst.rect.z;
    float py = inst.rect.y + c.y * inst.rect.w;
    float cx = (px / u.viewport.x) * 2.0 - 1.0;
    float cy = 1.0 - (py / u.viewport.y) * 2.0;
    ColorVsOut out;
    out.pos = float4(cx, cy, 0, 1);
    out.color = inst.color;
    out.local_pos = c * inst.rect.zw;
    out.rect_size = inst.rect.zw;
    out.params = inst.params;
    return out;
}

fragment float4 fs_color(ColorVsOut in [[stage_in]]) {
    uint draw_type = uint(in.params.z);
    float radius = in.params.x;
    float border_w = in.params.y;
    if (draw_type == 2u && radius > 0.0) {
        float2 half_size = in.rect_size * 0.5;
        float2 p = abs(in.local_pos - half_size) - half_size + float2(radius);
        float d = length(max(p, float2(0.0))) - radius;
        if (d > 0.5) discard_fragment();
        float alpha = in.color.a * clamp(0.5 - d, 0.0, 1.0);
        return float4(in.color.rgb * alpha, alpha);
    }
    if (draw_type == 1u) {
        float2 lp = in.local_pos;
        float2 sz = in.rect_size;
        if (lp.x > border_w && lp.x < sz.x - border_w &&
            lp.y > border_w && lp.y < sz.y - border_w) discard_fragment();
        return in.color;
    }
    return in.color;
}

struct TextVsOut {
    float4 pos [[position]];
    float2 uv;
    float4 color;
};

struct TextInstance {
    float4 rect;
    float4 uv_rect;
    float4 color;
};

vertex TextVsOut vs_text(
    uint vi [[vertex_id]],
    uint ii [[instance_id]],
    constant Uniforms& u [[buffer(0)]],
    const device TextInstance* instances [[buffer(1)]]
) {
    constexpr float2 corners[] = {
        float2(0,0), float2(1,0), float2(0,1),
        float2(1,0), float2(1,1), float2(0,1),
    };
    float2 c = corners[vi];
    TextInstance inst = instances[ii];
    float px = inst.rect.x + c.x * inst.rect.z;
    float py = inst.rect.y + c.y * inst.rect.w;
    float cx = (px / u.viewport.x) * 2.0 - 1.0;
    float cy = 1.0 - (py / u.viewport.y) * 2.0;
    TextVsOut out;
    out.pos = float4(cx, cy, 0, 1);
    out.uv = inst.uv_rect.xy + c * inst.uv_rect.zw;
    out.color = inst.color;
    return out;
}

fragment float4 fs_text(
    TextVsOut in [[stage_in]],
    texture2d<float> atlas [[texture(0)]],
    sampler samp [[sampler(0)]]
) {
    float coverage = atlas.sample(samp, in.uv).r;
    if (coverage < 0.01) discard_fragment();
    return float4(in.color.rgb, in.color.a * coverage);
}
)";

struct RendererState {
    MTL::Device* device = nullptr;
    MTL::CommandQueue* queue = nullptr;
    CA::MetalLayer* layer = nullptr;
    MTL::RenderPipelineState* color_pipeline = nullptr;
    MTL::RenderPipelineState* text_pipeline = nullptr;
    MTL::Buffer* uniform_buf = nullptr;
    MTL::Buffer* color_instances_buf = nullptr;
    std::size_t color_instances_capacity = 0;
    MTL::Buffer* text_instances_buf = nullptr;
    std::size_t text_instances_capacity = 0;
    MTL::Texture* text_atlas_texture = nullptr;
    MTL::SamplerState* sampler = nullptr;
    std::vector<HitRegionCmd> hit_regions;
    FrameScratch scratch;
    TextAtlasCache text_cache;
    GLFWwindow* window = nullptr;
    int drawable_width = 0;
    int drawable_height = 0;
    bool initialized = false;
};

inline RendererState g_renderer;

inline float current_backing_scale(GLFWwindow* window) {
    if (!window) return 1.0f;
    int fbw = 0;
    int fbh = 0;
    int winw = 0;
    int winh = 0;
    glfwGetFramebufferSize(window, &fbw, &fbh);
    glfwGetWindowSize(window, &winw, &winh);
    float sx = (winw > 0) ? static_cast<float>(fbw) / static_cast<float>(winw) : 1.0f;
    float sy = (winh > 0) ? static_cast<float>(fbh) / static_cast<float>(winh) : 1.0f;
    float scale = (sx > sy) ? sx : sy;
    return (scale > 0.0f && std::isfinite(scale)) ? scale : 1.0f;
}

inline MTL::RenderPipelineState* create_pipeline(
        MTL::Device* device, MTL::Library* lib,
        char const* vs_name, char const* fs_name) {
    auto* vs_fn = lib->newFunction(NS::String::string(vs_name, NS::UTF8StringEncoding));
    auto* fs_fn = lib->newFunction(NS::String::string(fs_name, NS::UTF8StringEncoding));
    if (!vs_fn || !fs_fn) {
        if (vs_fn) vs_fn->release();
        if (fs_fn) fs_fn->release();
        return nullptr;
    }

    auto* desc = MTL::RenderPipelineDescriptor::alloc()->init();
    desc->setVertexFunction(vs_fn);
    desc->setFragmentFunction(fs_fn);

    auto* attachment = desc->colorAttachments()->object(0);
    attachment->setPixelFormat(MTL::PixelFormatBGRA8Unorm);
    attachment->setBlendingEnabled(true);
    attachment->setSourceRGBBlendFactor(MTL::BlendFactorSourceAlpha);
    attachment->setDestinationRGBBlendFactor(MTL::BlendFactorOneMinusSourceAlpha);
    attachment->setRgbBlendOperation(MTL::BlendOperationAdd);
    attachment->setSourceAlphaBlendFactor(MTL::BlendFactorOne);
    attachment->setDestinationAlphaBlendFactor(MTL::BlendFactorOneMinusSourceAlpha);
    attachment->setAlphaBlendOperation(MTL::BlendOperationAdd);

    NS::Error* err = nullptr;
    auto* pipeline = device->newRenderPipelineState(desc, &err);
    if (err) {
        std::fprintf(stderr, "[metal] pipeline error: %s\n",
                     err->localizedDescription()->utf8String());
    }
    desc->release();
    vs_fn->release();
    fs_fn->release();
    return pipeline;
}

inline bool ensure_instance_buffer(MTL::Buffer*& buffer,
                                   std::size_t& capacity,
                                   std::size_t required,
                                   char const* name) {
    if (required == 0)
        return true;
    if (required <= capacity && buffer)
        return true;

    std::size_t new_capacity = (capacity > 0) ? capacity : 4096;
    while (new_capacity < required)
        new_capacity *= 2;

    auto* replacement = g_renderer.device->newBuffer(
        NS::UInteger(new_capacity),
        MTL::ResourceStorageModeShared);
    if (!replacement)
        return false;

    if (buffer)
        buffer->release();
    buffer = replacement;
    capacity = new_capacity;
    metrics::inst::native_buffer_reallocations.add(1, native_buffer_attrs(name));
    return true;
}

inline bool ensure_text_atlas_texture() {
    if (g_renderer.text_atlas_texture)
        return true;

    auto* tex_desc = MTL::TextureDescriptor::texture2DDescriptor(
        MTL::PixelFormatR8Unorm,
        NS::UInteger(TextAtlasCache::atlas_size),
        NS::UInteger(TextAtlasCache::atlas_size),
        false);
    tex_desc->setUsage(MTL::TextureUsageShaderRead);
    g_renderer.text_atlas_texture = g_renderer.device->newTexture(tex_desc);
    return g_renderer.text_atlas_texture != nullptr;
}

inline void sync_drawable_size(int fbw, int fbh) {
    if (fbw == g_renderer.drawable_width && fbh == g_renderer.drawable_height)
        return;
    CGSize drawable_size{
        .width = static_cast<CGFloat>(fbw),
        .height = static_cast<CGFloat>(fbh),
    };
    g_renderer.layer->setDrawableSize(drawable_size);
    g_renderer.drawable_width = fbw;
    g_renderer.drawable_height = fbh;
}

inline bool upload_text_cache() {
    auto& cache = g_renderer.text_cache;
    if (!cache.dirty)
        return true;
    if (!ensure_text_atlas_texture())
        return false;

    auto width = cache.dirty_max_x - cache.dirty_min_x;
    auto height = cache.dirty_max_y - cache.dirty_min_y;
    if (width <= 0 || height <= 0) {
        cache.dirty = false;
        cache.dirty_min_x = TextAtlasCache::atlas_size;
        cache.dirty_min_y = TextAtlasCache::atlas_size;
        cache.dirty_max_x = 0;
        cache.dirty_max_y = 0;
        return true;
    }

    MTL::Region region = MTL::Region::Make2D(
        cache.dirty_min_x,
        cache.dirty_min_y,
        width,
        height);
    auto const* src = cache.pixels.data()
        + static_cast<std::size_t>(
            cache.dirty_min_y * TextAtlasCache::atlas_size + cache.dirty_min_x);
    g_renderer.text_atlas_texture->replaceRegion(
        region,
        0,
        src,
        TextAtlasCache::atlas_size);
    metrics::inst::native_texture_upload_bytes.add(
        static_cast<std::uint64_t>(width * height),
        native_platform_attrs());

    cache.dirty = false;
    cache.dirty_min_x = TextAtlasCache::atlas_size;
    cache.dirty_min_y = TextAtlasCache::atlas_size;
    cache.dirty_max_x = 0;
    cache.dirty_max_y = 0;
    return true;
}

inline bool prepare_text_instances(float scale) {
    auto& scratch = g_renderer.scratch;
    scratch.text_instances.clear();
    if (scratch.text_runs.empty())
        return true;

    auto& cache = g_renderer.text_cache;
    int scale_key = quantize_metric(scale);
    if (cache.active_scale_key != scale_key) {
        reset_text_cache(cache);
        cache.active_scale_key = scale_key;
    } else if (cache.pixels.empty()) {
        reset_text_cache(cache);
        cache.active_scale_key = scale_key;
    }

    bool retried_after_reset = false;
    while (true) {
        bool restart = false;
        scratch.text_instances.clear();

        for (auto const& run : scratch.text_runs) {
            if (!run.text || run.len == 0)
                continue;

            int font_key = quantize_metric(run.font_size);
            int line_height_key = quantize_metric(run.line_height);
            auto* entry = find_text_cache_entry(
                cache, run, font_key, line_height_key, scale_key);

            if (entry) {
                metrics::inst::native_text_cache_hits.add(1, native_platform_attrs());
            } else {
                RasterizedTextRun rasterized;
                if (!rasterize_text_run(
                        run.text, run.len, run.font_size, run.mono,
                        run.line_height, scale, rasterized)) {
                    metrics::inst::native_text_cache_misses.add(1, native_platform_attrs());
                    continue;
                }

                int slot_x = 0;
                int slot_y = 0;
                if (!reserve_text_slot(cache, rasterized.pixel_width,
                                       rasterized.pixel_height, slot_x, slot_y)) {
                    if (retried_after_reset) {
                        log::warn("phenotype.native.macos",
                                  "text cache atlas exhausted at scale {}", scale);
                        return true;
                    }
                    reset_text_cache(cache);
                    cache.active_scale_key = scale_key;
                    retried_after_reset = true;
                    restart = true;
                    break;
                }

                for (int row = 0; row < rasterized.pixel_height; ++row) {
                    auto* dst = cache.pixels.data()
                        + static_cast<std::size_t>(
                            (slot_y + row) * TextAtlasCache::atlas_size + slot_x);
                    auto const* src = rasterized.alpha.data()
                        + static_cast<std::size_t>(row * rasterized.pixel_width);
                    std::memcpy(
                        dst,
                        src,
                        static_cast<std::size_t>(rasterized.pixel_width));
                }
                mark_text_cache_dirty(
                    cache, slot_x, slot_y, rasterized.pixel_width, rasterized.pixel_height);

                cache.entries.push_back({
                    std::string(run.text, run.len),
                    font_key,
                    line_height_key,
                    scale_key,
                    run.mono,
                    rasterized.x_offset,
                    rasterized.width,
                    rasterized.height,
                    static_cast<float>(slot_x) / TextAtlasCache::atlas_size,
                    static_cast<float>(slot_y) / TextAtlasCache::atlas_size,
                    static_cast<float>(rasterized.pixel_width) / TextAtlasCache::atlas_size,
                    static_cast<float>(rasterized.pixel_height) / TextAtlasCache::atlas_size,
                    rasterized.has_ink,
                });
                entry = &cache.entries.back();
                metrics::inst::native_text_cache_misses.add(1, native_platform_attrs());
            }

            if (!entry || !entry->has_ink)
                continue;

            append_text_instance(
                scratch.text_instances,
                snap_to_pixel_grid(run.x, scale) + entry->x_offset,
                snap_to_pixel_grid(run.y, scale),
                entry->width,
                entry->height,
                entry->u,
                entry->v,
                entry->uw,
                entry->vh,
                run.r,
                run.g,
                run.b,
                run.a);
        }

        if (!restart)
            return true;
    }
}

inline void renderer_init(GLFWwindow* window) {
    if (g_renderer.initialized) return;
    g_renderer.window = window;

    g_renderer.device = MTL::CreateSystemDefaultDevice();
    if (!g_renderer.device) {
        std::fprintf(stderr, "[metal] no device\n");
        return;
    }
    g_renderer.queue = g_renderer.device->newCommandQueue();

    void* nswin = glfwGetCocoaWindow(window);
    auto sel_cv = sel_registerName("contentView");
    auto sel_wl = sel_registerName("setWantsLayer:");
    auto sel_sl = sel_registerName("setLayer:");
    void* view = reinterpret_cast<void*(*)(void*, SEL)>(objc_msgSend)(nswin, sel_cv);
    reinterpret_cast<void(*)(void*, SEL, bool)>(objc_msgSend)(view, sel_wl, true);

    g_renderer.layer = CA::MetalLayer::layer()->retain();
    g_renderer.layer->setDevice(g_renderer.device);
    g_renderer.layer->setPixelFormat(MTL::PixelFormatBGRA8Unorm);

    int fbw = 0;
    int fbh = 0;
    glfwGetFramebufferSize(window, &fbw, &fbh);
    sync_drawable_size(fbw, fbh);

    reinterpret_cast<void(*)(void*, SEL, void*)>(objc_msgSend)(
        view, sel_sl, static_cast<void*>(g_renderer.layer));

    NS::Error* err = nullptr;
    auto* lib = g_renderer.device->newLibrary(
        NS::String::string(MSL_SHADERS, NS::UTF8StringEncoding), nullptr, &err);
    if (!lib) {
        std::fprintf(stderr, "[metal] shader compile: %s\n",
                     err ? err->localizedDescription()->utf8String() : "unknown");
        return;
    }

    g_renderer.color_pipeline = create_pipeline(g_renderer.device, lib, "vs_color", "fs_color");
    g_renderer.text_pipeline = create_pipeline(g_renderer.device, lib, "vs_text", "fs_text");
    lib->release();
    if (!g_renderer.color_pipeline || !g_renderer.text_pipeline) {
        std::fprintf(stderr, "[metal] failed to create render pipelines\n");
        return;
    }

    g_renderer.uniform_buf = g_renderer.device->newBuffer(16, MTL::ResourceStorageModeShared);

    auto* sampler_desc = MTL::SamplerDescriptor::alloc()->init();
    sampler_desc->setMinFilter(MTL::SamplerMinMagFilterLinear);
    sampler_desc->setMagFilter(MTL::SamplerMinMagFilterLinear);
    sampler_desc->setSAddressMode(MTL::SamplerAddressModeClampToEdge);
    sampler_desc->setTAddressMode(MTL::SamplerAddressModeClampToEdge);
    g_renderer.sampler = g_renderer.device->newSamplerState(sampler_desc);
    sampler_desc->release();

    g_renderer.initialized = true;
    g_renderer.text_cache.active_scale_key = 0;

    int winw = 0;
    int winh = 0;
    glfwGetWindowSize(window, &winw, &winh);
    std::printf("[phenotype-native] Metal initialized (%dx%d)\n", winw, winh);
}

inline void renderer_flush(unsigned char const* buf, unsigned int len) {
    if (len == 0 || !g_renderer.initialized) return;

    NS::AutoreleasePool* pool = NS::AutoreleasePool::alloc()->init();
    float text_scale = current_backing_scale(g_renderer.window);
    float line_height_ratio = ::phenotype::detail::g_app.theme.line_height_ratio;

    double cr = 0.98;
    double cg = 0.98;
    double cb = 0.98;
    double ca = 1.0;
    auto decode_started = metrics::detail::now_ns();
    if (!decode_frame_commands(
            buf, len, line_height_ratio,
            g_renderer.scratch, g_renderer.hit_regions,
            cr, cg, cb, ca)) {
        pool->release();
        return;
    }
    metrics::inst::native_phase_duration.record(
        metrics::detail::now_ns() - decode_started,
        native_attrs("command_decode"));

    int fbw = 0;
    int fbh = 0;
    glfwGetFramebufferSize(g_renderer.window, &fbw, &fbh);
    if (fbw == 0 || fbh == 0) {
        pool->release();
        return;
    }

    sync_drawable_size(fbw, fbh);

    auto text_started = metrics::detail::now_ns();
    if (!prepare_text_instances(text_scale)) {
        pool->release();
        return;
    }
    metrics::inst::native_phase_duration.record(
        metrics::detail::now_ns() - text_started,
        native_attrs("text_prepare"));

    int winw = 0;
    int winh = 0;
    glfwGetWindowSize(g_renderer.window, &winw, &winh);
    float uniforms[4] = {
        static_cast<float>(winw),
        static_cast<float>(winh),
        0,
        0,
    };
    std::memcpy(g_renderer.uniform_buf->contents(), uniforms, 16);

    auto* drawable = g_renderer.layer->nextDrawable();
    if (!drawable) {
        pool->release();
        return;
    }

    auto* pass = MTL::RenderPassDescriptor::renderPassDescriptor();
    pass->colorAttachments()->object(0)->setTexture(drawable->texture());
    pass->colorAttachments()->object(0)->setLoadAction(MTL::LoadActionClear);
    pass->colorAttachments()->object(0)->setClearColor(MTL::ClearColor::Make(cr, cg, cb, ca));
    pass->colorAttachments()->object(0)->setStoreAction(MTL::StoreActionStore);

    auto* command_buffer = g_renderer.queue->commandBuffer();
    auto* encoder = command_buffer->renderCommandEncoder(pass);

    auto& scratch = g_renderer.scratch;
    auto const color_bytes = scratch.color_instances.size() * sizeof(ColorInstanceGPU);
    uint32_t color_count = static_cast<uint32_t>(scratch.color_instances.size());
    if (color_count > 0) {
        if (!ensure_instance_buffer(
                g_renderer.color_instances_buf,
                g_renderer.color_instances_capacity,
                color_bytes,
                "color_instances")) {
            encoder->endEncoding();
            pool->release();
            return;
        }
        std::memcpy(
            g_renderer.color_instances_buf->contents(),
            scratch.color_instances.data(),
            color_bytes);
        encoder->setRenderPipelineState(g_renderer.color_pipeline);
        encoder->setVertexBuffer(g_renderer.uniform_buf, 0, 0);
        encoder->setVertexBuffer(g_renderer.color_instances_buf, 0, 1);
        encoder->drawPrimitives(MTL::PrimitiveTypeTriangle,
                                NS::UInteger(0), 6, NS::UInteger(color_count));
    }

    auto upload_started = metrics::detail::now_ns();
    if (!upload_text_cache()) {
        encoder->endEncoding();
        pool->release();
        return;
    }
    metrics::inst::native_phase_duration.record(
        metrics::detail::now_ns() - upload_started,
        native_attrs("text_upload"));

    auto const text_bytes = scratch.text_instances.size() * sizeof(TextInstanceGPU);
    if (!scratch.text_instances.empty() && g_renderer.text_atlas_texture) {
        if (!ensure_instance_buffer(
                g_renderer.text_instances_buf,
                g_renderer.text_instances_capacity,
                text_bytes,
                "text_instances")) {
            encoder->endEncoding();
            pool->release();
            return;
        }
        std::memcpy(
            g_renderer.text_instances_buf->contents(),
            scratch.text_instances.data(),
            text_bytes);

        encoder->setRenderPipelineState(g_renderer.text_pipeline);
        encoder->setVertexBuffer(g_renderer.uniform_buf, 0, 0);
        encoder->setVertexBuffer(g_renderer.text_instances_buf, 0, 1);
        encoder->setFragmentTexture(g_renderer.text_atlas_texture, 0);
        encoder->setFragmentSamplerState(g_renderer.sampler, 0);
        encoder->drawPrimitives(
            MTL::PrimitiveTypeTriangle,
            NS::UInteger(0), 6, NS::UInteger(scratch.text_instances.size()));
    }

    encoder->endEncoding();
    command_buffer->presentDrawable(drawable);
    command_buffer->commit();

    pool->release();
}

inline void renderer_shutdown() {
    if (g_renderer.sampler) { g_renderer.sampler->release(); g_renderer.sampler = nullptr; }
    if (g_renderer.text_atlas_texture) { g_renderer.text_atlas_texture->release(); g_renderer.text_atlas_texture = nullptr; }
    if (g_renderer.text_instances_buf) { g_renderer.text_instances_buf->release(); g_renderer.text_instances_buf = nullptr; }
    if (g_renderer.color_instances_buf) { g_renderer.color_instances_buf->release(); g_renderer.color_instances_buf = nullptr; }
    if (g_renderer.uniform_buf) { g_renderer.uniform_buf->release(); g_renderer.uniform_buf = nullptr; }
    if (g_renderer.text_pipeline) { g_renderer.text_pipeline->release(); g_renderer.text_pipeline = nullptr; }
    if (g_renderer.color_pipeline) { g_renderer.color_pipeline->release(); g_renderer.color_pipeline = nullptr; }
    if (g_renderer.layer) { g_renderer.layer->release(); g_renderer.layer = nullptr; }
    if (g_renderer.queue) { g_renderer.queue->release(); g_renderer.queue = nullptr; }
    if (g_renderer.device) { g_renderer.device->release(); g_renderer.device = nullptr; }
    g_renderer.hit_regions.clear();
    g_renderer.scratch.clear();
    g_renderer.text_cache.entries.clear();
    g_renderer.text_cache.pixels.clear();
    g_renderer.text_cache.cursor_x = 0;
    g_renderer.text_cache.cursor_y = 0;
    g_renderer.text_cache.row_height = 0;
    g_renderer.text_cache.dirty = false;
    g_renderer.text_cache.dirty_min_x = TextAtlasCache::atlas_size;
    g_renderer.text_cache.dirty_min_y = TextAtlasCache::atlas_size;
    g_renderer.text_cache.dirty_max_x = 0;
    g_renderer.text_cache.dirty_max_y = 0;
    g_renderer.text_cache.active_scale_key = 0;
    g_renderer.color_instances_capacity = 0;
    g_renderer.text_instances_capacity = 0;
    g_renderer.drawable_width = 0;
    g_renderer.drawable_height = 0;
    g_renderer.window = nullptr;
    g_renderer.initialized = false;
}

inline std::optional<unsigned int> renderer_hit_test(float x, float y,
                                                     float scroll_y) {
    float wy = y + scroll_y;
    for (int i = static_cast<int>(g_renderer.hit_regions.size()) - 1; i >= 0; --i) {
        auto const& hr = g_renderer.hit_regions[static_cast<std::size_t>(i)];
        if (x >= hr.x && x <= hr.x + hr.w && wy >= hr.y && wy <= hr.y + hr.h)
            return hr.callback_id;
    }
    return std::nullopt;
}

inline void macos_open_url(char const*, unsigned int) {
    // TODO: bridge to `open` or LaunchServices from the shell layer.
}

} // namespace phenotype::native::detail
#endif

export namespace phenotype::native {

inline platform_api const& macos_platform() {
#ifdef __APPLE__
    static platform_api api{
        "macos",
        true,
        {
            detail::text_init,
            detail::text_shutdown,
            detail::text_measure,
            detail::text_build_atlas,
        },
        {
            detail::renderer_init,
            detail::renderer_flush,
            detail::renderer_shutdown,
            detail::renderer_hit_test,
        },
        {},
        detail::macos_open_url,
        nullptr,
    };
    return api;
#else
    static platform_api api{};
    return api;
#endif
}

} // namespace phenotype::native
#endif
