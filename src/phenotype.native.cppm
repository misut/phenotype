// Native backend — Metal renderer + CoreText text (macOS).
// On WASM targets this compiles as an empty module.

module;
#ifndef __wasi__
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
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
#endif // __APPLE__

#endif // !__wasi__

export module phenotype.native;

#ifndef __wasi__
export import phenotype;

// ============================================================
// RAII guard for CoreFoundation types
// ============================================================

#ifdef __APPLE__
namespace phenotype::native::detail {

template<typename T>
struct CFGuard {
    T ref;
    CFGuard(T r) : ref(r) {}
    ~CFGuard() { if (ref) CFRelease(ref); }
    CFGuard(CFGuard const&) = delete;
    CFGuard& operator=(CFGuard const&) = delete;
    operator T() const { return ref; }
    explicit operator bool() const { return ref != nullptr; }
};

} // namespace phenotype::native::detail
#endif // __APPLE__

// ============================================================
// Text subsystem — CoreText on macOS, stub elsewhere
// ============================================================

export namespace phenotype::native::text {

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
    float line_height = 0;
};

#ifdef __APPLE__

struct TextState {
    CTFontRef sans = nullptr;
    CTFontRef mono = nullptr;
    bool initialized = false;
};
inline TextState g_text;

inline float sanitize_scale(float scale) {
    return (scale > 0.f && std::isfinite(scale)) ? scale : 1.f;
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

inline LineBoxMetrics make_line_box(float logical_width, float logical_line_height,
                                    float ascent, float descent, float leading,
                                    float scale, int padding) {
    LineBoxMetrics box;
    float natural_line_height = ascent + descent + leading;
    int text_width_px = logical_pixels(logical_width, scale, 0);
    int line_height_px = logical_pixels(logical_line_height, scale);
    int natural_height_px = static_cast<int>(std::ceil(natural_line_height));
    if (natural_height_px < 1) natural_height_px = 1;

    box.slot_width = text_width_px + padding * 2;
    if (box.slot_width < padding * 2 + 1) box.slot_width = padding * 2 + 1;

    box.render_top = padding;
    box.render_height = (line_height_px > natural_height_px)
        ? line_height_px : natural_height_px;
    box.slot_height = box.render_height + padding * 2;

    float extra_leading = static_cast<float>(box.render_height) - natural_line_height;
    if (extra_leading < 0.f) extra_leading = 0.f;
    box.baseline_y = static_cast<float>(box.render_top) + extra_leading * 0.5f + ascent;
    return box;
}

inline void init() {
    if (g_text.initialized) return;
    g_text.sans = CTFontCreateUIFontForLanguage(kCTFontUIFontSystem, 16.0, nullptr);
    g_text.mono = CTFontCreateUIFontForLanguage(kCTFontUIFontUserFixedPitch, 16.0, nullptr);
    if (!g_text.sans) {
        std::fprintf(stderr, "[text] failed to create system font\n");
        return;
    }
    if (!g_text.mono) g_text.mono = static_cast<CTFontRef>(CFRetain(g_text.sans));
    g_text.initialized = true;
}

inline void shutdown() {
    if (g_text.sans) { CFRelease(g_text.sans); g_text.sans = nullptr; }
    if (g_text.mono) { CFRelease(g_text.mono); g_text.mono = nullptr; }
    g_text.initialized = false;
}

inline unsigned int decode_utf8(char const* src, unsigned int byte_len,
                                std::vector<uint32_t>& out) {
    out.clear();
    for (unsigned int i = 0; i < byte_len; ) {
        uint32_t cp;
        auto c = static_cast<unsigned char>(src[i]);
        if (c < 0x80)      { cp = c; i += 1; }
        else if (c < 0xE0) { cp = (c & 0x1F) << 6;
            if (i+1 < byte_len) cp |= (src[i+1] & 0x3F); i += 2; }
        else if (c < 0xF0) { cp = (c & 0x0F) << 12;
            if (i+1 < byte_len) cp |= (src[i+1] & 0x3F) << 6;
            if (i+2 < byte_len) cp |= (src[i+2] & 0x3F); i += 3; }
        else { cp = (c & 0x07) << 18;
            if (i+1 < byte_len) cp |= (src[i+1] & 0x3F) << 12;
            if (i+2 < byte_len) cp |= (src[i+2] & 0x3F) << 6;
            if (i+3 < byte_len) cp |= (src[i+3] & 0x3F); i += 4; }
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
        } else {
            cp -= 0x10000;
            out.push_back(static_cast<UniChar>(0xD800 + (cp >> 10)));
            out.push_back(static_cast<UniChar>(0xDC00 + (cp & 0x3FF)));
        }
    }
}

inline float measure(float font_size, bool mono,
                     char const* text_ptr, unsigned int len) {
    if (!g_text.initialized || len == 0) return 0.f;

    CTFontRef base = mono ? g_text.mono : g_text.sans;
    detail::CFGuard<CTFontRef> font(
        CTFontCreateCopyWithAttributes(base, font_size, nullptr, nullptr));
    if (!font) return 0.f;

    static thread_local std::vector<uint32_t> codepoints;
    static thread_local std::vector<UniChar> unichars;
    decode_utf8(text_ptr, len, codepoints);
    codepoints_to_unichars(codepoints, unichars);

    auto count = static_cast<CFIndex>(unichars.size());
    std::vector<CGGlyph> glyphs(count);
    CTFontGetGlyphsForCharacters(font, unichars.data(), glyphs.data(), count);

    std::vector<CGSize> advances(count);
    CTFontGetAdvancesForGlyphs(font, kCTFontOrientationHorizontal,
                                glyphs.data(), advances.data(), count);

    double width = 0;
    for (CFIndex i = 0; i < count; ++i) width += advances[i].width;
    return static_cast<float>(width);
}

inline TextAtlas build_atlas(std::vector<TextEntry> const& entries,
                             float backing_scale = 1.0f) {
    if (entries.empty()) return {};

    float scale = sanitize_scale(backing_scale);
    static constexpr int ATLAS_SIZE = 2048;
    TextAtlas atlas;
    atlas.width = ATLAS_SIZE;
    atlas.height = ATLAS_SIZE;
    atlas.pixels.resize(ATLAS_SIZE * ATLAS_SIZE * 4, 0);

    int ax = 0, ay = 0, row_height = 0;
    int padding = static_cast<int>(std::ceil(scale)) + 1;
    if (padding < 2) padding = 2;

    static thread_local std::vector<uint32_t> codepoints;
    static thread_local std::vector<UniChar> unichars;

    for (auto const& e : entries) {
        if (e.text.empty()) continue;

        CTFontRef base = e.mono ? g_text.mono : g_text.sans;
        detail::CFGuard<CTFontRef> font(
            CTFontCreateCopyWithAttributes(base, e.font_size * scale, nullptr, nullptr));
        if (!font) continue;

        float ascent = static_cast<float>(CTFontGetAscent(font));
        float descent = static_cast<float>(CTFontGetDescent(font));
        float leading = static_cast<float>(CTFontGetLeading(font));
        float logical_line_height = e.line_height > 0
            ? e.line_height
            : e.font_size * 1.6f;
        float snapped_x = snap_to_pixel_grid(e.x, scale);
        float snapped_y = snap_to_pixel_grid(e.y, scale);

        float tw_f = measure(e.font_size, e.mono, e.text.c_str(),
            static_cast<unsigned int>(e.text.size()));
        auto box = make_line_box(tw_f, logical_line_height, ascent, descent, leading,
            scale, padding);

        if (ax + box.slot_width > ATLAS_SIZE) { ax = 0; ay += row_height; row_height = 0; }
        if (ay + box.slot_height > ATLAS_SIZE) break;

        decode_utf8(e.text.c_str(), static_cast<unsigned int>(e.text.size()), codepoints);
        codepoints_to_unichars(codepoints, unichars);
        auto count = static_cast<CFIndex>(unichars.size());
        std::vector<CGGlyph> glyphs(count);
        CTFontGetGlyphsForCharacters(font, unichars.data(), glyphs.data(), count);

        std::vector<CGSize> advances(count);
        CTFontGetAdvancesForGlyphs(font, kCTFontOrientationHorizontal,
                                    glyphs.data(), advances.data(), count);

        bool has_ink = false;
        int ink_min_x = box.slot_width;
        int ink_max_x = -1;
        float pen_x = static_cast<float>(padding);
        for (CFIndex gi = 0; gi < count; ++gi) {
            if (glyphs[gi] == 0) { pen_x += static_cast<float>(advances[gi].width); continue; }

            CGRect bbox;
            CTFontGetBoundingRectsForGlyphs(font, kCTFontOrientationHorizontal,
                                             &glyphs[gi], &bbox, 1);
            int gw = static_cast<int>(std::ceil(bbox.size.width)) + 2;
            int gh = static_cast<int>(std::ceil(bbox.size.height)) + 2;
            if (gw <= 0 || gh <= 0) { pen_x += static_cast<float>(advances[gi].width); continue; }

            std::vector<uint8_t> glyph_buf(gw * gh, 0);
            auto* ctx = CGBitmapContextCreate(
                glyph_buf.data(), gw, gh, 8, gw, nullptr, kCGImageAlphaOnly);
            if (!ctx) { pen_x += static_cast<float>(advances[gi].width); continue; }

            CGPoint pos = CGPointMake(-bbox.origin.x + 1, -bbox.origin.y + 1);
            CTFontDrawGlyphs(font, &glyphs[gi], &pos, 1, ctx);
            CGContextRelease(ctx);

            int gx = static_cast<int>(std::lround(pen_x + bbox.origin.x));
            int gy = static_cast<int>(std::lround(
                box.baseline_y - bbox.origin.y - bbox.size.height));

            for (int row = 0; row < gh; ++row) {
                for (int col = 0; col < gw; ++col) {
                    int local_x = gx + col;
                    int local_y = gy + row;
                    if (local_x >= 0 && local_x < box.slot_width &&
                        local_y >= 0 && local_y < box.slot_height) {
                        int px = ax + local_x;
                        int py = ay + local_y;
                        if (px < 0 || px >= ATLAS_SIZE || py < 0 || py >= ATLAS_SIZE)
                            continue;
                        uint8_t alpha = glyph_buf[row * gw + col];
                        if (alpha == 0) continue;
                        has_ink = true;
                        if (local_x < ink_min_x) ink_min_x = local_x;
                        if (local_x > ink_max_x) ink_max_x = local_x;
                        int idx = (py * ATLAS_SIZE + px) * 4;
                        atlas.pixels[idx + 0] = static_cast<uint8_t>(e.r * 255.f * alpha / 255.f);
                        atlas.pixels[idx + 1] = static_cast<uint8_t>(e.g * 255.f * alpha / 255.f);
                        atlas.pixels[idx + 2] = static_cast<uint8_t>(e.b * 255.f * alpha / 255.f);
                        atlas.pixels[idx + 3] = alpha;
                    }
                }
            }
            pen_x += static_cast<float>(advances[gi].width);
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

#else // !__APPLE__

inline void init() {}
inline void shutdown() {}
inline float measure(float, bool, char const*, unsigned int) { return 0.f; }
inline TextAtlas build_atlas(std::vector<TextEntry> const&, float = 1.0f) { return {}; }

#endif // __APPLE__

} // namespace phenotype::native::text

// ============================================================
// Renderer — Metal on macOS, stub elsewhere
// ============================================================

export namespace phenotype::native::renderer {

#ifdef __APPLE__

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
};

struct TextInstance {
    float4 rect;
    float4 uv_rect;
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
    return out;
}

fragment float4 fs_text(
    TextVsOut in [[stage_in]],
    texture2d<float> atlas [[texture(0)]],
    sampler samp [[sampler(0)]]
) {
    float4 s = atlas.sample(samp, in.uv);
    if (s.a < 0.01) discard_fragment();
    return s;
}
)";

struct RendererState {
    MTL::Device*              device = nullptr;
    MTL::CommandQueue*        queue = nullptr;
    CA::MetalLayer*           layer = nullptr;
    MTL::RenderPipelineState* color_pipeline = nullptr;
    MTL::RenderPipelineState* text_pipeline = nullptr;
    MTL::Buffer*              uniform_buf = nullptr;
    MTL::SamplerState*        sampler = nullptr;
    std::vector<HitRegionCmd> hit_regions;
    GLFWwindow*               window = nullptr;
    bool initialized = false;
};
inline RendererState g_state;

inline float current_backing_scale(GLFWwindow* window) {
    if (!window) return 1.0f;
    int fbw = 0, fbh = 0;
    int winw = 0, winh = 0;
    glfwGetFramebufferSize(window, &fbw, &fbh);
    glfwGetWindowSize(window, &winw, &winh);
    float sx = (winw > 0) ? static_cast<float>(fbw) / static_cast<float>(winw) : 1.0f;
    float sy = (winh > 0) ? static_cast<float>(fbh) / static_cast<float>(winh) : 1.0f;
    float scale = (sx > sy) ? sx : sy;
    return (scale > 0.0f && std::isfinite(scale)) ? scale : 1.0f;
}

inline MTL::RenderPipelineState* create_pipeline(
    MTL::Device* device, MTL::Library* lib,
    char const* vs_name, char const* fs_name)
{
    auto* vs_fn = lib->newFunction(NS::String::string(vs_name, NS::UTF8StringEncoding));
    auto* fs_fn = lib->newFunction(NS::String::string(fs_name, NS::UTF8StringEncoding));
    if (!vs_fn || !fs_fn) { if (vs_fn) vs_fn->release(); if (fs_fn) fs_fn->release(); return nullptr; }

    auto* desc = MTL::RenderPipelineDescriptor::alloc()->init();
    desc->setVertexFunction(vs_fn);
    desc->setFragmentFunction(fs_fn);

    auto* ca = desc->colorAttachments()->object(0);
    ca->setPixelFormat(MTL::PixelFormatBGRA8Unorm);
    ca->setBlendingEnabled(true);
    ca->setSourceRGBBlendFactor(MTL::BlendFactorSourceAlpha);
    ca->setDestinationRGBBlendFactor(MTL::BlendFactorOneMinusSourceAlpha);
    ca->setRgbBlendOperation(MTL::BlendOperationAdd);
    ca->setSourceAlphaBlendFactor(MTL::BlendFactorOne);
    ca->setDestinationAlphaBlendFactor(MTL::BlendFactorOneMinusSourceAlpha);
    ca->setAlphaBlendOperation(MTL::BlendOperationAdd);

    NS::Error* err = nullptr;
    auto* pso = device->newRenderPipelineState(desc, &err);
    if (err) std::fprintf(stderr, "[metal] pipeline error: %s\n",
        err->localizedDescription()->utf8String());
    desc->release();
    vs_fn->release();
    fs_fn->release();
    return pso;
}

inline void init(GLFWwindow* window) {
    if (g_state.initialized) return;
    g_state.window = window;

    g_state.device = MTL::CreateSystemDefaultDevice();
    if (!g_state.device) { std::fprintf(stderr, "[metal] no device\n"); return; }
    g_state.queue = g_state.device->newCommandQueue();

    // Bridge GLFW window → CAMetalLayer
    void* nswin = glfwGetCocoaWindow(window);
    auto sel_cv = sel_registerName("contentView");
    auto sel_wl = sel_registerName("setWantsLayer:");
    auto sel_sl = sel_registerName("setLayer:");
    void* view = reinterpret_cast<void*(*)(void*, SEL)>(objc_msgSend)(nswin, sel_cv);
    reinterpret_cast<void(*)(void*, SEL, bool)>(objc_msgSend)(view, sel_wl, true);

    g_state.layer = CA::MetalLayer::layer()->retain();
    g_state.layer->setDevice(g_state.device);
    g_state.layer->setPixelFormat(MTL::PixelFormatBGRA8Unorm);

    int fbw, fbh;
    glfwGetFramebufferSize(window, &fbw, &fbh);
    g_state.layer->setDrawableSize(CGSizeMake(fbw, fbh));

    reinterpret_cast<void(*)(void*, SEL, void*)>(objc_msgSend)(
        view, sel_sl, static_cast<void*>(g_state.layer));

    // Compile shaders
    NS::Error* err = nullptr;
    auto* lib = g_state.device->newLibrary(
        NS::String::string(MSL_SHADERS, NS::UTF8StringEncoding), nullptr, &err);
    if (!lib) {
        std::fprintf(stderr, "[metal] shader compile: %s\n",
            err ? err->localizedDescription()->utf8String() : "unknown");
        return;
    }

    g_state.color_pipeline = create_pipeline(g_state.device, lib, "vs_color", "fs_color");
    g_state.text_pipeline = create_pipeline(g_state.device, lib, "vs_text", "fs_text");
    lib->release();
    if (!g_state.color_pipeline || !g_state.text_pipeline) {
        std::fprintf(stderr, "[metal] failed to create render pipelines\n");
        return;
    }

    g_state.uniform_buf = g_state.device->newBuffer(16, MTL::ResourceStorageModeShared);

    auto* sd = MTL::SamplerDescriptor::alloc()->init();
    sd->setMinFilter(MTL::SamplerMinMagFilterLinear);
    sd->setMagFilter(MTL::SamplerMinMagFilterLinear);
    sd->setSAddressMode(MTL::SamplerAddressModeClampToEdge);
    sd->setTAddressMode(MTL::SamplerAddressModeClampToEdge);
    g_state.sampler = g_state.device->newSamplerState(sd);
    sd->release();

    g_state.initialized = true;

    int winw, winh;
    glfwGetWindowSize(window, &winw, &winh);
    std::printf("[phenotype-native] Metal initialized (%dx%d)\n", winw, winh);
}

inline void flush(unsigned char const* buf, unsigned int len) {
    if (len == 0 || !g_state.initialized) return;

    NS::AutoreleasePool* pool = NS::AutoreleasePool::alloc()->init();

    auto cmds = parse_commands(buf, len);
    float text_scale = current_backing_scale(g_state.window);
    float line_height_ratio = ::phenotype::detail::g_app.theme.line_height_ratio;

    double cr = 0.98, cg = 0.98, cb = 0.98, ca = 1.0;
    std::vector<float> colorData;
    std::vector<text::TextEntry> textEntries;
    g_state.hit_regions.clear();

    for (auto& cmd : cmds) {
        if (auto* c = std::get_if<ClearCmd>(&cmd)) {
            cr = c->color.r / 255.0; cg = c->color.g / 255.0;
            cb = c->color.b / 255.0; ca = c->color.a / 255.0;
        } else if (auto* t = std::get_if<DrawTextCmd>(&cmd)) {
            textEntries.push_back({t->x, t->y, t->font_size, t->mono,
                t->color.r/255.f, t->color.g/255.f, t->color.b/255.f, t->color.a/255.f,
                t->text, t->font_size * line_height_ratio});
        } else if (auto* hr = std::get_if<HitRegionCmd>(&cmd)) {
            g_state.hit_regions.push_back(*hr);
        } else if (auto* r = std::get_if<FillRectCmd>(&cmd)) {
            colorData.insert(colorData.end(), {
                r->x, r->y, r->w, r->h,
                r->color.r/255.f, r->color.g/255.f, r->color.b/255.f, r->color.a/255.f,
                0, 0, 0, 0 });
        } else if (auto* s = std::get_if<StrokeRectCmd>(&cmd)) {
            colorData.insert(colorData.end(), {
                s->x, s->y, s->w, s->h,
                s->color.r/255.f, s->color.g/255.f, s->color.b/255.f, s->color.a/255.f,
                0, s->line_width, 1, 0 });
        } else if (auto* rr = std::get_if<RoundRectCmd>(&cmd)) {
            colorData.insert(colorData.end(), {
                rr->x, rr->y, rr->w, rr->h,
                rr->color.r/255.f, rr->color.g/255.f, rr->color.b/255.f, rr->color.a/255.f,
                rr->radius, 0, 2, 0 });
        } else if (auto* l = std::get_if<DrawLineCmd>(&cmd)) {
            float dx = l->x2 - l->x1, dy = l->y2 - l->y1;
            float line_len = std::sqrt(dx*dx + dy*dy);
            float w = (dy == 0) ? line_len : l->thickness;
            float h = (dx == 0) ? line_len : l->thickness;
            float x = (dx == 0) ? l->x1 - l->thickness/2 : (l->x1 < l->x2 ? l->x1 : l->x2);
            float y = (dy == 0) ? l->y1 - l->thickness/2 : (l->y1 < l->y2 ? l->y1 : l->y2);
            colorData.insert(colorData.end(), {
                x, y, w, h,
                l->color.r/255.f, l->color.g/255.f, l->color.b/255.f, l->color.a/255.f,
                0, 0, 3, 0 });
        }
    }

    int fbw, fbh;
    glfwGetFramebufferSize(g_state.window, &fbw, &fbh);
    if (fbw == 0 || fbh == 0) { pool->release(); return; }

    g_state.layer->setDrawableSize(CGSizeMake(fbw, fbh));

    int winw, winh;
    glfwGetWindowSize(g_state.window, &winw, &winh);
    float uniforms[4] = {static_cast<float>(winw), static_cast<float>(winh), 0, 0};
    std::memcpy(g_state.uniform_buf->contents(), uniforms, 16);

    auto* drawable = g_state.layer->nextDrawable();
    if (!drawable) { pool->release(); return; }

    auto* rpd = MTL::RenderPassDescriptor::renderPassDescriptor();
    rpd->colorAttachments()->object(0)->setTexture(drawable->texture());
    rpd->colorAttachments()->object(0)->setLoadAction(MTL::LoadActionClear);
    rpd->colorAttachments()->object(0)->setClearColor(MTL::ClearColor::Make(cr, cg, cb, ca));
    rpd->colorAttachments()->object(0)->setStoreAction(MTL::StoreActionStore);

    auto* cmd_buf = g_state.queue->commandBuffer();
    auto* enc = cmd_buf->renderCommandEncoder(rpd);

    uint32_t colorCount = static_cast<uint32_t>(colorData.size() / 12);
    if (colorCount > 0) {
        auto* ibuf = g_state.device->newBuffer(
            colorData.data(), colorData.size() * sizeof(float),
            MTL::ResourceStorageModeShared);
        enc->setRenderPipelineState(g_state.color_pipeline);
        enc->setVertexBuffer(g_state.uniform_buf, 0, 0);
        enc->setVertexBuffer(ibuf, 0, 1);
        enc->drawPrimitives(MTL::PrimitiveTypeTriangle,
            NS::UInteger(0), 6, NS::UInteger(colorCount));
        ibuf->release();
    }

    if (!textEntries.empty()) {
        auto atlas = text::build_atlas(textEntries, text_scale);
        if (!atlas.quads.empty() && !atlas.pixels.empty()) {
            auto* texDesc = MTL::TextureDescriptor::texture2DDescriptor(
                MTL::PixelFormatRGBA8Unorm,
                NS::UInteger(atlas.width), NS::UInteger(atlas.height), false);
            texDesc->setUsage(MTL::TextureUsageShaderRead);
            auto* atlasTex = g_state.device->newTexture(texDesc);

            MTL::Region region = MTL::Region::Make2D(0, 0, atlas.width, atlas.height);
            atlasTex->replaceRegion(region, 0, atlas.pixels.data(), atlas.width * 4);

            std::vector<float> textData;
            textData.reserve(atlas.quads.size() * 8);
            for (auto& q : atlas.quads) {
                textData.insert(textData.end(), {
                    q.x, q.y, q.w, q.h,
                    q.u, q.v, q.uw, q.vh });
            }

            auto* tbuf = g_state.device->newBuffer(
                textData.data(), textData.size() * sizeof(float),
                MTL::ResourceStorageModeShared);

            enc->setRenderPipelineState(g_state.text_pipeline);
            enc->setVertexBuffer(g_state.uniform_buf, 0, 0);
            enc->setVertexBuffer(tbuf, 0, 1);
            enc->setFragmentTexture(atlasTex, 0);
            enc->setFragmentSamplerState(g_state.sampler, 0);
            enc->drawPrimitives(MTL::PrimitiveTypeTriangle,
                NS::UInteger(0), 6, NS::UInteger(atlas.quads.size()));

            tbuf->release();
            atlasTex->release();
        }
    }

    enc->endEncoding();
    cmd_buf->presentDrawable(drawable);
    cmd_buf->commit();

    pool->release();
}

inline void shutdown() {
    if (g_state.sampler)        { g_state.sampler->release(); g_state.sampler = nullptr; }
    if (g_state.uniform_buf)    { g_state.uniform_buf->release(); g_state.uniform_buf = nullptr; }
    if (g_state.text_pipeline)  { g_state.text_pipeline->release(); g_state.text_pipeline = nullptr; }
    if (g_state.color_pipeline) { g_state.color_pipeline->release(); g_state.color_pipeline = nullptr; }
    if (g_state.layer)          { g_state.layer->release(); g_state.layer = nullptr; }
    if (g_state.queue)          { g_state.queue->release(); g_state.queue = nullptr; }
    if (g_state.device)         { g_state.device->release(); g_state.device = nullptr; }
    g_state.hit_regions.clear();
    g_state.window = nullptr;
    g_state.initialized = false;
    text::shutdown();
}

inline std::optional<unsigned int> hit_test(float x, float y, float scroll_y) {
    float wy = y + scroll_y;
    for (int i = static_cast<int>(g_state.hit_regions.size()) - 1; i >= 0; --i) {
        auto& hr = g_state.hit_regions[i];
        if (x >= hr.x && x <= hr.x + hr.w && wy >= hr.y && wy <= hr.y + hr.h)
            return hr.callback_id;
    }
    return std::nullopt;
}

#else // !__APPLE__

inline std::vector<HitRegionCmd> g_hit_regions_linux;

inline void init(GLFWwindow*) {}

inline void flush(unsigned char const* buf, unsigned int len) {
    if (len == 0) return;
    auto cmds = parse_commands(buf, len);
    g_hit_regions_linux.clear();
    for (auto& cmd : cmds) {
        if (auto* hr = std::get_if<HitRegionCmd>(&cmd))
            g_hit_regions_linux.push_back(*hr);
    }
}

inline void shutdown() { g_hit_regions_linux.clear(); }

inline std::optional<unsigned int> hit_test(float x, float y, float scroll_y) {
    float wy = y + scroll_y;
    for (int i = static_cast<int>(g_hit_regions_linux.size()) - 1; i >= 0; --i) {
        auto& hr = g_hit_regions_linux[i];
        if (x >= hr.x && x <= hr.x + hr.w && wy >= hr.y && wy <= hr.y + hr.h)
            return hr.callback_id;
    }
    return std::nullopt;
}

#endif // __APPLE__

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

inline std::optional<unsigned int> hit_test(float x, float y, float scroll_y) {
    return renderer::hit_test(x, y, scroll_y);
}

template<typename State, typename Msg, typename View, typename Update>
    requires std::invocable<View, State const&>
          && std::invocable<Update, State&, Msg>
void run(native_host& host, View view, Update update) {
    ::phenotype::detail::g_open_url = [](char const* url, unsigned int len) {
        (void)url; (void)len;
    };
    text::init();
    renderer::init(host.window);
    phenotype::run<State, Msg>(host, std::move(view), std::move(update));
}

inline void shutdown() {
    renderer::shutdown();
}

// ============================================================
// run_app — self-contained entry point (GLFW lifecycle internal)
// ============================================================

namespace app_detail {

struct AppState {
    native_host* host = nullptr;
    float scroll_y = 0;
    unsigned int hovered_id = 0xFFFFFFFF;
};
inline AppState g_app_state;

inline void on_mouse_button(GLFWwindow* window, int button, int action, int) {
    if (button != GLFW_MOUSE_BUTTON_LEFT || action != GLFW_PRESS) return;
    double mx, my;
    glfwGetCursorPos(window, &mx, &my);
    if (auto h = hit_test(static_cast<float>(mx), static_cast<float>(my), g_app_state.scroll_y)) {
        ::phenotype::detail::set_focus_id(*h);
        ::phenotype::detail::handle_event(*h);
    }
}

inline void on_cursor_pos(GLFWwindow* window, double mx, double my) {
    auto h = hit_test(static_cast<float>(mx), static_cast<float>(my), g_app_state.scroll_y);
    unsigned int id = h.value_or(0xFFFFFFFF);
    if (id != g_app_state.hovered_id) {
        g_app_state.hovered_id = id;
        if (::phenotype::detail::set_hover_id(id))
            ::phenotype::detail::repaint(*g_app_state.host, g_app_state.scroll_y);
    }
    static GLFWcursor* arrow = nullptr;
    static GLFWcursor* hand  = nullptr;
    if (!arrow) arrow = glfwCreateStandardCursor(GLFW_ARROW_CURSOR);
    if (!hand)  hand  = glfwCreateStandardCursor(GLFW_POINTING_HAND_CURSOR);
    glfwSetCursor(window, h ? hand : arrow);
}

inline void on_scroll(GLFWwindow* window, double, double dy) {
    float total = ::phenotype::detail::get_total_height();
    int w, h;
    glfwGetWindowSize(window, &w, &h);
    float max_scroll = total - static_cast<float>(h);
    if (max_scroll < 0) max_scroll = 0;
    g_app_state.scroll_y += static_cast<float>(dy);
    if (g_app_state.scroll_y < 0) g_app_state.scroll_y = 0;
    if (g_app_state.scroll_y > max_scroll) g_app_state.scroll_y = max_scroll;
    ::phenotype::detail::repaint(*g_app_state.host, g_app_state.scroll_y);
}

inline void on_framebuffer_size(GLFWwindow*, int, int) {
    ::phenotype::detail::repaint(*g_app_state.host, g_app_state.scroll_y);
}

inline void on_key(GLFWwindow*, int key, int, int action, int mods) {
    if (action != GLFW_PRESS && action != GLFW_REPEAT) return;
    if (key == GLFW_KEY_TAB) {
        ::phenotype::detail::handle_tab((mods & GLFW_MOD_SHIFT) ? 1 : 0);
        ::phenotype::detail::repaint(*g_app_state.host, g_app_state.scroll_y);
    } else if (key == GLFW_KEY_ENTER) {
        unsigned int fid = ::phenotype::detail::get_focused_id();
        if (fid != 0xFFFFFFFF) ::phenotype::detail::handle_event(fid);
    }
}

} // namespace app_detail

// Self-contained app runner. Owns the GLFW window and event loop.
// Returns 0 on success, 1 on failure.
template<typename State, typename Msg, typename View, typename Update>
    requires std::invocable<View, State const&>
          && std::invocable<Update, State&, Msg>
int run_app(int width, int height, char const* title, View view, Update update) {
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

    static native_host host;
    host.window = window;
    app_detail::g_app_state = {&host, 0, 0xFFFFFFFF};

    glfwSetMouseButtonCallback(window, app_detail::on_mouse_button);
    glfwSetCursorPosCallback(window, app_detail::on_cursor_pos);
    glfwSetScrollCallback(window, app_detail::on_scroll);
    glfwSetFramebufferSizeCallback(window, app_detail::on_framebuffer_size);
    glfwSetKeyCallback(window, app_detail::on_key);

    run<State, Msg>(host, std::move(view), std::move(update));

    while (!glfwWindowShouldClose(window))
        glfwPollEvents();

    shutdown();
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}

} // namespace phenotype::native

#endif // !__wasi__
