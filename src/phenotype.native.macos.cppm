module;
#ifndef __wasi__
#include <cmath>
#include <cstdint>
#include <cstdio>
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
#endif
#endif

export module phenotype.native.macos;

#ifndef __wasi__
import phenotype.commands;
import phenotype.state;
import phenotype.native.platform;

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
    MTL::Device* device = nullptr;
    MTL::CommandQueue* queue = nullptr;
    CA::MetalLayer* layer = nullptr;
    MTL::RenderPipelineState* color_pipeline = nullptr;
    MTL::RenderPipelineState* text_pipeline = nullptr;
    MTL::Buffer* uniform_buf = nullptr;
    MTL::SamplerState* sampler = nullptr;
    std::vector<HitRegionCmd> hit_regions;
    GLFWwindow* window = nullptr;
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
    CGSize drawable_size{
        .width = static_cast<CGFloat>(fbw),
        .height = static_cast<CGFloat>(fbh),
    };
    g_renderer.layer->setDrawableSize(drawable_size);

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

    int winw = 0;
    int winh = 0;
    glfwGetWindowSize(window, &winw, &winh);
    std::printf("[phenotype-native] Metal initialized (%dx%d)\n", winw, winh);
}

inline void renderer_flush(unsigned char const* buf, unsigned int len) {
    if (len == 0 || !g_renderer.initialized) return;

    NS::AutoreleasePool* pool = NS::AutoreleasePool::alloc()->init();

    auto cmds = parse_commands(buf, len);
    float text_scale = current_backing_scale(g_renderer.window);
    float line_height_ratio = ::phenotype::detail::g_app.theme.line_height_ratio;

    double cr = 0.98;
    double cg = 0.98;
    double cb = 0.98;
    double ca = 1.0;
    std::vector<float> color_data;
    std::vector<TextEntry> text_entries;
    g_renderer.hit_regions.clear();

    for (auto const& cmd : cmds) {
        if (auto const* clear = std::get_if<ClearCmd>(&cmd)) {
            cr = clear->color.r / 255.0;
            cg = clear->color.g / 255.0;
            cb = clear->color.b / 255.0;
            ca = clear->color.a / 255.0;
        } else if (auto const* text = std::get_if<DrawTextCmd>(&cmd)) {
            text_entries.push_back({
                text->x,
                text->y,
                text->font_size,
                text->mono,
                text->color.r / 255.0f,
                text->color.g / 255.0f,
                text->color.b / 255.0f,
                text->color.a / 255.0f,
                text->text,
                text->font_size * line_height_ratio,
            });
        } else if (auto const* hr = std::get_if<HitRegionCmd>(&cmd)) {
            g_renderer.hit_regions.push_back(*hr);
        } else if (auto const* rect = std::get_if<FillRectCmd>(&cmd)) {
            color_data.insert(color_data.end(), {
                rect->x, rect->y, rect->w, rect->h,
                rect->color.r / 255.0f, rect->color.g / 255.0f,
                rect->color.b / 255.0f, rect->color.a / 255.0f,
                0, 0, 0, 0,
            });
        } else if (auto const* stroke = std::get_if<StrokeRectCmd>(&cmd)) {
            color_data.insert(color_data.end(), {
                stroke->x, stroke->y, stroke->w, stroke->h,
                stroke->color.r / 255.0f, stroke->color.g / 255.0f,
                stroke->color.b / 255.0f, stroke->color.a / 255.0f,
                0, stroke->line_width, 1, 0,
            });
        } else if (auto const* round = std::get_if<RoundRectCmd>(&cmd)) {
            color_data.insert(color_data.end(), {
                round->x, round->y, round->w, round->h,
                round->color.r / 255.0f, round->color.g / 255.0f,
                round->color.b / 255.0f, round->color.a / 255.0f,
                round->radius, 0, 2, 0,
            });
        } else if (auto const* line = std::get_if<DrawLineCmd>(&cmd)) {
            float dx = line->x2 - line->x1;
            float dy = line->y2 - line->y1;
            float line_len = std::sqrt(dx * dx + dy * dy);
            float w = (dy == 0) ? line_len : line->thickness;
            float h = (dx == 0) ? line_len : line->thickness;
            float x = (dx == 0)
                ? line->x1 - line->thickness / 2
                : (line->x1 < line->x2 ? line->x1 : line->x2);
            float y = (dy == 0)
                ? line->y1 - line->thickness / 2
                : (line->y1 < line->y2 ? line->y1 : line->y2);
            color_data.insert(color_data.end(), {
                x, y, w, h,
                line->color.r / 255.0f, line->color.g / 255.0f,
                line->color.b / 255.0f, line->color.a / 255.0f,
                0, 0, 3, 0,
            });
        }
    }

    int fbw = 0;
    int fbh = 0;
    glfwGetFramebufferSize(g_renderer.window, &fbw, &fbh);
    if (fbw == 0 || fbh == 0) {
        pool->release();
        return;
    }

    CGSize drawable_size{
        .width = static_cast<CGFloat>(fbw),
        .height = static_cast<CGFloat>(fbh),
    };
    g_renderer.layer->setDrawableSize(drawable_size);

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

    uint32_t color_count = static_cast<uint32_t>(color_data.size() / 12);
    if (color_count > 0) {
        auto* instance_buffer = g_renderer.device->newBuffer(
            color_data.data(),
            color_data.size() * sizeof(float),
            MTL::ResourceStorageModeShared);
        encoder->setRenderPipelineState(g_renderer.color_pipeline);
        encoder->setVertexBuffer(g_renderer.uniform_buf, 0, 0);
        encoder->setVertexBuffer(instance_buffer, 0, 1);
        encoder->drawPrimitives(MTL::PrimitiveTypeTriangle,
                                NS::UInteger(0), 6, NS::UInteger(color_count));
        instance_buffer->release();
    }

    if (!text_entries.empty()) {
        auto atlas = text_build_atlas(text_entries, text_scale);
        if (!atlas.quads.empty() && !atlas.pixels.empty()) {
            auto* tex_desc = MTL::TextureDescriptor::texture2DDescriptor(
                MTL::PixelFormatRGBA8Unorm,
                NS::UInteger(atlas.width), NS::UInteger(atlas.height), false);
            tex_desc->setUsage(MTL::TextureUsageShaderRead);
            auto* atlas_texture = g_renderer.device->newTexture(tex_desc);

            MTL::Region region = MTL::Region::Make2D(0, 0, atlas.width, atlas.height);
            atlas_texture->replaceRegion(region, 0, atlas.pixels.data(), atlas.width * 4);

            std::vector<float> text_data;
            text_data.reserve(atlas.quads.size() * 8);
            for (auto const& quad : atlas.quads) {
                text_data.insert(text_data.end(), {
                    quad.x, quad.y, quad.w, quad.h,
                    quad.u, quad.v, quad.uw, quad.vh,
                });
            }

            auto* text_buffer = g_renderer.device->newBuffer(
                text_data.data(),
                text_data.size() * sizeof(float),
                MTL::ResourceStorageModeShared);

            encoder->setRenderPipelineState(g_renderer.text_pipeline);
            encoder->setVertexBuffer(g_renderer.uniform_buf, 0, 0);
            encoder->setVertexBuffer(text_buffer, 0, 1);
            encoder->setFragmentTexture(atlas_texture, 0);
            encoder->setFragmentSamplerState(g_renderer.sampler, 0);
            encoder->drawPrimitives(
                MTL::PrimitiveTypeTriangle,
                NS::UInteger(0), 6, NS::UInteger(atlas.quads.size()));

            text_buffer->release();
            atlas_texture->release();
        }
    }

    encoder->endEncoding();
    command_buffer->presentDrawable(drawable);
    command_buffer->commit();

    pool->release();
}

inline void renderer_shutdown() {
    if (g_renderer.sampler) { g_renderer.sampler->release(); g_renderer.sampler = nullptr; }
    if (g_renderer.uniform_buf) { g_renderer.uniform_buf->release(); g_renderer.uniform_buf = nullptr; }
    if (g_renderer.text_pipeline) { g_renderer.text_pipeline->release(); g_renderer.text_pipeline = nullptr; }
    if (g_renderer.color_pipeline) { g_renderer.color_pipeline->release(); g_renderer.color_pipeline = nullptr; }
    if (g_renderer.layer) { g_renderer.layer->release(); g_renderer.layer = nullptr; }
    if (g_renderer.queue) { g_renderer.queue->release(); g_renderer.queue = nullptr; }
    if (g_renderer.device) { g_renderer.device->release(); g_renderer.device = nullptr; }
    g_renderer.hit_regions.clear();
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
