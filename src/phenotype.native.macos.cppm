module;
#ifndef __wasi__
#include <algorithm>
#include <array>
#include <cmath>
#include <condition_variable>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <deque>
#include <filesystem>
#include <limits>
#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
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
import cppx.http.system;
import cppx.os;
import cppx.os.system;
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

struct TextLineMetrics {
    float logical_width = 0.0f;
    float ascent = 0.0f;
    float descent = 0.0f;
    float leading = 0.0f;
    CGRect glyph_bounds = CGRectNull;
};

inline CFGuard<CTFontRef> copy_text_font(float font_size, bool mono) {
    CTFontRef base = mono ? g_text.mono : g_text.sans;
    return CFGuard<CTFontRef>(
        base ? CTFontCreateCopyWithAttributes(base, font_size, nullptr, nullptr) : nullptr);
}

inline CFGuard<CFStringRef> create_text_cf_string(char const* text_ptr,
                                                  unsigned int len) {
    return CFGuard<CFStringRef>(
        (text_ptr && len > 0)
            ? CFStringCreateWithBytes(
                kCFAllocatorDefault,
                reinterpret_cast<UInt8 const*>(text_ptr),
                static_cast<CFIndex>(len),
                kCFStringEncodingUTF8,
                false)
            : nullptr);
}

inline CFGuard<CTLineRef> create_text_line(CTFontRef font,
                                           char const* text_ptr,
                                           unsigned int len) {
    if (!font || !text_ptr || len == 0)
        return CFGuard<CTLineRef>(nullptr);

    auto text = create_text_cf_string(text_ptr, len);
    if (!text)
        return CFGuard<CTLineRef>(nullptr);

    CFTypeRef keys[] = {kCTFontAttributeName};
    CFTypeRef values[] = {font};
    auto attrs = CFGuard<CFDictionaryRef>(CFDictionaryCreate(
        kCFAllocatorDefault,
        reinterpret_cast<void const**>(keys),
        reinterpret_cast<void const**>(values),
        1,
        &kCFTypeDictionaryKeyCallBacks,
        &kCFTypeDictionaryValueCallBacks));
    if (!attrs)
        return CFGuard<CTLineRef>(nullptr);

    auto attr_text = CFGuard<CFAttributedStringRef>(
        CFAttributedStringCreate(kCFAllocatorDefault, text, attrs));
    if (!attr_text)
        return CFGuard<CTLineRef>(nullptr);

    return CFGuard<CTLineRef>(CTLineCreateWithAttributedString(attr_text));
}

inline bool describe_text_line(CTLineRef line, float scale, TextLineMetrics& out) {
    if (!line)
        return false;

    CGFloat ascent = 0.0;
    CGFloat descent = 0.0;
    CGFloat leading = 0.0;
    double width = CTLineGetTypographicBounds(line, &ascent, &descent, &leading);
    if (!std::isfinite(width))
        return false;

    out.logical_width = static_cast<float>(width) / sanitize_scale(scale);
    out.ascent = static_cast<float>(ascent);
    out.descent = static_cast<float>(descent);
    out.leading = static_cast<float>(leading);
    out.glyph_bounds = CTLineGetBoundsWithOptions(
        line,
        static_cast<CTLineBoundsOptions>(kCTLineBoundsUseGlyphPathBounds));
    return true;
}

inline void expand_line_box_for_ink(LineBoxMetrics& box,
                                    CGRect const& glyph_bounds,
                                    float logical_width,
                                    float scale,
                                    int padding,
                                    int& line_origin_x) {
    line_origin_x = padding;
    if (CGRectIsNull(glyph_bounds)
        || !std::isfinite(glyph_bounds.origin.x)
        || !std::isfinite(glyph_bounds.size.width))
        return;

    float typographic_width_px = logical_width * sanitize_scale(scale);
    int left_overhang = 0;
    if (glyph_bounds.origin.x < 0.0)
        left_overhang = static_cast<int>(std::ceil(-glyph_bounds.origin.x));

    float bounds_max_x = static_cast<float>(CGRectGetMaxX(glyph_bounds));
    int right_overhang = 0;
    if (std::isfinite(bounds_max_x) && bounds_max_x > typographic_width_px) {
        right_overhang = static_cast<int>(
            std::ceil(bounds_max_x - typographic_width_px));
    }

    box.slot_width += left_overhang + right_overhang;
    if (box.slot_width < padding * 2 + 1)
        box.slot_width = padding * 2 + 1;
    line_origin_x = padding + left_overhang;
}

inline bool rasterize_line_alpha(CTLineRef line,
                                 int slot_width,
                                 int slot_height,
                                 int line_origin_x,
                                 float baseline_y,
                                 std::vector<std::uint8_t>& slot_alpha,
                                 int& ink_min_x,
                                 int& ink_max_x) {
    slot_alpha.assign(static_cast<std::size_t>(slot_width * slot_height), 0);
    ink_min_x = slot_width;
    ink_max_x = -1;

    auto runs = CTLineGetGlyphRuns(line);
    if (!runs)
        return false;

    bool has_ink = false;
    auto run_count = CFArrayGetCount(runs);
    for (CFIndex run_index = 0; run_index < run_count; ++run_index) {
        auto run = static_cast<CTRunRef>(CFArrayGetValueAtIndex(runs, run_index));
        if (!run)
            continue;

        auto run_attrs = CTRunGetAttributes(run);
        auto run_font = static_cast<CTFontRef>(
            run_attrs ? CFDictionaryGetValue(run_attrs, kCTFontAttributeName) : nullptr);
        if (!run_font)
            continue;

        auto glyph_count = CTRunGetGlyphCount(run);
        if (glyph_count <= 0)
            continue;

        std::vector<CGGlyph> glyphs(static_cast<std::size_t>(glyph_count));
        std::vector<CGPoint> positions(static_cast<std::size_t>(glyph_count));
        CFRange full_range{0, 0};
        CTRunGetGlyphs(run, full_range, glyphs.data());
        CTRunGetPositions(run, full_range, positions.data());

        for (CFIndex glyph_index = 0; glyph_index < glyph_count; ++glyph_index) {
            auto glyph = glyphs[static_cast<std::size_t>(glyph_index)];
            if (glyph == 0)
                continue;

            CGRect bbox;
            CTFontGetBoundingRectsForGlyphs(
                run_font, kCTFontOrientationHorizontal, &glyph, &bbox, 1);
            int gw = static_cast<int>(std::ceil(bbox.size.width)) + 2;
            int gh = static_cast<int>(std::ceil(bbox.size.height)) + 2;
            if (gw <= 0 || gh <= 0)
                continue;

            std::vector<std::uint8_t> glyph_buf(static_cast<std::size_t>(gw * gh), 0);
            auto* ctx = CGBitmapContextCreate(
                glyph_buf.data(), gw, gh, 8, gw, nullptr, kCGImageAlphaOnly);
            if (!ctx)
                continue;

            CGPoint pos{
                .x = static_cast<CGFloat>(-bbox.origin.x + 1.0),
                .y = static_cast<CGFloat>(-bbox.origin.y + 1.0),
            };
            CTFontDrawGlyphs(run_font, &glyph, &pos, 1, ctx);
            CGContextRelease(ctx);

            auto run_pos = positions[static_cast<std::size_t>(glyph_index)];
            int gx = static_cast<int>(std::lround(
                static_cast<float>(line_origin_x) + run_pos.x + bbox.origin.x));
            int gy = static_cast<int>(std::lround(
                baseline_y + run_pos.y - bbox.origin.y - bbox.size.height));

            for (int row = 0; row < gh; ++row) {
                for (int col = 0; col < gw; ++col) {
                    int local_x = gx + col;
                    int local_y = gy + row;
                    if (local_x < 0 || local_x >= slot_width
                        || local_y < 0 || local_y >= slot_height)
                        continue;

                    auto alpha = glyph_buf[static_cast<std::size_t>(row * gw + col)];
                    if (alpha == 0)
                        continue;

                    has_ink = true;
                    if (local_x < ink_min_x) ink_min_x = local_x;
                    if (local_x > ink_max_x) ink_max_x = local_x;
                    slot_alpha[static_cast<std::size_t>(
                        local_y * slot_width + local_x)] = alpha;
                }
            }
        }
    }

    return has_ink;
}

inline float text_measure(float font_size, bool mono,
                          char const* text_ptr, unsigned int len) {
    if (!g_text.initialized || len == 0)
        return 0.0f;

    auto font = copy_text_font(font_size, mono);
    if (!font)
        return 0.0f;

    auto line = create_text_line(font, text_ptr, len);
    if (!line)
        return 0.0f;

    double width = CTLineGetTypographicBounds(line, nullptr, nullptr, nullptr);
    return std::isfinite(width) ? static_cast<float>(width) : 0.0f;
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

    for (auto const& entry : entries) {
        if (entry.text.empty()) continue;

        auto font = copy_text_font(entry.font_size * scale, entry.mono);
        if (!font) continue;
        auto line = create_text_line(
            font, entry.text.c_str(), static_cast<unsigned int>(entry.text.size()));
        if (!line) continue;

        TextLineMetrics line_metrics;
        if (!describe_text_line(line, scale, line_metrics))
            continue;

        float logical_line_height = entry.line_height > 0
            ? entry.line_height
            : entry.font_size * 1.6f;
        float snapped_x = snap_to_pixel_grid(entry.x, scale);
        float snapped_y = snap_to_pixel_grid(entry.y, scale);

        auto box = make_line_box(
            line_metrics.logical_width,
            logical_line_height,
            line_metrics.ascent,
            line_metrics.descent,
            line_metrics.leading,
            scale,
            padding);
        int line_origin_x = padding;
        expand_line_box_for_ink(
            box,
            line_metrics.glyph_bounds,
            line_metrics.logical_width,
            scale,
            padding,
            line_origin_x);

        if (ax + box.slot_width > ATLAS_SIZE) {
            ax = 0;
            ay += row_height;
            row_height = 0;
        }
        if (ay + box.slot_height > ATLAS_SIZE) break;

        std::vector<std::uint8_t> slot_alpha;
        int ink_min_x = box.slot_width;
        int ink_max_x = -1;
        bool has_ink = rasterize_line_alpha(
            line,
            box.slot_width,
            box.slot_height,
            line_origin_x,
            box.baseline_y,
            slot_alpha,
            ink_min_x,
            ink_max_x);

        if (has_ink) {
            for (int row = 0; row < box.slot_height; ++row) {
                for (int col = 0; col < box.slot_width; ++col) {
                    auto alpha = slot_alpha[static_cast<std::size_t>(
                        row * box.slot_width + col)];
                    if (alpha == 0)
                        continue;

                    int px = ax + col;
                    int py = ay + row;
                    if (px < 0 || px >= ATLAS_SIZE || py < 0 || py >= ATLAS_SIZE)
                        continue;

                    auto idx = static_cast<std::size_t>((py * ATLAS_SIZE + px) * 4);
                    atlas.pixels[idx + 0] = static_cast<std::uint8_t>(
                        entry.r * 255.0f * alpha / 255.0f);
                    atlas.pixels[idx + 1] = static_cast<std::uint8_t>(
                        entry.g * 255.0f * alpha / 255.0f);
                    atlas.pixels[idx + 2] = static_cast<std::uint8_t>(
                        entry.b * 255.0f * alpha / 255.0f);
                    atlas.pixels[idx + 3] = alpha;
                }
            }

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

struct PendingImageCmd {
    float x = 0.0f;
    float y = 0.0f;
    float w = 0.0f;
    float h = 0.0f;
    std::string url;
};

struct ImageInstanceGPU {
    float rect[4]{};
    float uv_rect[4]{};
    float color[4]{};
};

struct DecodedImage {
    std::string url;
    std::vector<std::uint8_t> pixels;
    int width = 0;
    int height = 0;
    bool failed = false;
};

enum class ImageEntryState {
    pending,
    ready,
    failed,
};

struct ImageCacheEntry {
    ImageEntryState state = ImageEntryState::pending;
    float u = 0.0f;
    float v = 0.0f;
    float uw = 0.0f;
    float vh = 0.0f;
};

struct ImageAtlasCache {
    static constexpr int atlas_size = 2048;

    std::vector<std::uint8_t> pixels;
    std::map<std::string, ImageCacheEntry> cache;
    std::deque<std::string> pending_jobs;
    std::deque<DecodedImage> completed_jobs;
    std::mutex mutex;
    std::condition_variable cv;
    std::thread worker;
    void (*request_repaint)() = nullptr;
    bool worker_started = false;
    bool stop_worker = false;
    int cursor_x = 0;
    int cursor_y = 0;
    int row_height = 0;
    bool dirty = false;
    int dirty_min_x = atlas_size;
    int dirty_min_y = atlas_size;
    int dirty_max_x = 0;
    int dirty_max_y = 0;
};

inline ImageAtlasCache g_images;

struct FrameScratch {
    std::vector<ColorInstanceGPU> color_instances;
    std::vector<ParsedTextRun> text_runs;
    std::vector<PendingImageCmd> images;
    std::vector<ImageInstanceGPU> image_instances;
    std::vector<TextInstanceGPU> text_instances;

    void clear() {
        color_instances.clear();
        text_runs.clear();
        images.clear();
        image_instances.clear();
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

inline void append_image_instance(std::vector<ImageInstanceGPU>& out,
                                  float x, float y, float w, float h,
                                  float u, float v, float uw, float vh,
                                  float r = 1.0f, float g = 1.0f,
                                  float b = 1.0f, float a = 1.0f) {
    ImageInstanceGPU inst{};
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

inline bool is_http_url(std::string const& url) {
    return url.rfind("http://", 0) == 0 || url.rfind("https://", 0) == 0;
}

inline std::filesystem::path resolve_image_path(std::string const& url) {
    auto path = std::filesystem::path(url);
    if (path.is_absolute())
        return path;
    return std::filesystem::current_path() / path;
}

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
                PendingImageCmd image{};
                unsigned int url_len = 0;
                char const* url = nullptr;
                if (!reader.read_f32(image.x) || !reader.read_f32(image.y)
                    || !reader.read_f32(image.w) || !reader.read_f32(image.h)
                    || !reader.read_u32(url_len)
                    || !reader.read_text(url, url_len))
                    return false;
                image.url.assign(url ? url : "", url_len);
                scratch.images.push_back(std::move(image));
                break;
            }
            default:
                return false;
        }
    }
    return true;
}

inline void mark_image_cache_dirty(ImageAtlasCache& cache,
                                   int x, int y, int w, int h) {
    cache.dirty = true;
    if (x < cache.dirty_min_x) cache.dirty_min_x = x;
    if (y < cache.dirty_min_y) cache.dirty_min_y = y;
    if (x + w > cache.dirty_max_x) cache.dirty_max_x = x + w;
    if (y + h > cache.dirty_max_y) cache.dirty_max_y = y + h;
}

inline bool reserve_image_slot(ImageAtlasCache& cache, int width, int height,
                               int& out_x, int& out_y) {
    if (width <= 0 || height <= 0
        || width > ImageAtlasCache::atlas_size
        || height > ImageAtlasCache::atlas_size) {
        return false;
    }

    if (cache.pixels.size()
        != static_cast<std::size_t>(ImageAtlasCache::atlas_size * ImageAtlasCache::atlas_size * 4)) {
        cache.pixels.assign(
            static_cast<std::size_t>(ImageAtlasCache::atlas_size * ImageAtlasCache::atlas_size * 4),
            0);
    }

    if (cache.cursor_x + width > ImageAtlasCache::atlas_size) {
        cache.cursor_x = 0;
        cache.cursor_y += cache.row_height;
        cache.row_height = 0;
    }
    if (cache.cursor_y + height > ImageAtlasCache::atlas_size)
        return false;

    out_x = cache.cursor_x;
    out_y = cache.cursor_y;
    cache.cursor_x += width;
    if (height > cache.row_height) cache.row_height = height;
    return true;
}

inline std::uint16_t read_le16(std::uint8_t const* ptr) noexcept {
    return static_cast<std::uint16_t>(ptr[0])
        | (static_cast<std::uint16_t>(ptr[1]) << 8);
}

inline std::uint32_t read_le32(std::uint8_t const* ptr) noexcept {
    return static_cast<std::uint32_t>(ptr[0])
        | (static_cast<std::uint32_t>(ptr[1]) << 8)
        | (static_cast<std::uint32_t>(ptr[2]) << 16)
        | (static_cast<std::uint32_t>(ptr[3]) << 24);
}

inline bool decode_bmp_memory(std::vector<std::uint8_t> const& bytes,
                              DecodedImage& out) {
    if (bytes.size() < 54)
        return false;
    if (bytes[0] != 'B' || bytes[1] != 'M')
        return false;

    auto const dib_size = read_le32(bytes.data() + 14);
    if (dib_size < 40)
        return false;

    auto const data_offset = read_le32(bytes.data() + 10);
    auto const raw_width = static_cast<std::int32_t>(read_le32(bytes.data() + 18));
    auto const raw_height = static_cast<std::int32_t>(read_le32(bytes.data() + 22));
    auto const planes = read_le16(bytes.data() + 26);
    auto const bits_per_pixel = read_le16(bytes.data() + 28);
    auto const compression = read_le32(bytes.data() + 30);

    if (planes != 1 || raw_width <= 0 || raw_height == 0 || compression != 0)
        return false;
    if (bits_per_pixel != 24 && bits_per_pixel != 32)
        return false;

    auto const top_down = raw_height < 0;
    auto const height = top_down ? -raw_height : raw_height;
    auto const width = raw_width;
    auto const bytes_per_pixel = bits_per_pixel / 8;
    auto const row_stride = static_cast<std::size_t>(((width * bits_per_pixel + 31) / 32) * 4);
    auto const required = static_cast<std::size_t>(data_offset)
        + row_stride * static_cast<std::size_t>(height);
    if (required > bytes.size())
        return false;

    out.width = width;
    out.height = height;
    out.failed = false;
    out.pixels.assign(static_cast<std::size_t>(width * height * 4), 0);

    for (int row = 0; row < height; ++row) {
        int const src_row = top_down ? row : (height - 1 - row);
        auto const* src = bytes.data()
            + static_cast<std::size_t>(data_offset)
            + static_cast<std::size_t>(src_row) * row_stride;
        auto* dst = out.pixels.data()
            + static_cast<std::size_t>(row * width * 4);

        for (int col = 0; col < width; ++col) {
            auto const pixel_index = static_cast<std::size_t>(col * bytes_per_pixel);
            dst[col * 4 + 0] = src[pixel_index + 2];
            dst[col * 4 + 1] = src[pixel_index + 1];
            dst[col * 4 + 2] = src[pixel_index + 0];
            dst[col * 4 + 3] = (bytes_per_pixel == 4)
                ? src[pixel_index + 3]
                : static_cast<std::uint8_t>(255);
        }
    }

    return true;
}

inline bool decode_bmp_file(std::filesystem::path const& path, DecodedImage& out) {
    auto file = std::fopen(path.string().c_str(), "rb");
    if (!file)
        return false;

    std::vector<std::uint8_t> bytes;
    std::array<std::uint8_t, 8192> chunk{};
    while (true) {
        auto const read = std::fread(chunk.data(), 1, chunk.size(), file);
        if (read > 0) {
            bytes.insert(bytes.end(), chunk.begin(), chunk.begin() + static_cast<std::ptrdiff_t>(read));
        }
        if (read < chunk.size()) {
            if (std::ferror(file)) {
                std::fclose(file);
                return false;
            }
            break;
        }
    }
    std::fclose(file);
    return decode_bmp_memory(bytes, out);
}

inline void ensure_image_worker();

inline void queue_image_load(std::string const& url) {
    auto [it, inserted] = g_images.cache.try_emplace(url, ImageCacheEntry{});
    if (!inserted)
        return;
    ensure_image_worker();
    {
        std::lock_guard lock(g_images.mutex);
        g_images.pending_jobs.push_back(url);
    }
    g_images.cv.notify_one();
}

inline bool store_decoded_image(DecodedImage decoded) {
    auto [it, inserted] = g_images.cache.try_emplace(decoded.url, ImageCacheEntry{});
    if (!inserted && it->second.state == ImageEntryState::ready)
        return false;

    if (decoded.failed || decoded.pixels.empty()
        || decoded.width <= 0 || decoded.height <= 0) {
        bool changed = it->second.state != ImageEntryState::failed;
        it->second = ImageCacheEntry{.state = ImageEntryState::failed};
        return changed;
    }

    int slot_x = 0;
    int slot_y = 0;
    if (!reserve_image_slot(g_images, decoded.width, decoded.height, slot_x, slot_y)) {
        bool changed = it->second.state != ImageEntryState::failed;
        it->second = ImageCacheEntry{.state = ImageEntryState::failed};
        return changed;
    }

    for (int row = 0; row < decoded.height; ++row) {
        auto* dst = g_images.pixels.data()
            + static_cast<std::size_t>(
                ((slot_y + row) * ImageAtlasCache::atlas_size + slot_x) * 4);
        auto const* src = decoded.pixels.data()
            + static_cast<std::size_t>(row * decoded.width * 4);
        std::memcpy(dst, src, static_cast<std::size_t>(decoded.width * 4));
    }
    mark_image_cache_dirty(g_images, slot_x, slot_y, decoded.width, decoded.height);

    it->second.state = ImageEntryState::ready;
    it->second.u = static_cast<float>(slot_x) / ImageAtlasCache::atlas_size;
    it->second.v = static_cast<float>(slot_y) / ImageAtlasCache::atlas_size;
    it->second.uw = static_cast<float>(decoded.width) / ImageAtlasCache::atlas_size;
    it->second.vh = static_cast<float>(decoded.height) / ImageAtlasCache::atlas_size;
    return true;
}

inline bool process_completed_images() {
    std::deque<DecodedImage> completed;
    {
        std::lock_guard lock(g_images.mutex);
        if (g_images.completed_jobs.empty())
            return false;
        completed.swap(g_images.completed_jobs);
    }

    bool changed = false;
    while (!completed.empty()) {
        changed = store_decoded_image(std::move(completed.front())) || changed;
        completed.pop_front();
    }
    return changed;
}

inline void image_worker_main() {
    for (;;) {
        std::string url;
        {
            std::unique_lock lock(g_images.mutex);
            g_images.cv.wait(lock, [] {
                return g_images.stop_worker || !g_images.pending_jobs.empty();
            });
            if (g_images.stop_worker && g_images.pending_jobs.empty())
                break;
            url = std::move(g_images.pending_jobs.front());
            g_images.pending_jobs.pop_front();
        }

        DecodedImage decoded;
        decoded.url = url;
        if (auto response = cppx::http::system::get(url);
            response && response->stat.ok()) {
            std::vector<std::uint8_t> body;
            body.reserve(response->body.size());
            for (auto byte : response->body)
                body.push_back(static_cast<std::uint8_t>(byte));
            decoded.failed = !decode_bmp_memory(body, decoded);
        } else {
            decoded.failed = true;
        }

        {
            std::lock_guard lock(g_images.mutex);
            g_images.completed_jobs.push_back(std::move(decoded));
        }
    }
}

inline void ensure_image_worker() {
    if (g_images.worker_started)
        return;
    g_images.stop_worker = false;
    g_images.worker = std::thread(image_worker_main);
    g_images.worker_started = true;
}

inline void shutdown_image_worker() {
    {
        std::lock_guard lock(g_images.mutex);
        g_images.stop_worker = true;
        g_images.pending_jobs.clear();
    }
    g_images.cv.notify_all();
    if (g_images.worker.joinable())
        g_images.worker.join();
    g_images.worker_started = false;
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

    auto font = copy_text_font(font_size * scale, mono);
    if (!font)
        return false;
    auto line = create_text_line(font, text_ptr, len);
    if (!line)
        return false;

    TextLineMetrics line_metrics;
    if (!describe_text_line(line, scale, line_metrics))
        return false;

    int padding = static_cast<int>(std::ceil(scale)) + 1;
    if (padding < 2) padding = 2;

    auto box = make_line_box(
        line_metrics.logical_width,
        line_height > 0.0f ? line_height : font_size * 1.6f,
        line_metrics.ascent,
        line_metrics.descent,
        line_metrics.leading,
        scale,
        padding);
    int line_origin_x = padding;
    expand_line_box_for_ink(
        box,
        line_metrics.glyph_bounds,
        line_metrics.logical_width,
        scale,
        padding,
        line_origin_x);

    std::vector<std::uint8_t> slot_alpha;
    int ink_min_x = box.slot_width;
    int ink_max_x = -1;
    if (!rasterize_line_alpha(
            line,
            box.slot_width,
            box.slot_height,
            line_origin_x,
            box.baseline_y,
            slot_alpha,
            ink_min_x,
            ink_max_x)
        || ink_max_x < ink_min_x) {
        return false;
    }

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

inline TextVsOut make_textured_vs(
    uint vi,
    uint ii,
    constant Uniforms& u,
    const device TextInstance* instances
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

vertex TextVsOut vs_text(
    uint vi [[vertex_id]],
    uint ii [[instance_id]],
    constant Uniforms& u [[buffer(0)]],
    const device TextInstance* instances [[buffer(1)]]
) {
    return make_textured_vs(vi, ii, u, instances);
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

vertex TextVsOut vs_image(
    uint vi [[vertex_id]],
    uint ii [[instance_id]],
    constant Uniforms& u [[buffer(0)]],
    const device TextInstance* instances [[buffer(1)]]
) {
    return make_textured_vs(vi, ii, u, instances);
}

fragment float4 fs_image(
    TextVsOut in [[stage_in]],
    texture2d<float> atlas [[texture(0)]],
    sampler samp [[sampler(0)]]
) {
    float4 sample = atlas.sample(samp, in.uv);
    if (sample.a < 0.01) discard_fragment();
    return sample * in.color;
}
)";

struct RendererState {
    MTL::Device* device = nullptr;
    MTL::CommandQueue* queue = nullptr;
    CA::MetalLayer* layer = nullptr;
    MTL::RenderPipelineState* color_pipeline = nullptr;
    MTL::RenderPipelineState* image_pipeline = nullptr;
    MTL::RenderPipelineState* text_pipeline = nullptr;
    MTL::Buffer* uniform_buf = nullptr;
    MTL::Buffer* color_instances_buf = nullptr;
    std::size_t color_instances_capacity = 0;
    MTL::Buffer* image_instances_buf = nullptr;
    std::size_t image_instances_capacity = 0;
    MTL::Buffer* text_instances_buf = nullptr;
    std::size_t text_instances_capacity = 0;
    MTL::Texture* image_atlas_texture = nullptr;
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

inline bool ensure_image_atlas_texture() {
    if (g_renderer.image_atlas_texture)
        return true;

    auto* tex_desc = MTL::TextureDescriptor::texture2DDescriptor(
        MTL::PixelFormatRGBA8Unorm,
        NS::UInteger(ImageAtlasCache::atlas_size),
        NS::UInteger(ImageAtlasCache::atlas_size),
        false);
    tex_desc->setUsage(MTL::TextureUsageShaderRead);
    g_renderer.image_atlas_texture = g_renderer.device->newTexture(tex_desc);
    return g_renderer.image_atlas_texture != nullptr;
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

inline bool upload_image_cache() {
    auto& cache = g_images;
    if (!cache.dirty)
        return true;
    if (!ensure_image_atlas_texture())
        return false;

    auto width = cache.dirty_max_x - cache.dirty_min_x;
    auto height = cache.dirty_max_y - cache.dirty_min_y;
    if (width <= 0 || height <= 0) {
        cache.dirty = false;
        cache.dirty_min_x = ImageAtlasCache::atlas_size;
        cache.dirty_min_y = ImageAtlasCache::atlas_size;
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
            (cache.dirty_min_y * ImageAtlasCache::atlas_size + cache.dirty_min_x) * 4);
    g_renderer.image_atlas_texture->replaceRegion(
        region,
        0,
        src,
        ImageAtlasCache::atlas_size * 4);
    metrics::inst::native_texture_upload_bytes.add(
        static_cast<std::uint64_t>(width * height * 4),
        native_platform_attrs());

    cache.dirty = false;
    cache.dirty_min_x = ImageAtlasCache::atlas_size;
    cache.dirty_min_y = ImageAtlasCache::atlas_size;
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

inline void prepare_image_instances(float scale) {
    auto& scratch = g_renderer.scratch;
    scratch.image_instances.clear();

    for (auto const& image : scratch.images) {
        auto it = g_images.cache.find(image.url);
        if (it == g_images.cache.end()) {
            it = g_images.cache.try_emplace(image.url, ImageCacheEntry{}).first;
            if (is_http_url(image.url)) {
                queue_image_load(image.url);
            } else {
                DecodedImage decoded;
                decoded.url = image.url;
                decoded.failed = !decode_bmp_file(resolve_image_path(image.url), decoded);
                (void)store_decoded_image(std::move(decoded));
            }
            it = g_images.cache.find(image.url);
        }

        if (it != g_images.cache.end() && it->second.state == ImageEntryState::ready) {
            append_image_instance(
                scratch.image_instances,
                snap_to_pixel_grid(image.x, scale),
                snap_to_pixel_grid(image.y, scale),
                image.w,
                image.h,
                it->second.u,
                it->second.v,
                it->second.uw,
                it->second.vh);
            continue;
        }

        bool failed = it != g_images.cache.end()
            && it->second.state == ImageEntryState::failed;
        float fill = failed ? 0.90f : 0.94f;
        float edge = failed ? 0.78f : 0.82f;
        append_color_instance(
            scratch.color_instances,
            image.x, image.y, image.w, image.h,
            fill, fill, fill, 1.0f,
            6.0f, 0.0f, 2.0f);
        append_color_instance(
            scratch.color_instances,
            image.x, image.y, image.w, image.h,
            edge, edge, edge, 1.0f,
            0.0f, 1.0f, 1.0f);
    }
}

inline void input_attach(GLFWwindow*, void (*request_repaint)()) {
    g_images.request_repaint = request_repaint;
}

inline void input_detach() {
    g_images.request_repaint = nullptr;
}

inline void input_sync() {
    if (process_completed_images() && g_images.request_repaint)
        g_images.request_repaint();
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
    g_renderer.image_pipeline = create_pipeline(g_renderer.device, lib, "vs_image", "fs_image");
    g_renderer.text_pipeline = create_pipeline(g_renderer.device, lib, "vs_text", "fs_text");
    lib->release();
    if (!g_renderer.color_pipeline || !g_renderer.image_pipeline || !g_renderer.text_pipeline) {
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
    (void)process_completed_images();
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

    auto image_started = metrics::detail::now_ns();
    prepare_image_instances(text_scale);
    metrics::inst::native_phase_duration.record(
        metrics::detail::now_ns() - image_started,
        native_attrs("image_prepare"));

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

    auto image_upload_started = metrics::detail::now_ns();
    if (!upload_image_cache()) {
        pool->release();
        return;
    }
    metrics::inst::native_phase_duration.record(
        metrics::detail::now_ns() - image_upload_started,
        native_attrs("image_upload"));

    auto upload_started = metrics::detail::now_ns();
    if (!upload_text_cache()) {
        pool->release();
        return;
    }
    metrics::inst::native_phase_duration.record(
        metrics::detail::now_ns() - upload_started,
        native_attrs("text_upload"));

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

    auto const image_bytes = scratch.image_instances.size() * sizeof(ImageInstanceGPU);
    if (!scratch.image_instances.empty() && g_renderer.image_atlas_texture) {
        if (!ensure_instance_buffer(
                g_renderer.image_instances_buf,
                g_renderer.image_instances_capacity,
                image_bytes,
                "image_instances")) {
            encoder->endEncoding();
            pool->release();
            return;
        }
        std::memcpy(
            g_renderer.image_instances_buf->contents(),
            scratch.image_instances.data(),
            image_bytes);
        encoder->setRenderPipelineState(g_renderer.image_pipeline);
        encoder->setVertexBuffer(g_renderer.uniform_buf, 0, 0);
        encoder->setVertexBuffer(g_renderer.image_instances_buf, 0, 1);
        encoder->setFragmentTexture(g_renderer.image_atlas_texture, 0);
        encoder->setFragmentSamplerState(g_renderer.sampler, 0);
        encoder->drawPrimitives(
            MTL::PrimitiveTypeTriangle,
            NS::UInteger(0), 6, NS::UInteger(scratch.image_instances.size()));
    }

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
    if (g_renderer.image_atlas_texture) { g_renderer.image_atlas_texture->release(); g_renderer.image_atlas_texture = nullptr; }
    if (g_renderer.text_atlas_texture) { g_renderer.text_atlas_texture->release(); g_renderer.text_atlas_texture = nullptr; }
    if (g_renderer.image_instances_buf) { g_renderer.image_instances_buf->release(); g_renderer.image_instances_buf = nullptr; }
    if (g_renderer.text_instances_buf) { g_renderer.text_instances_buf->release(); g_renderer.text_instances_buf = nullptr; }
    if (g_renderer.color_instances_buf) { g_renderer.color_instances_buf->release(); g_renderer.color_instances_buf = nullptr; }
    if (g_renderer.uniform_buf) { g_renderer.uniform_buf->release(); g_renderer.uniform_buf = nullptr; }
    if (g_renderer.image_pipeline) { g_renderer.image_pipeline->release(); g_renderer.image_pipeline = nullptr; }
    if (g_renderer.text_pipeline) { g_renderer.text_pipeline->release(); g_renderer.text_pipeline = nullptr; }
    if (g_renderer.color_pipeline) { g_renderer.color_pipeline->release(); g_renderer.color_pipeline = nullptr; }
    if (g_renderer.layer) { g_renderer.layer->release(); g_renderer.layer = nullptr; }
    if (g_renderer.queue) { g_renderer.queue->release(); g_renderer.queue = nullptr; }
    if (g_renderer.device) { g_renderer.device->release(); g_renderer.device = nullptr; }
    shutdown_image_worker();
    g_renderer.hit_regions.clear();
    g_renderer.scratch.clear();
    g_images.cache.clear();
    g_images.completed_jobs.clear();
    g_images.pixels.clear();
    g_images.cursor_x = 0;
    g_images.cursor_y = 0;
    g_images.row_height = 0;
    g_images.dirty = false;
    g_images.dirty_min_x = ImageAtlasCache::atlas_size;
    g_images.dirty_min_y = ImageAtlasCache::atlas_size;
    g_images.dirty_max_x = 0;
    g_images.dirty_max_y = 0;
    g_images.request_repaint = nullptr;
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
    g_renderer.image_instances_capacity = 0;
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
        {
            detail::input_attach,
            detail::input_detach,
            detail::input_sync,
            nullptr,
            nullptr,
            nullptr,
        },
        [](char const* url, unsigned int len) {
            auto opened = cppx::os::system::open_url(std::string_view(url, len));
            if (!opened) {
                std::fprintf(
                    stderr,
                    "[macos] failed to open url: %.*s (%.*s)\n",
                    static_cast<int>(len),
                    url,
                    static_cast<int>(cppx::os::to_string(opened.error()).size()),
                    cppx::os::to_string(opened.error()).data());
            }
        },
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
