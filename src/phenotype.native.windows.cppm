module;
#if !defined(__wasi__) && !defined(__ANDROID__)
#include <cstdio>
#include <GLFW/glfw3.h>

#ifdef _WIN32
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <imm.h>
#include <wincodec.h>

#include <d3d12.h>
#include <d3d12sdklayers.h>
#include <d3dcompiler.h>
#include <dxgi1_6.h>
#include <dwrite.h>
#include <dwrite_1.h>
#include <wrl/client.h>

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "dwrite.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "imm32.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "windowscodecs.lib")

#ifdef DrawText
#undef DrawText
#endif
#endif
#endif

export module phenotype.native.windows;

#if !defined(__wasi__) && !defined(__ANDROID__)
import std;
import cppx.http;
import cppx.http.system;
import cppx.os;
import cppx.os.system;
import cppx.resource;
import cppx.unicode;
import json;
import phenotype;
import phenotype.commands;
import phenotype.state;
import phenotype.types;
import phenotype.native.platform;
import phenotype.native.shell;
import phenotype.native.shell.glfw;
import phenotype.native.stub;

namespace phenotype::native::detail {

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

inline bool env_enabled(char const* name) {
#ifdef _WIN32
    char buf[8]{};
    DWORD len = GetEnvironmentVariableA(name, buf, static_cast<DWORD>(std::size(buf)));
    if (len == 0 || len >= std::size(buf)) return false;
    return buf[0] == '1' || buf[0] == 'y' || buf[0] == 'Y'
        || buf[0] == 't' || buf[0] == 'T';
#else
    (void)name;
    return false;
#endif
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

#ifdef _WIN32

using Microsoft::WRL::ComPtr;

inline void log_hresult(char const* label, HRESULT hr) {
    std::fprintf(stderr, "[windows] %s failed (hr=0x%08lx)\n",
                 label, static_cast<unsigned long>(hr));
}

inline void set_debug_name(ID3D12Object* object, wchar_t const* name) {
    if (!object || !name || !*name)
        return;
    (void)object->SetName(name);
}

template <typename T>
inline void set_debug_name(ComPtr<T> const& object, wchar_t const* name) {
    set_debug_name(object.Get(), name);
}

inline UINT64 align_up(UINT64 value, UINT64 alignment) {
    return (value + alignment - 1) / alignment * alignment;
}

inline DXGI_ADAPTER_DESC1 describe_adapter(IDXGIAdapter1* adapter) {
    DXGI_ADAPTER_DESC1 desc{};
    if (adapter) adapter->GetDesc1(&desc);
    return desc;
}

struct TextState {
    ComPtr<IDWriteFactory> factory;
    ComPtr<IDWriteGdiInterop> gdi_interop;
    ComPtr<IDWriteRenderingParams> rendering_params;
    ComPtr<IDWriteTextFormat> sans_format;
    ComPtr<IDWriteTextFormat> mono_format;
    bool initialized = false;
};

static TextState g_text;

inline HRESULT create_text_format(
        wchar_t const* family_name,
        ComPtr<IDWriteTextFormat>& format) {
    auto hr = g_text.factory->CreateTextFormat(
        family_name,
        nullptr,
        DWRITE_FONT_WEIGHT_NORMAL,
        DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL,
        16.0f,
        L"en-us",
        &format);
    if (FAILED(hr)) return hr;
    format->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
    format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
    format->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_NEAR);
    return S_OK;
}

inline void text_init() {
    if (g_text.initialized) return;

    auto hr = DWriteCreateFactory(
        DWRITE_FACTORY_TYPE_SHARED,
        __uuidof(IDWriteFactory),
        reinterpret_cast<IUnknown**>(g_text.factory.ReleaseAndGetAddressOf()));
    if (FAILED(hr)) {
        log_hresult("DWriteCreateFactory", hr);
        return;
    }

    hr = g_text.factory->GetGdiInterop(&g_text.gdi_interop);
    if (FAILED(hr)) {
        log_hresult("IDWriteFactory::GetGdiInterop", hr);
        g_text = {};
        return;
    }

    hr = g_text.factory->CreateRenderingParams(&g_text.rendering_params);
    if (FAILED(hr)) {
        log_hresult("IDWriteFactory::CreateRenderingParams", hr);
        g_text = {};
        return;
    }

    hr = create_text_format(L"Segoe UI", g_text.sans_format);
    if (FAILED(hr)) {
        log_hresult("CreateTextFormat(Segoe UI)", hr);
        g_text = {};
        return;
    }

    hr = create_text_format(L"Consolas", g_text.mono_format);
    if (FAILED(hr)) {
        log_hresult("CreateTextFormat(Consolas)", hr);
        g_text = {};
        return;
    }

    g_text.initialized = true;
}

inline void text_shutdown() {
    g_text = {};
}

inline HRESULT make_text_layout(
        std::wstring const& text,
        float font_size,
        bool mono,
        float max_width,
        float max_height,
        ComPtr<IDWriteTextLayout>& layout) {
    if (!g_text.initialized) return E_FAIL;
    auto const& format = mono ? g_text.mono_format : g_text.sans_format;
    auto hr = g_text.factory->CreateTextLayout(
        text.c_str(),
        static_cast<UINT32>(text.size()),
        format.Get(),
        max_width,
        max_height,
        &layout);
    if (FAILED(hr)) return hr;
    DWRITE_TEXT_RANGE range{0, static_cast<UINT32>(text.size())};
    layout->SetFontSize(font_size, range);
    layout->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
    return S_OK;
}

inline float text_measure(float font_size, bool mono,
                          char const* text_ptr, unsigned int len) {
    if (!g_text.initialized || len == 0) return 0.0f;

    auto wide = cppx::unicode::utf8_to_wide(
        std::string_view{text_ptr, len}).value_or(std::wstring{});
    if (wide.empty()) return 0.0f;

    ComPtr<IDWriteTextLayout> layout;
    auto hr = make_text_layout(
        wide, font_size, mono,
        16384.0f, font_size * 4.0f + 32.0f,
        layout);
    if (FAILED(hr)) {
        log_hresult("CreateTextLayout(measure)", hr);
        return 0.0f;
    }

    DWRITE_TEXT_METRICS metrics{};
    hr = layout->GetMetrics(&metrics);
    if (FAILED(hr)) {
        log_hresult("IDWriteTextLayout::GetMetrics", hr);
        return 0.0f;
    }
    return metrics.widthIncludingTrailingWhitespace;
}

class GlyphBitmapRenderer final : public IDWriteTextRenderer {
public:
    GlyphBitmapRenderer(IDWriteBitmapRenderTarget* target,
                        IDWriteRenderingParams* rendering_params,
                        COLORREF text_color,
                        float pixels_per_dip)
        : refs_(1),
          target_(target),
          rendering_params_(rendering_params),
          text_color_(text_color),
          pixels_per_dip_(sanitize_scale(pixels_per_dip)) {
        if (target_) target_->AddRef();
        if (rendering_params_) rendering_params_->AddRef();
    }

    ~GlyphBitmapRenderer() {
        if (rendering_params_) rendering_params_->Release();
        if (target_) target_->Release();
    }

    HRESULT STDMETHODCALLTYPE QueryInterface(
            REFIID riid, void** object) override {
        if (!object) return E_POINTER;
        *object = nullptr;
        if (riid == __uuidof(IUnknown)
            || riid == __uuidof(IDWritePixelSnapping)
            || riid == __uuidof(IDWriteTextRenderer)) {
            *object = static_cast<IDWriteTextRenderer*>(this);
            AddRef();
            return S_OK;
        }
        return E_NOINTERFACE;
    }

    ULONG STDMETHODCALLTYPE AddRef() override {
        return static_cast<ULONG>(++refs_);
    }

    ULONG STDMETHODCALLTYPE Release() override {
        auto count = static_cast<ULONG>(--refs_);
        if (count == 0) delete this;
        return count;
    }

    HRESULT STDMETHODCALLTYPE IsPixelSnappingDisabled(
            void*, BOOL* is_disabled) override {
        if (!is_disabled) return E_POINTER;
        *is_disabled = FALSE;
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE GetCurrentTransform(
            void*, DWRITE_MATRIX* transform) override {
        if (!transform) return E_POINTER;
        *transform = DWRITE_MATRIX{1, 0, 0, 1, 0, 0};
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE GetPixelsPerDip(
            void*, FLOAT* pixels_per_dip) override {
        if (!pixels_per_dip) return E_POINTER;
        *pixels_per_dip = pixels_per_dip_;
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE DrawGlyphRun(
            void*,
            FLOAT baseline_origin_x,
            FLOAT baseline_origin_y,
            DWRITE_MEASURING_MODE measuring_mode,
            DWRITE_GLYPH_RUN const* glyph_run,
            DWRITE_GLYPH_RUN_DESCRIPTION const*,
            IUnknown*) override {
        if (!target_ || !glyph_run) return E_POINTER;
        return target_->DrawGlyphRun(
            baseline_origin_x,
            baseline_origin_y,
            measuring_mode,
            glyph_run,
            rendering_params_,
            text_color_,
            nullptr);
    }

    HRESULT STDMETHODCALLTYPE DrawUnderline(
            void*,
            FLOAT,
            FLOAT,
            DWRITE_UNDERLINE const*,
            IUnknown*) override {
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE DrawStrikethrough(
            void*,
            FLOAT,
            FLOAT,
            DWRITE_STRIKETHROUGH const*,
            IUnknown*) override {
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE DrawInlineObject(
            void*,
            FLOAT,
            FLOAT,
            IDWriteInlineObject*,
            BOOL,
            BOOL,
            IUnknown*) override {
        return S_OK;
    }

private:
    std::atomic_ulong refs_;
    IDWriteBitmapRenderTarget* target_ = nullptr;
    IDWriteRenderingParams* rendering_params_ = nullptr;
    COLORREF text_color_ = RGB(255, 255, 255);
    float pixels_per_dip_ = 1.0f;
};

struct BitmapSectionView {
    DIBSECTION dib{};
    std::uint32_t* pixels = nullptr;
    int stride_pixels = 0;
};

inline std::optional<BitmapSectionView> bitmap_section_view(HDC dc) {
    if (!dc) return std::nullopt;
    HBITMAP bitmap = static_cast<HBITMAP>(GetCurrentObject(dc, OBJ_BITMAP));
    if (!bitmap) return std::nullopt;

    BitmapSectionView view{};
    if (GetObjectW(bitmap, sizeof(view.dib), &view.dib) != sizeof(view.dib))
        return std::nullopt;
    if (!view.dib.dsBm.bmBits || view.dib.dsBm.bmWidthBytes <= 0)
        return std::nullopt;

    view.pixels = static_cast<std::uint32_t*>(view.dib.dsBm.bmBits);
    view.stride_pixels = view.dib.dsBm.bmWidthBytes / static_cast<int>(sizeof(std::uint32_t));
    if (!view.pixels || view.stride_pixels <= 0)
        return std::nullopt;
    return view;
}

inline void clear_bitmap_section(BitmapSectionView const& view, int height) {
    auto bytes = static_cast<std::size_t>(view.dib.dsBm.bmWidthBytes)
        * static_cast<std::size_t>(height);
    std::memset(view.pixels, 0, bytes);
}

inline std::uint32_t bitmap_section_pixel(
        BitmapSectionView const& view,
        int x, int y) {
    // IDWrite bitmap render targets expose the DIB memory in draw order here,
    // so treat scanlines as top-down when copying glyph coverage into the atlas.
    int row = y;
    return view.pixels[
        static_cast<std::size_t>(row) * static_cast<std::size_t>(view.stride_pixels) + x];
}

inline TextAtlas text_build_atlas(std::vector<TextEntry> const& entries,
                                  float backing_scale) {
    if (!g_text.initialized || entries.empty()) return {};

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

        auto wide = cppx::unicode::utf8_to_wide(entry.text).value_or(std::wstring{});
        if (wide.empty()) continue;

        ComPtr<IDWriteTextLayout> metrics_layout;
        auto hr = make_text_layout(
            wide,
            entry.font_size,
            entry.mono,
            16384.0f,
            entry.font_size * 4.0f + 64.0f,
            metrics_layout);
        if (FAILED(hr)) continue;

        DWRITE_TEXT_METRICS metrics{};
        if (FAILED(metrics_layout->GetMetrics(&metrics))) continue;

        std::array<DWRITE_LINE_METRICS, 4> line_metrics{};
        UINT32 actual_lines = 0;
        hr = metrics_layout->GetLineMetrics(
            line_metrics.data(),
            static_cast<UINT32>(line_metrics.size()),
            &actual_lines);
        if (FAILED(hr) || actual_lines == 0) continue;

        float logical_width = metrics.widthIncludingTrailingWhitespace;
        float ascent = line_metrics[0].baseline * scale;
        float descent = (line_metrics[0].height - line_metrics[0].baseline) * scale;
        if (descent < 0.0f) descent = 0.0f;
        float logical_line_height = entry.line_height > 0
            ? entry.line_height
            : entry.font_size * 1.6f;
        float snapped_x = snap_to_pixel_grid(entry.x, scale);
        float snapped_y = snap_to_pixel_grid(entry.y, scale);
        auto box = make_line_box(
            logical_width,
            logical_line_height,
            ascent,
            descent,
            0.0f,
            scale,
            padding);

        if (ax + box.slot_width > ATLAS_SIZE) {
            ax = 0;
            ay += row_height;
            row_height = 0;
        }
        if (ay + box.slot_height > ATLAS_SIZE) break;

        ComPtr<IDWriteTextLayout> draw_layout;
        hr = make_text_layout(
            wide,
            entry.font_size,
            entry.mono,
            static_cast<float>(box.slot_width - padding * 2) / scale,
            static_cast<float>(box.render_height) / scale,
            draw_layout);
        if (FAILED(hr)) continue;

        ComPtr<IDWriteBitmapRenderTarget> target;
        hr = g_text.gdi_interop->CreateBitmapRenderTarget(
            nullptr,
            box.slot_width,
            box.slot_height,
            &target);
        if (FAILED(hr) || !target) continue;
        if (FAILED(target->SetPixelsPerDip(scale))) continue;

        ComPtr<IDWriteBitmapRenderTarget1> target1;
        hr = target.As(&target1);
        if (FAILED(hr) || !target1) continue;
        // This atlas is composited later with alpha, so ClearType subpixels cannot
        // survive the intermediate bitmap. DirectWrite recommends grayscale here.
        hr = target1->SetTextAntialiasMode(DWRITE_TEXT_ANTIALIAS_MODE_GRAYSCALE);
        if (FAILED(hr)) continue;

        HDC dc = target->GetMemoryDC();
        if (!dc) continue;

        auto bitmap_view = bitmap_section_view(dc);
        if (!bitmap_view) continue;
        clear_bitmap_section(*bitmap_view, box.slot_height);
        SetBkMode(dc, TRANSPARENT);

        auto* renderer = new GlyphBitmapRenderer(
            target.Get(),
            g_text.rendering_params.Get(),
            RGB(255, 255, 255),
            scale);
        hr = draw_layout->Draw(
            nullptr,
            renderer,
            static_cast<float>(padding) / scale,
            static_cast<float>(box.render_top) / scale);
        renderer->Release();
        if (FAILED(hr)) continue;

        bool has_ink = false;
        int ink_min_x = box.slot_width;
        int ink_max_x = -1;
        for (int py = box.render_top; py < box.render_top + box.render_height; ++py) {
            if (py < 0 || py >= box.slot_height) continue;
            for (int px = 0; px < box.slot_width; ++px) {
                auto packed = bitmap_section_pixel(*bitmap_view, px, py);
                unsigned char alpha = static_cast<unsigned char>((packed >> 24) & 0xFF);
                if (alpha == 0) continue;
                has_ink = true;
                if (px < ink_min_x) ink_min_x = px;
                if (px > ink_max_x) ink_max_x = px;

                auto idx = static_cast<std::size_t>(((ay + py) * ATLAS_SIZE + (ax + px)) * 4);
                atlas.pixels[idx + 0] = static_cast<unsigned char>(entry.r * 255.0f * alpha / 255.0f);
                atlas.pixels[idx + 1] = static_cast<unsigned char>(entry.g * 255.0f * alpha / 255.0f);
                atlas.pixels[idx + 2] = static_cast<unsigned char>(entry.b * 255.0f * alpha / 255.0f);
                atlas.pixels[idx + 3] = alpha;
            }
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

inline constexpr char HLSL_SHADERS[] = R"(
cbuffer Uniforms : register(b0) {
    float2 viewport;
    float2 _pad;
};

struct ColorInstance {
    float4 rect : RECT;
    float4 color : COLOR;
    float4 params : PARAMS;
};

struct ColorVsOut {
    float4 pos : SV_POSITION;
    float4 color : COLOR0;
    float2 local_pos : TEXCOORD0;
    float2 rect_size : TEXCOORD1;
    float4 params : TEXCOORD2;
};

ColorVsOut vs_color(uint vertex_id : SV_VertexID, ColorInstance inst) {
    static const float2 corners[6] = {
        float2(0,0), float2(1,0), float2(0,1),
        float2(1,0), float2(1,1), float2(0,1)
    };
    float2 c = corners[vertex_id];
    float px = inst.rect.x + c.x * inst.rect.z;
    float py = inst.rect.y + c.y * inst.rect.w;
    float cx = (px / viewport.x) * 2.0f - 1.0f;
    float cy = 1.0f - (py / viewport.y) * 2.0f;
    ColorVsOut o;
    o.pos = float4(cx, cy, 0, 1);
    o.color = inst.color;
    o.local_pos = c * inst.rect.zw;
    o.rect_size = inst.rect.zw;
    o.params = inst.params;
    return o;
}

float4 fs_color(ColorVsOut input) : SV_TARGET {
    uint draw_type = (uint)input.params.z;
    float radius = input.params.x;
    float border_w = input.params.y;
    if (draw_type == 2u && radius > 0.0f) {
        float2 half_size = input.rect_size * 0.5f;
        float2 p = abs(input.local_pos - half_size) - half_size + float2(radius, radius);
        float d = length(max(p, float2(0.0f, 0.0f))) - radius;
        clip(0.5f - d);
        float alpha = input.color.a * saturate(0.5f - d);
        return float4(input.color.rgb * alpha, alpha);
    }
    if (draw_type == 1u) {
        float2 lp = input.local_pos;
        float2 sz = input.rect_size;
        if (lp.x > border_w && lp.x < sz.x - border_w &&
            lp.y > border_w && lp.y < sz.y - border_w) {
            discard;
        }
        return input.color;
    }
    return input.color;
}

Texture2D atlas_tex : register(t0);
SamplerState image_samp : register(s0);
SamplerState text_samp : register(s1);

struct TextInstance {
    float4 rect : RECT;
    float4 uv_rect : UVRECT;
};

struct TextVsOut {
    float4 pos : SV_POSITION;
    float2 uv : TEXCOORD0;
};

TextVsOut vs_text(uint vertex_id : SV_VertexID, TextInstance inst) {
    static const float2 corners[6] = {
        float2(0,0), float2(1,0), float2(0,1),
        float2(1,0), float2(1,1), float2(0,1)
    };
    float2 c = corners[vertex_id];
    float px = inst.rect.x + c.x * inst.rect.z;
    float py = inst.rect.y + c.y * inst.rect.w;
    float cx = (px / viewport.x) * 2.0f - 1.0f;
    float cy = 1.0f - (py / viewport.y) * 2.0f;
    TextVsOut o;
    o.pos = float4(cx, cy, 0, 1);
    o.uv = inst.uv_rect.xy + c * inst.uv_rect.zw;
    return o;
}

float4 fs_image(TextVsOut input) : SV_TARGET {
    float4 s = atlas_tex.Sample(image_samp, input.uv);
    clip(s.a - 0.01f);
    return s;
}

float4 fs_text(TextVsOut input) : SV_TARGET {
    float4 s = atlas_tex.Sample(text_samp, input.uv);
    clip(s.a - 0.01f);
    return s;
}
)";

struct FrameContext {
    ComPtr<ID3D12CommandAllocator> allocator;
    ComPtr<ID3D12Resource> render_target;
    ComPtr<ID3D12Resource> atlas_texture;
    D3D12_CPU_DESCRIPTOR_HANDLE rtv_handle{};
    UINT text_srv_slot = 0;
    UINT64 fence_value = 0;
};

struct UploadBuffer {
    ComPtr<ID3D12Resource> resource;
    unsigned char* mapped = nullptr;
    std::size_t capacity = 0;
    std::size_t offset = 0;
};

struct ReadbackBuffer {
    ComPtr<ID3D12Resource> resource;
    UINT64 capacity = 0;
};

struct RendererState {
    static constexpr UINT frame_count = 2;

    ComPtr<IDXGIFactory6> factory;
    ComPtr<IDXGIAdapter1> adapter;
    ComPtr<ID3D12Device> device;
    ComPtr<ID3D12CommandQueue> queue;
    ComPtr<IDXGISwapChain3> swap_chain;
    ComPtr<ID3D12DescriptorHeap> rtv_heap;
    ComPtr<ID3D12DescriptorHeap> srv_heap;
    ComPtr<ID3D12RootSignature> root_signature;
    ComPtr<ID3D12PipelineState> color_pipeline;
    ComPtr<ID3D12PipelineState> image_pipeline;
    ComPtr<ID3D12PipelineState> text_pipeline;
    ComPtr<ID3D12GraphicsCommandList> command_list;
    ComPtr<ID3D12Fence> fence;
    HANDLE fence_event = nullptr;
    UINT rtv_descriptor_size = 0;
    UINT srv_descriptor_size = 0;
    FrameContext frames[frame_count];
    UploadBuffer upload{};
    ReadbackBuffer readback{};
    std::vector<HitRegionCmd> hit_regions;
    GLFWwindow* window = nullptr;
    bool initialized = false;
    bool debug_preset_enabled = false;
    bool debug_enabled = false;
    bool gpu_validation_enabled = false;
    bool dred_enabled = false;
    bool warp_enabled = false;
    bool com_initialized = false;
    bool lost = false;
    bool last_frame_available = false;
    UINT last_presented_frame_index = 0;
    UINT last_render_width = 0;
    UINT last_render_height = 0;
    UINT64 next_fence_value = 1;
    HRESULT last_failure_hr = S_OK;
    HRESULT device_removed_reason = S_OK;
    HRESULT last_close_hr = S_OK;
    HRESULT last_present_hr = S_OK;
    HRESULT last_signal_hr = S_OK;
    D3D12_DRED_DEVICE_STATE dred_device_state = D3D12_DRED_DEVICE_STATE_UNKNOWN;
    std::string last_failure_label;
};

static RendererState g_renderer;
constexpr UINT WM_PHENOTYPE_IMAGE_READY = WM_APP + 61;
constexpr UINT IMAGE_SRV_SLOT = 0;
constexpr UINT FIRST_TEXT_SRV_SLOT = 1;
constexpr UINT SRV_SLOT_COUNT = FIRST_TEXT_SRV_SLOT + RendererState::frame_count;

inline bool running_under_tests() {
    char path[MAX_PATH]{};
    auto len = GetModuleFileNameA(nullptr, path, static_cast<DWORD>(std::size(path)));
    if (len == 0 || len >= std::size(path))
        return false;
    auto exe = std::string_view(path, len);
    auto slash = exe.find_last_of("\\/");
    if (slash != std::string_view::npos)
        exe.remove_prefix(slash + 1);
    return exe.find("test-") != std::string_view::npos
        || exe.find("test_") != std::string_view::npos;
}

inline bool should_enable_dred() {
    return g_renderer.debug_enabled || g_renderer.warp_enabled || running_under_tests();
}

inline UINT frame_text_srv_slot(UINT frame_index) {
    return FIRST_TEXT_SRV_SLOT + frame_index;
}

inline char const* breadcrumb_op_name(D3D12_AUTO_BREADCRUMB_OP op) {
    switch (op) {
    case D3D12_AUTO_BREADCRUMB_OP_DRAWINSTANCED:
        return "DrawInstanced";
    case D3D12_AUTO_BREADCRUMB_OP_DRAWINDEXEDINSTANCED:
        return "DrawIndexedInstanced";
    case D3D12_AUTO_BREADCRUMB_OP_COPYTEXTUREREGION:
        return "CopyTextureRegion";
    case D3D12_AUTO_BREADCRUMB_OP_CLEARRENDERTARGETVIEW:
        return "ClearRenderTargetView";
    case D3D12_AUTO_BREADCRUMB_OP_RESOURCEBARRIER:
        return "ResourceBarrier";
    case D3D12_AUTO_BREADCRUMB_OP_PRESENT:
        return "Present";
    case D3D12_AUTO_BREADCRUMB_OP_BEGINSUBMISSION:
        return "BeginSubmission";
    case D3D12_AUTO_BREADCRUMB_OP_ENDSUBMISSION:
        return "EndSubmission";
    case D3D12_AUTO_BREADCRUMB_OP_SETPIPELINESTATE1:
        return "SetPipelineState1";
    case D3D12_AUTO_BREADCRUMB_OP_BEGIN_COMMAND_LIST:
        return "BeginCommandList";
    default:
        return "Other";
    }
}

inline char const* dred_allocation_type_name(D3D12_DRED_ALLOCATION_TYPE type) {
    switch (type) {
    case D3D12_DRED_ALLOCATION_TYPE_COMMAND_QUEUE:
        return "command_queue";
    case D3D12_DRED_ALLOCATION_TYPE_COMMAND_ALLOCATOR:
        return "command_allocator";
    case D3D12_DRED_ALLOCATION_TYPE_PIPELINE_STATE:
        return "pipeline_state";
    case D3D12_DRED_ALLOCATION_TYPE_COMMAND_LIST:
        return "command_list";
    case D3D12_DRED_ALLOCATION_TYPE_FENCE:
        return "fence";
    case D3D12_DRED_ALLOCATION_TYPE_DESCRIPTOR_HEAP:
        return "descriptor_heap";
    case D3D12_DRED_ALLOCATION_TYPE_HEAP:
        return "heap";
    case D3D12_DRED_ALLOCATION_TYPE_RESOURCE:
        return "resource";
    default:
        return "other";
    }
}

inline void dump_dred_allocation_list(
        char const* label,
        D3D12_DRED_ALLOCATION_NODE1 const* node) {
    for (int i = 0; node && i < 6; ++i, node = node->pNext) {
        if (node->ObjectNameW && *node->ObjectNameW) {
            std::fprintf(
                stderr,
                "[dx12/dred] %s[%d] type=%s name=%ls\n",
                label,
                i,
                dred_allocation_type_name(node->AllocationType),
                node->ObjectNameW);
        } else if (node->ObjectNameA && *node->ObjectNameA) {
            std::fprintf(
                stderr,
                "[dx12/dred] %s[%d] type=%s name=%s\n",
                label,
                i,
                dred_allocation_type_name(node->AllocationType),
                node->ObjectNameA);
        } else {
            std::fprintf(
                stderr,
                "[dx12/dred] %s[%d] type=%s name=<unnamed>\n",
                label,
                i,
                dred_allocation_type_name(node->AllocationType));
        }
    }
}

inline void dump_dred_diagnostics() {
    if (!g_renderer.device)
        return;

    ComPtr<ID3D12DeviceRemovedExtendedData2> dred2;
    if (SUCCEEDED(g_renderer.device.As(&dred2))) {
        g_renderer.dred_device_state = dred2->GetDeviceState();
    } else {
        g_renderer.dred_device_state = D3D12_DRED_DEVICE_STATE_UNKNOWN;
    }

    ComPtr<ID3D12DeviceRemovedExtendedData1> dred1;
    if (FAILED(g_renderer.device.As(&dred1)))
        return;

    D3D12_DRED_AUTO_BREADCRUMBS_OUTPUT1 breadcrumbs{};
    auto hr = dred1->GetAutoBreadcrumbsOutput1(&breadcrumbs);
    if (SUCCEEDED(hr) && breadcrumbs.pHeadAutoBreadcrumbNode) {
        auto const* node = breadcrumbs.pHeadAutoBreadcrumbNode;
        for (int i = 0; node && i < 8; ++i, node = node->pNext) {
            auto completed = node->pLastBreadcrumbValue ? *node->pLastBreadcrumbValue : 0u;
            auto history_len = node->BreadcrumbCount;
            auto op = D3D12_AUTO_BREADCRUMB_OP_SETMARKER;
            if (node->pCommandHistory && completed > 0 && history_len > 0) {
                auto index = (completed - 1u) % history_len;
                op = node->pCommandHistory[index];
            }
            auto const* list_name =
                (node->pCommandListDebugNameW && *node->pCommandListDebugNameW)
                ? node->pCommandListDebugNameW
                : L"<unnamed>";
            auto const* queue_name =
                (node->pCommandQueueDebugNameW && *node->pCommandQueueDebugNameW)
                ? node->pCommandQueueDebugNameW
                : L"<unnamed>";
            std::fprintf(
                stderr,
                "[dx12/dred] breadcrumb[%d] queue=%ls list=%ls completed=%u/%u last_op=%s\n",
                i,
                queue_name,
                list_name,
                completed,
                history_len,
                breadcrumb_op_name(op));
        }
    }

    D3D12_DRED_PAGE_FAULT_OUTPUT1 page_fault{};
    hr = dred1->GetPageFaultAllocationOutput1(&page_fault);
    if (SUCCEEDED(hr)) {
        if (page_fault.PageFaultVA != 0) {
            std::fprintf(
                stderr,
                "[dx12/dred] page_fault_va=0x%llx state=%u\n",
                static_cast<unsigned long long>(page_fault.PageFaultVA),
                static_cast<unsigned int>(g_renderer.dred_device_state));
        }
        dump_dred_allocation_list("existing", page_fault.pHeadExistingAllocationNode);
        dump_dred_allocation_list("recent_freed", page_fault.pHeadRecentFreedAllocationNode);
    }
}

inline HRESULT mark_renderer_lost(char const* label, HRESULT hr) {
    if (!FAILED(hr))
        return hr;

    log_hresult(label, hr);
    g_renderer.lost = true;
    g_renderer.last_failure_label = label ? label : "";
    g_renderer.last_failure_hr = hr;
    g_renderer.device_removed_reason =
        g_renderer.device ? g_renderer.device->GetDeviceRemovedReason() : S_OK;

    if (FAILED(g_renderer.device_removed_reason)) {
        std::fprintf(
            stderr,
            "[dx12] device removed after %s (reason=0x%08lx)\n",
            label ? label : "<unknown>",
            static_cast<unsigned long>(g_renderer.device_removed_reason));
        dump_dred_diagnostics();
    }
    return hr;
}

enum class CandidateHitKind {
    none,
    item,
    prev_page,
    next_page,
};

struct CandidateHit {
    CandidateHitKind kind = CandidateHitKind::none;
    unsigned int index = 0;
    float x = 0.0f;
    float y = 0.0f;
    float w = 0.0f;
    float h = 0.0f;

    bool contains(float px, float py) const noexcept {
        return px >= x && px <= x + w && py >= y && py <= y + h;
    }
};

struct CandidateOverlayLayout {
    bool visible = false;
    float x = 0.0f;
    float y = 0.0f;
    float w = 0.0f;
    float h = 0.0f;
    float row_height = 0.0f;
    std::vector<CandidateHit> hits;
};

struct FlagGuard {
    bool& flag;

    explicit FlagGuard(bool& target) : flag(target) {
        flag = true;
    }

    ~FlagGuard() {
        flag = false;
    }

    FlagGuard(FlagGuard const&) = delete;
    FlagGuard& operator=(FlagGuard const&) = delete;
};

struct WindowPointCache {
    bool valid = false;
    LONG x = 0;
    LONG y = 0;
};

struct ImeState {
    GLFWwindow* window = nullptr;
    HWND hwnd = nullptr;
    WNDPROC prev_wndproc = nullptr;
    void (*request_repaint)() = nullptr;
    DWORD ui_thread_id = 0;
    bool attached = false;
    bool in_wndproc = false;
    bool sync_in_progress = false;
    bool repaint_pending = false;
    bool repaint_dispatch_in_progress = false;
    bool composition_active = false;
    std::wstring composition_text;
    LONG composition_cursor = 0;
    std::size_t composition_anchor = 0;
    std::size_t replacement_start = 0;
    std::size_t replacement_end = 0;
    bool candidate_open = false;
    std::vector<std::wstring> candidates;
    DWORD candidate_selection = 0;
    DWORD candidate_page_start = 0;
    DWORD candidate_page_size = 0;
    int hovered_candidate = -1;
    CandidateHitKind hovered_kind = CandidateHitKind::none;
    CandidateOverlayLayout overlay{};
    WindowPointCache composition_window{};
    WindowPointCache candidate_window{};
    std::size_t sync_call_count = 0;
    std::size_t repaint_request_count = 0;
    std::size_t deferred_repaint_count = 0;
};

struct DecodedImage {
    std::string url;
    int width = 0;
    int height = 0;
    std::vector<unsigned char> pixels;
    bool failed = false;
    std::string error_detail;
};

enum class ImageEntryState {
    pending,
    ready,
    failed,
};

inline char const* image_entry_state_name(ImageEntryState state) {
    switch (state) {
    case ImageEntryState::pending:
        return "pending";
    case ImageEntryState::ready:
        return "ready";
    case ImageEntryState::failed:
        return "failed";
    default:
        return "unknown";
    }
}

struct ImageCacheEntry {
    ImageEntryState state = ImageEntryState::pending;
    float u = 0.0f;
    float v = 0.0f;
    float uw = 0.0f;
    float vh = 0.0f;
    std::string failure_reason;
};

struct CachedImageRecord {
    std::string url;
    ImageCacheEntry entry;
};

struct ImageAtlasState {
    static constexpr int atlas_size = 2048;

    ComPtr<ID3D12Resource> texture;
    std::vector<unsigned char> pixels;
    std::vector<CachedImageRecord> cache;
    std::vector<std::string> pending_jobs;
    std::size_t pending_head = 0;
    std::vector<DecodedImage> completed_jobs;
    std::mutex mutex;
    std::condition_variable cv;
    std::thread worker;
    std::atomic<DWORD> worker_thread_id = 0;
    bool worker_started = false;
    bool queue_only_for_tests = false;
    bool stop_worker = false;
    bool atlas_dirty = false;
    int cursor_x = 0;
    int cursor_y = 0;
    int row_height = 0;
};

static ImeState g_ime;
static ImageAtlasState g_images;

struct AddressDescription {
    std::uintptr_t relative = 0;
    char module_path[MAX_PATH] = {};
};

inline AddressDescription describe_address(void const* address) {
    AddressDescription info{};
    if (!address)
        return info;

    HMODULE module = nullptr;
    if (GetModuleHandleExA(
            GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS
                | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            static_cast<LPCSTR>(address),
            &module)
        && module) {
        info.relative = reinterpret_cast<std::uintptr_t>(address)
            - reinterpret_cast<std::uintptr_t>(module);
        (void)GetModuleFileNameA(module, info.module_path, MAX_PATH);
    }
    return info;
}

inline void dump_context_stack(CONTEXT const* context) {
#if defined(_M_X64)
    if (!context)
        return;

    auto unwind = *context;
    constexpr unsigned int max_frames = 24;
    for (unsigned int frame = 0; frame < max_frames; ++frame) {
        if (unwind.Rip == 0)
            break;

        auto const address = reinterpret_cast<void const*>(unwind.Rip);
        auto info = describe_address(address);
        std::fprintf(
            stderr,
            "[phenotype-windows] stack[%u]=%p module=%s relative=0x%zx sp=0x%llx\n",
            frame,
            address,
            info.module_path[0] ? info.module_path : "<unknown>",
            static_cast<std::size_t>(info.relative),
            static_cast<unsigned long long>(unwind.Rsp));

        DWORD64 image_base = 0;
        auto* runtime_function = RtlLookupFunctionEntry(unwind.Rip, &image_base, nullptr);
        if (!runtime_function) {
            DWORD64 next_pc = 0;
            __try {
                next_pc = *reinterpret_cast<DWORD64 const*>(unwind.Rsp);
            } __except (EXCEPTION_EXECUTE_HANDLER) {
                break;
            }
            if (next_pc == 0 || next_pc == unwind.Rip)
                break;
            unwind.Rip = next_pc;
            unwind.Rsp += sizeof(DWORD64);
            continue;
        }

        void* handler_data = nullptr;
        DWORD64 establisher_frame = 0;
        KNONVOLATILE_CONTEXT_POINTERS context_pointers{};
        RtlVirtualUnwind(
            UNW_FLAG_NHANDLER,
            image_base,
            unwind.Rip,
            runtime_function,
            &unwind,
            &handler_data,
            &establisher_frame,
            &context_pointers);
    }
#else
    (void)context;
#endif
}

inline void dump_runtime_diagnostics(char const* reason) {
    std::fprintf(
        stderr,
        "[phenotype-windows] %s: renderer_lost=%d last_failure_hr=0x%08lx "
        "device_removed=0x%08lx close_hr=0x%08lx present_hr=0x%08lx signal_hr=0x%08lx "
        "dred_enabled=%d initialized=%d debug=%d warp=%d current_tid=%lu ui_tid=%lu worker_tid=%lu\n",
        reason ? reason : "runtime diagnostics",
        g_renderer.lost ? 1 : 0,
        static_cast<unsigned long>(g_renderer.last_failure_hr),
        static_cast<unsigned long>(g_renderer.device_removed_reason),
        static_cast<unsigned long>(g_renderer.last_close_hr),
        static_cast<unsigned long>(g_renderer.last_present_hr),
        static_cast<unsigned long>(g_renderer.last_signal_hr),
        g_renderer.dred_enabled ? 1 : 0,
        g_renderer.initialized ? 1 : 0,
        g_renderer.debug_enabled ? 1 : 0,
        g_renderer.warp_enabled ? 1 : 0,
        static_cast<unsigned long>(GetCurrentThreadId()),
        static_cast<unsigned long>(g_ime.ui_thread_id),
        static_cast<unsigned long>(g_images.worker_thread_id.load()));
    if (!g_renderer.last_failure_label.empty()) {
        std::fprintf(stderr,
                     "[phenotype-windows] last failure label: %s\n",
                     g_renderer.last_failure_label.c_str());
    }
    std::fflush(stderr);
}

inline LONG CALLBACK phenotype_unhandled_exception_filter(
        EXCEPTION_POINTERS* exception) {
    auto const code = exception && exception->ExceptionRecord
        ? exception->ExceptionRecord->ExceptionCode
        : 0ul;
    auto const address = exception && exception->ExceptionRecord
        ? exception->ExceptionRecord->ExceptionAddress
        : nullptr;
    auto info = describe_address(address);
    std::fprintf(
        stderr,
        "[phenotype-windows] unhandled exception code=0x%08lx address=%p module=%s relative=0x%zx\n",
        code,
        address,
        info.module_path[0] ? info.module_path : "<unknown>",
        static_cast<std::size_t>(info.relative));
    dump_context_stack(exception ? exception->ContextRecord : nullptr);
    dump_runtime_diagnostics("unhandled exception");
    return EXCEPTION_CONTINUE_SEARCH;
}

inline void phenotype_terminate_handler() {
    auto current = std::current_exception();
    if (!current) {
        std::fprintf(stderr,
                     "[phenotype-windows] std::terminate without active exception\n");
    } else {
        try {
            std::rethrow_exception(current);
        } catch (std::exception const& ex) {
            std::fprintf(stderr,
                         "[phenotype-windows] std::terminate due to exception: %s\n",
                         ex.what());
        } catch (...) {
            std::fprintf(stderr,
                         "[phenotype-windows] std::terminate due to non-std exception\n");
        }
    }
    dump_runtime_diagnostics("std::terminate");
    std::fflush(stderr);
    TerminateProcess(GetCurrentProcess(), 3);
    std::_Exit(3);
}

inline void install_process_diagnostics() {
    static std::once_flag once;
    std::call_once(once, [] {
        SetUnhandledExceptionFilter(phenotype_unhandled_exception_filter);
        std::set_terminate(phenotype_terminate_handler);
    });
}

inline void invalidate_ime_window_positions() {
    g_ime.composition_window = {};
    g_ime.candidate_window = {};
}

inline bool should_defer_window_repaint() {
    return g_ime.in_wndproc
        || g_ime.sync_in_progress
        || g_ime.repaint_dispatch_in_progress;
}

inline void request_window_repaint() {
    ++g_ime.repaint_request_count;
    if (should_defer_window_repaint()) {
        g_ime.repaint_pending = true;
        ++g_ime.deferred_repaint_count;
        return;
    }
    if (g_ime.request_repaint)
        g_ime.request_repaint();
}

inline void drain_deferred_window_repaint() {
    if (!g_ime.repaint_pending || !g_ime.request_repaint)
        return;
    if (should_defer_window_repaint())
        return;
    g_ime.repaint_pending = false;
    g_ime.repaint_dispatch_in_progress = true;
    g_ime.request_repaint();
    g_ime.repaint_dispatch_in_progress = false;
}

inline std::vector<metrics::Attribute> native_platform_attrs() {
    return {{"platform", "windows"}};
}

inline std::string format_hresult_detail(char const* label, HRESULT hr) {
    char buffer[96]{};
    std::snprintf(
        buffer,
        std::size(buffer),
        "%s (hr=0x%08lx)",
        label,
        static_cast<unsigned long>(hr));
    return buffer;
}

inline void log_image_issue(std::string_view url, std::string_view detail) {
    std::fprintf(
        stderr,
        "[windows/image] '%.*s': %.*s\n",
        static_cast<int>(url.size()),
        url.data(),
        static_cast<int>(detail.size()),
        detail.data());
}

inline constexpr std::size_t image_atlas_byte_size() {
    return static_cast<std::size_t>(ImageAtlasState::atlas_size)
        * ImageAtlasState::atlas_size * 4;
}

inline std::expected<std::size_t, std::string> rgba_byte_size(UINT width, UINT height) {
    if (width == 0 || height == 0)
        return std::unexpected("Invalid image dimensions");
    auto width_sz = static_cast<std::size_t>(width);
    auto height_sz = static_cast<std::size_t>(height);
    if (width_sz > ((std::numeric_limits<std::size_t>::max)() / height_sz) / 4)
        return std::unexpected("RGBA pixel buffer overflow");
    return width_sz * height_sz * 4;
}

inline std::expected<UINT, std::string> rgba_stride(UINT width) {
    if (width == 0)
        return std::unexpected("Image row stride overflow");
    if (width > ((std::numeric_limits<UINT>::max)() / 4))
        return std::unexpected("Image row stride overflow");
    return width * 4;
}

inline void clear_image_entry(ImageCacheEntry& entry) {
    entry.u = 0.0f;
    entry.v = 0.0f;
    entry.uw = 0.0f;
    entry.vh = 0.0f;
    entry.failure_reason.clear();
}

inline void mark_image_entry_failed(ImageCacheEntry& entry,
                                    std::string_view url,
                                    std::string_view detail) {
    entry.state = ImageEntryState::failed;
    clear_image_entry(entry);
    entry.failure_reason = std::string(detail);
    log_image_issue(url, detail);
}

inline ImageCacheEntry* find_image_entry(std::string_view url) {
    for (auto& record : g_images.cache) {
        if (record.url == url)
            return &record.entry;
    }
    return nullptr;
}

inline ImageCacheEntry& ensure_image_entry(std::string const& url) {
    if (auto* entry = find_image_entry(url))
        return *entry;
    g_images.cache.push_back({url, {}});
    return g_images.cache.back().entry;
}

inline std::size_t pending_job_count_unlocked() {
    if (g_images.pending_head >= g_images.pending_jobs.size())
        return 0;
    return g_images.pending_jobs.size() - g_images.pending_head;
}

inline bool pending_jobs_empty_unlocked() {
    return pending_job_count_unlocked() == 0;
}

inline std::string pop_pending_job_unlocked() {
    auto index = g_images.pending_head;
    auto url = std::move(g_images.pending_jobs[index]);
    ++g_images.pending_head;
    if (g_images.pending_head >= g_images.pending_jobs.size()) {
        g_images.pending_jobs.clear();
        g_images.pending_head = 0;
    }
    return url;
}

inline bool is_remote_image_queued_unlocked(std::string const& url) {
    for (auto index = g_images.pending_head; index < g_images.pending_jobs.size(); ++index) {
        if (g_images.pending_jobs[index] == url)
            return true;
    }
    return false;
}

inline std::size_t snapshot_caret_byte_offset(
        ::phenotype::FocusedInputSnapshot const& snapshot) {
    if (snapshot.caret_pos == ::phenotype::native::invalid_callback_id)
        return snapshot.value.size();
    return ::phenotype::detail::clamp_utf8_boundary(snapshot.value, snapshot.caret_pos);
}

inline float measure_utf8_prefix(float font_size,
                                 bool mono,
                                 std::string const& text,
                                 std::size_t bytes) {
    bytes = ::phenotype::detail::clamp_utf8_boundary(text, bytes);
    if (bytes == 0)
        return 0.0f;
    return text_measure(
        font_size,
        mono,
        text.data(),
        static_cast<unsigned int>(bytes));
}

inline unsigned int composition_cursor_bytes() {
    auto cursor_units = static_cast<std::size_t>(
        (g_ime.composition_cursor < 0) ? 0 : g_ime.composition_cursor);
    if (cursor_units > g_ime.composition_text.size())
        cursor_units = g_ime.composition_text.size();
    auto prefix = cppx::unicode::wide_to_utf8(
        std::wstring_view(g_ime.composition_text.data(), cursor_units))
                      .value_or(std::string{});
    return static_cast<unsigned int>(prefix.size());
}

inline std::size_t resolved_composition_caret_bytes(std::string const& composition) {
    auto caret_bytes = static_cast<std::size_t>(composition_cursor_bytes());
    if (caret_bytes == 0 && !composition.empty())
        return composition.size();
    return caret_bytes;
}

inline void sync_input_debug_composition_state() {
    auto composition = cppx::unicode::wide_to_utf8(g_ime.composition_text)
                           .value_or(std::string{});
    ::phenotype::detail::set_input_composition_state(
        g_ime.composition_active && !composition.empty(),
        composition,
        composition_cursor_bytes());
}

struct WindowsCaretLayout {
    bool valid = false;
    bool composition_active = false;
    ::phenotype::FocusedInputSnapshot snapshot{};
    ::phenotype::FocusedInputCaretLayout base{};
    float draw_x = 0.0f;
    float draw_y = 0.0f;
    float content_x = 0.0f;
    float content_y = 0.0f;
    float height = 0.0f;
};

struct CompositionVisualState {
    bool valid = false;
    ::phenotype::FocusedInputSnapshot snapshot{};
    std::string erase_text;
    std::string visible_text;
    std::size_t marked_start = 0;
    std::size_t marked_end = 0;
    std::size_t caret_bytes = 0;
    float base_x = 0.0f;
    float text_y = 0.0f;
    float underline_x = 0.0f;
    float underline_width = 0.0f;
    float caret_x = 0.0f;
};

inline CompositionVisualState current_composition_visual_state(
        ::phenotype::FocusedInputSnapshot snapshot =
            ::phenotype::detail::focused_input_snapshot()) {
    CompositionVisualState visual{};
    if (!snapshot.valid || !g_ime.composition_active || g_ime.composition_text.empty())
        return visual;

    auto composition = cppx::unicode::wide_to_utf8(g_ime.composition_text)
                           .value_or(std::string{});
    if (composition.empty())
        return visual;

    auto anchor = ::phenotype::detail::clamp_utf8_boundary(
        snapshot.value,
        g_ime.replacement_start);
    auto replacement_end = ::phenotype::detail::clamp_utf8_boundary(
        snapshot.value,
        g_ime.replacement_end);
    if (replacement_end < anchor)
        replacement_end = anchor;
    auto prefix = snapshot.value.substr(0, anchor);
    auto suffix = snapshot.value.substr(replacement_end);

    visual.valid = true;
    visual.snapshot = std::move(snapshot);
    visual.erase_text = visual.snapshot.value.empty()
        ? visual.snapshot.placeholder
        : visual.snapshot.value;
    visual.visible_text = prefix + composition + suffix;
    visual.marked_start = prefix.size();
    visual.marked_end = visual.marked_start + composition.size();
    visual.caret_bytes = (std::min)(
        visual.visible_text.size(),
        visual.marked_start + resolved_composition_caret_bytes(composition));

    auto scroll_y = ::phenotype::detail::get_scroll_y();
    visual.base_x = visual.snapshot.x + visual.snapshot.padding[3];
    visual.text_y = visual.snapshot.y - scroll_y + visual.snapshot.padding[0];
    visual.underline_x = visual.base_x + measure_utf8_prefix(
        visual.snapshot.font_size,
        visual.snapshot.mono,
        visual.visible_text,
        visual.marked_start);
    auto underline_end = visual.base_x + measure_utf8_prefix(
        visual.snapshot.font_size,
        visual.snapshot.mono,
        visual.visible_text,
        visual.marked_end);
    visual.underline_width = underline_end - visual.underline_x;
    visual.caret_x = visual.base_x + measure_utf8_prefix(
        visual.snapshot.font_size,
        visual.snapshot.mono,
        visual.visible_text,
        visual.caret_bytes);
    return visual;
}

inline WindowsCaretLayout current_windows_caret_layout(
        ::phenotype::FocusedInputSnapshot snapshot =
            ::phenotype::detail::focused_input_snapshot()) {
    WindowsCaretLayout layout{};
    if (!snapshot.valid)
        return layout;

    auto composition = current_composition_visual_state(snapshot);
    auto measure_prefix = [](auto const& input, std::size_t bytes) {
        return measure_utf8_prefix(
            input.font_size,
            input.mono,
            input.value,
            bytes);
    };

    auto base_snapshot = snapshot;
    bool composition_active = composition.valid;
    if (composition_active)
        base_snapshot.caret_pos = static_cast<unsigned int>(g_ime.composition_anchor);
    layout.base = ::phenotype::detail::compute_single_line_caret_layout(
        base_snapshot,
        ::phenotype::detail::get_scroll_y(),
        composition_active,
        measure_prefix);
    if (!layout.base.valid)
        return layout;

    layout.valid = true;
    layout.composition_active = composition_active;
    layout.snapshot = std::move(snapshot);
    layout.draw_x = layout.base.draw_x;
    layout.draw_y = layout.base.draw_y;
    layout.content_x = layout.base.content_x;
    layout.content_y = layout.base.content_y;
    layout.height = layout.base.height;

    if (composition_active) {
        layout.draw_x = composition.caret_x;
        layout.content_x = composition.caret_x;
    }

    return layout;
}

inline void sync_windows_debug_caret_presentation() {
    auto layout = current_windows_caret_layout();
    if (!layout.valid || (!layout.snapshot.caret_visible && !layout.composition_active)) {
        ::phenotype::detail::clear_input_debug_caret_presentation();
        return;
    }

    ::phenotype::detail::set_input_debug_caret_presentation(
        "custom",
        layout.draw_x,
        layout.draw_y,
        1.5f,
        layout.height);
}

inline void capture_composition_anchor() {
    auto snapshot = ::phenotype::detail::focused_input_snapshot();
    if (!snapshot.valid)
        return;
    g_ime.composition_anchor = snapshot.selection_start;
    g_ime.replacement_start = snapshot.selection_start;
    g_ime.replacement_end = snapshot.selection_end;
    ::phenotype::detail::set_focused_input_selection(
        g_ime.composition_anchor,
        g_ime.composition_anchor);
}

inline bool is_http_url(std::string const& url) {
    return cppx::resource::is_remote(url);
}

inline std::filesystem::path resolve_image_path(std::string const& url) {
    return cppx::resource::resolve_path(
        std::filesystem::current_path(),
        std::string_view{url});
}

inline void wait_for_fence(UINT64 value);
inline void transition_resource(ID3D12Resource* resource,
                                D3D12_RESOURCE_STATES before,
                                D3D12_RESOURCE_STATES after);
inline std::pair<void*, UINT64> upload_alloc(UINT64 bytes, UINT64 alignment = 16);

inline HRESULT ensure_readback_buffer(UINT64 bytes) {
    if (!g_renderer.device || bytes == 0)
        return E_FAIL;
    if (g_renderer.readback.resource && g_renderer.readback.capacity >= bytes)
        return S_OK;

    g_renderer.readback.resource.Reset();
    g_renderer.readback.capacity = 0;

    D3D12_HEAP_PROPERTIES heap{};
    heap.Type = D3D12_HEAP_TYPE_READBACK;

    D3D12_RESOURCE_DESC desc{};
    desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    desc.Width = bytes;
    desc.Height = 1;
    desc.DepthOrArraySize = 1;
    desc.MipLevels = 1;
    desc.SampleDesc.Count = 1;
    desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

    auto hr = g_renderer.device->CreateCommittedResource(
        &heap,
        D3D12_HEAP_FLAG_NONE,
        &desc,
        D3D12_RESOURCE_STATE_COPY_DEST,
        nullptr,
        IID_PPV_ARGS(&g_renderer.readback.resource));
    if (FAILED(hr))
        return mark_renderer_lost("CreateCommittedResource(frame readback)", hr);

    g_renderer.readback.capacity = bytes;
    set_debug_name(g_renderer.readback.resource, L"phenotype.frame_readback");
    return S_OK;
}

inline void wait_for_all_frames() {
    for (auto const& frame : g_renderer.frames)
        wait_for_fence(frame.fence_value);
}

inline D3D12_CPU_DESCRIPTOR_HANDLE srv_cpu_handle(UINT slot) {
    auto handle = g_renderer.srv_heap->GetCPUDescriptorHandleForHeapStart();
    handle.ptr += static_cast<SIZE_T>(slot) * g_renderer.srv_descriptor_size;
    return handle;
}

inline D3D12_GPU_DESCRIPTOR_HANDLE srv_gpu_handle(UINT slot) {
    auto handle = g_renderer.srv_heap->GetGPUDescriptorHandleForHeapStart();
    handle.ptr += static_cast<UINT64>(slot) * g_renderer.srv_descriptor_size;
    return handle;
}

inline Color unpack_color(unsigned int packed) noexcept {
    return {
        static_cast<unsigned char>((packed >> 24) & 0xFF),
        static_cast<unsigned char>((packed >> 16) & 0xFF),
        static_cast<unsigned char>((packed >> 8) & 0xFF),
        static_cast<unsigned char>(packed & 0xFF),
    };
}

inline void append_color_instance(std::vector<float>& color_data,
                                  float x, float y, float w, float h,
                                  float r, float g, float b, float a,
                                  float p0 = 0.0f, float p1 = 0.0f,
                                  float kind = 0.0f, float p3 = 0.0f) {
    color_data.insert(color_data.end(), {
        x, y, w, h,
        r, g, b, a,
        p0, p1, kind, p3,
    });
}

inline void append_textured_quad(std::vector<float>& quad_data,
                                 float x, float y, float w, float h,
                                 float u, float v, float uw, float vh) {
    quad_data.insert(quad_data.end(), {
        x, y, w, h,
        u, v, uw, vh,
    });
}

struct CommandReader {
    unsigned char const* cur = nullptr;
    unsigned char const* end = nullptr;

    bool can_read(unsigned int bytes) const noexcept {
        return static_cast<std::size_t>(end - cur) >= bytes;
    }

    bool read_u32(unsigned int& value) noexcept {
        if (!can_read(4)) return false;
        std::memcpy(&value, cur, 4);
        cur += 4;
        return true;
    }

    bool read_f32(float& value) noexcept {
        unsigned int bits = 0;
        if (!read_u32(bits)) return false;
        std::memcpy(&value, &bits, 4);
        return true;
    }

    bool read_string(std::string& value, unsigned int len) {
        if (!can_read(len)) return false;
        value.assign(reinterpret_cast<char const*>(cur), len);
        cur += len;
        auto padded_len = (len + 3u) & ~3u;
        if (padded_len > len) {
            auto padding = padded_len - len;
            if (!can_read(padding)) return false;
            cur += padding;
        }
        return true;
    }
};

struct PendingImageCmd {
    float x = 0.0f;
    float y = 0.0f;
    float w = 0.0f;
    float h = 0.0f;
    std::string url;
};

struct DecodedFrame {
    double clear_r = 0.98;
    double clear_g = 0.98;
    double clear_b = 0.98;
    double clear_a = 1.0;
    std::vector<float> color_data;
    std::vector<float> overlay_color_data;
    std::vector<float> image_data;
    std::vector<TextEntry> text_entries;
    std::vector<HitRegionCmd> hit_regions;
    std::vector<PendingImageCmd> images;
};

inline bool decode_frame_commands(unsigned char const* buf,
                                  unsigned int len,
                                  float line_height_ratio,
                                  DecodedFrame& frame) {
    frame = {};
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
            frame.clear_r = color.r / 255.0;
            frame.clear_g = color.g / 255.0;
            frame.clear_b = color.b / 255.0;
            frame.clear_a = color.a / 255.0;
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
                frame.color_data,
                x, y, w, h,
                color.r / 255.0f, color.g / 255.0f,
                color.b / 255.0f, color.a / 255.0f);
            break;
        }
        case Cmd::StrokeRect: {
            float x = 0.0f, y = 0.0f, w = 0.0f, h = 0.0f;
            float line_width = 0.0f;
            unsigned int packed = 0;
            if (!reader.read_f32(x) || !reader.read_f32(y)
                || !reader.read_f32(w) || !reader.read_f32(h)
                || !reader.read_f32(line_width)
                || !reader.read_u32(packed))
                return false;
            auto color = unpack_color(packed);
            append_color_instance(
                frame.color_data,
                x, y, w, h,
                color.r / 255.0f, color.g / 255.0f,
                color.b / 255.0f, color.a / 255.0f,
                0.0f, line_width, 1.0f);
            break;
        }
        case Cmd::RoundRect: {
            float x = 0.0f, y = 0.0f, w = 0.0f, h = 0.0f;
            float radius = 0.0f;
            unsigned int packed = 0;
            if (!reader.read_f32(x) || !reader.read_f32(y)
                || !reader.read_f32(w) || !reader.read_f32(h)
                || !reader.read_f32(radius)
                || !reader.read_u32(packed))
                return false;
            auto color = unpack_color(packed);
            append_color_instance(
                frame.color_data,
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
            std::string text;
            if (!reader.read_f32(x) || !reader.read_f32(y)
                || !reader.read_f32(font_size)
                || !reader.read_u32(mono)
                || !reader.read_u32(packed)
                || !reader.read_u32(text_len)
                || !reader.read_string(text, text_len))
                return false;
            auto color = unpack_color(packed);
            frame.text_entries.push_back({
                x,
                y,
                font_size,
                mono != 0,
                color.r / 255.0f,
                color.g / 255.0f,
                color.b / 255.0f,
                color.a / 255.0f,
                std::move(text),
                font_size * line_height_ratio,
            });
            break;
        }
        case Cmd::DrawLine: {
            float x1 = 0.0f, y1 = 0.0f, x2 = 0.0f, y2 = 0.0f;
            float thickness = 0.0f;
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
                frame.color_data,
                x, y, w, h,
                color.r / 255.0f, color.g / 255.0f,
                color.b / 255.0f, color.a / 255.0f,
                0.0f, 0.0f, 3.0f);
            break;
        }
        case Cmd::HitRegion: {
            HitRegionCmd hit{};
            if (!reader.read_f32(hit.x) || !reader.read_f32(hit.y)
                || !reader.read_f32(hit.w) || !reader.read_f32(hit.h)
                || !reader.read_u32(hit.callback_id)
                || !reader.read_u32(hit.cursor_type))
                return false;
            frame.hit_regions.push_back(hit);
            break;
        }
        case Cmd::DrawImage: {
            PendingImageCmd image{};
            unsigned int url_len = 0;
            if (!reader.read_f32(image.x) || !reader.read_f32(image.y)
                || !reader.read_f32(image.w) || !reader.read_f32(image.h)
                || !reader.read_u32(url_len)
                || !reader.read_string(image.url, url_len))
                return false;
            frame.images.push_back(std::move(image));
            break;
        }
        case Cmd::Scissor: {
            float x = 0.0f, y = 0.0f, w = 0.0f, h = 0.0f;
            if (!reader.read_f32(x) || !reader.read_f32(y)
                || !reader.read_f32(w) || !reader.read_f32(h))
                return false;
            // Decoded-and-skipped: D3D12 RSSetScissorRects needs to be
            // interleaved between draw calls, but the current pipeline
            // issues one instanced draw per bucket. Routing Scissor to
            // the command list requires a batch split — deferred to a
            // follow-up. Parsing is still required so paint_node can
            // emit Scissor bytes without the backend erroring.
            (void)x; (void)y; (void)w; (void)h;
            break;
        }
        default:
            return false;
        }
    }
    return true;
}

inline bool matches_focused_input_base_text_entry(
        TextEntry const& entry,
        ::phenotype::FocusedInputSnapshot const& snapshot,
        std::string const& rendered_text,
        float expected_x,
        float expected_y) {
    return entry.text == rendered_text
        && entry.mono == snapshot.mono
        && std::fabs(entry.font_size - snapshot.font_size) < 0.01f
        && std::fabs(entry.x - expected_x) < 0.75f
        && std::fabs(entry.y - expected_y) < 0.75f;
}

inline bool suppress_focused_input_base_text_for_composition(DecodedFrame& frame) {
    auto visual = current_composition_visual_state();
    if (!visual.valid)
        return false;

    auto const& snapshot = visual.snapshot;
    auto rendered_text = snapshot.value.empty()
        ? snapshot.placeholder
        : snapshot.value;
    if (rendered_text.empty())
        return false;

    float expected_x = snapshot.x + snapshot.padding[3];
    float expected_y = snapshot.y - ::phenotype::detail::get_scroll_y() + snapshot.padding[0];
    auto original_size = frame.text_entries.size();
    std::erase_if(
        frame.text_entries,
        [&](TextEntry const& entry) {
            return matches_focused_input_base_text_entry(
                entry,
                snapshot,
                rendered_text,
                expected_x,
                expected_y);
        });
    return frame.text_entries.size() != original_size;
}

inline std::vector<TextEntry> composition_overlay_text_entries() {
    auto visual = current_composition_visual_state();
    if (!visual.valid)
        return {};

    return {{
        visual.base_x,
        visual.text_y,
        visual.snapshot.font_size,
        visual.snapshot.mono,
        visual.snapshot.foreground.r / 255.0f,
        visual.snapshot.foreground.g / 255.0f,
        visual.snapshot.foreground.b / 255.0f,
        visual.snapshot.foreground.a / 255.0f,
        visual.visible_text,
        visual.snapshot.line_height,
    }};
}

inline std::optional<CandidateHit> find_candidate_hit(float x, float y) {
    for (auto const& hit : g_ime.overlay.hits) {
        if (hit.contains(x, y))
            return hit;
    }
    return std::nullopt;
}

inline std::vector<std::wstring> load_candidate_strings(HIMC himc) {
    std::vector<std::wstring> out;
    auto bytes = ImmGetCandidateListW(himc, 0, nullptr, 0);
    if (bytes == 0)
        return out;
    std::vector<unsigned char> storage(static_cast<std::size_t>(bytes), 0);
    auto* list = reinterpret_cast<CANDIDATELIST*>(storage.data());
    auto written = ImmGetCandidateListW(himc, 0, list, bytes);
    if (written == 0)
        return out;
    out.reserve(list->dwCount);
    for (DWORD i = 0; i < list->dwCount; ++i) {
        auto offset = list->dwOffset[i];
        auto const* text = reinterpret_cast<wchar_t const*>(storage.data() + offset);
        out.emplace_back(text ? text : L"");
    }
    g_ime.candidate_selection = list->dwSelection;
    g_ime.candidate_page_start = list->dwPageStart;
    g_ime.candidate_page_size = list->dwPageSize;
    return out;
}

inline void update_candidate_state(HIMC himc) {
    g_ime.candidates = load_candidate_strings(himc);
    g_ime.candidate_open = !g_ime.candidates.empty();
    if (!g_ime.candidate_open) {
        g_ime.hovered_candidate = -1;
        g_ime.hovered_kind = CandidateHitKind::none;
    }
}

inline void clear_ime_state() {
    g_ime.composition_active = false;
    g_ime.composition_text.clear();
    g_ime.composition_cursor = 0;
    g_ime.composition_anchor = 0;
    g_ime.replacement_start = 0;
    g_ime.replacement_end = 0;
    g_ime.candidate_open = false;
    g_ime.candidates.clear();
    g_ime.candidate_selection = 0;
    g_ime.candidate_page_start = 0;
    g_ime.candidate_page_size = 0;
    g_ime.hovered_candidate = -1;
    g_ime.hovered_kind = CandidateHitKind::none;
    g_ime.overlay = {};
    invalidate_ime_window_positions();
    ::phenotype::detail::clear_input_composition_state();
}

inline void commit_result_string(std::wstring_view result) {
    auto suffix = cppx::unicode::wide_to_utf8(result).value_or(std::string{});
    if (suffix.empty())
        return;
    if (!::phenotype::detail::replace_focused_input_text(
            g_ime.replacement_start,
            g_ime.replacement_end,
            suffix))
        return;
    g_ime.composition_anchor = g_ime.replacement_start + suffix.size();
    g_ime.replacement_start = g_ime.composition_anchor;
    g_ime.replacement_end = g_ime.composition_anchor;
}

inline void update_composition_state(HWND hwnd, LPARAM lparam) {
    auto himc = ImmGetContext(hwnd);
    if (!himc)
        return;

    if ((lparam & (GCS_RESULTSTR | GCS_COMPSTR)) && !g_ime.composition_active)
        capture_composition_anchor();

    if (lparam & GCS_RESULTSTR) {
        auto bytes = ImmGetCompositionStringW(himc, GCS_RESULTSTR, nullptr, 0);
        if (bytes > 0) {
            std::wstring result(static_cast<std::size_t>(bytes / sizeof(wchar_t)), L'\0');
            ImmGetCompositionStringW(
                himc,
                GCS_RESULTSTR,
                result.data(),
                bytes);
            commit_result_string(result);
        }
    }

    if (lparam & GCS_COMPSTR) {
        auto bytes = ImmGetCompositionStringW(himc, GCS_COMPSTR, nullptr, 0);
        if (bytes > 0) {
            g_ime.composition_text.assign(static_cast<std::size_t>(bytes / sizeof(wchar_t)), L'\0');
            ImmGetCompositionStringW(
                himc,
                GCS_COMPSTR,
                g_ime.composition_text.data(),
                bytes);
            g_ime.composition_active = true;
        } else {
            g_ime.composition_text.clear();
            g_ime.composition_active = false;
        }
    }

    if (lparam & GCS_CURSORPOS) {
        auto pos = ImmGetCompositionStringW(himc, GCS_CURSORPOS, nullptr, 0);
        g_ime.composition_cursor = (pos >= 0) ? pos : 0;
    } else if (g_ime.composition_cursor > static_cast<LONG>(g_ime.composition_text.size())) {
        g_ime.composition_cursor = static_cast<LONG>(g_ime.composition_text.size());
    }

    update_candidate_state(himc);
    ImmReleaseContext(hwnd, himc);
    sync_input_debug_composition_state();
}

inline void sync_ime_windows() {
    if (!g_ime.hwnd)
        return;
    ++g_ime.sync_call_count;
    if (g_ime.sync_in_progress)
        return;

    {
        FlagGuard sync_guard{g_ime.sync_in_progress};

        auto layout = current_windows_caret_layout();
        if (!layout.valid) {
            clear_ime_state();
            ::phenotype::detail::clear_input_debug_caret_presentation();
        } else {
            auto const& snapshot = layout.snapshot;

            g_ime.overlay = {};

            auto himc = ImmGetContext(g_ime.hwnd);
            if (!himc) {
                sync_windows_debug_caret_presentation();
            } else {
                LONG composition_x = static_cast<LONG>(std::lround(layout.content_x));
                LONG composition_y = static_cast<LONG>(std::lround(layout.content_y));
                if (!g_ime.composition_window.valid
                    || g_ime.composition_window.x != composition_x
                    || g_ime.composition_window.y != composition_y) {
                    COMPOSITIONFORM comp{};
                    comp.dwStyle = CFS_FORCE_POSITION;
                    comp.ptCurrentPos.x = composition_x;
                    comp.ptCurrentPos.y = composition_y;
                    if (ImmSetCompositionWindow(himc, &comp)) {
                        g_ime.composition_window = {
                            true,
                            composition_x,
                            composition_y,
                        };
                    } else {
                        g_ime.composition_window = {};
                    }
                }

                LONG candidate_x = static_cast<LONG>(std::lround(layout.content_x));
                LONG candidate_y = static_cast<LONG>(std::lround(layout.draw_y + snapshot.height));
                if (!g_ime.candidate_window.valid
                    || g_ime.candidate_window.x != candidate_x
                    || g_ime.candidate_window.y != candidate_y) {
                    CANDIDATEFORM cand{};
                    cand.dwIndex = 0;
                    cand.dwStyle = CFS_CANDIDATEPOS;
                    cand.ptCurrentPos.x = candidate_x;
                    cand.ptCurrentPos.y = candidate_y;
                    if (ImmSetCandidateWindow(himc, &cand)) {
                        g_ime.candidate_window = {
                            true,
                            candidate_x,
                            candidate_y,
                        };
                    } else {
                        g_ime.candidate_window = {};
                    }
                }

                ImmReleaseContext(g_ime.hwnd, himc);
                sync_input_debug_composition_state();
                sync_windows_debug_caret_presentation();

                if (!g_ime.candidate_open || g_ime.candidates.empty()) {
                    g_ime.hovered_candidate = -1;
                    g_ime.hovered_kind = CandidateHitKind::none;
                } else {
                    int winw = 0;
                    int winh = 0;
                    glfwGetWindowSize(g_ime.window, &winw, &winh);
                    if (winw <= 0) winw = 1;
                    if (winh <= 0) winh = 1;

                    auto total = static_cast<unsigned int>(g_ime.candidates.size());
                    auto page_size = (g_ime.candidate_page_size > 0)
                        ? static_cast<unsigned int>(g_ime.candidate_page_size)
                        : static_cast<unsigned int>((total > 8u) ? 8u : total);
                    if (page_size == 0)
                        page_size = 1;
                    auto page_start = static_cast<unsigned int>(g_ime.candidate_page_start);
                    if (page_start >= total)
                        page_start = 0;
                    auto page_end = (page_start + page_size < total)
                        ? (page_start + page_size)
                        : total;
                    auto visible_count = page_end - page_start;
                    bool has_prev = page_start > 0;
                    bool has_next = page_end < total;

                    float row_height = snapshot.line_height + 10.0f;
                    if (row_height < snapshot.font_size + 14.0f)
                        row_height = snapshot.font_size + 14.0f;

                    float content_width = snapshot.width;
                    for (unsigned int index = page_start; index < page_end; ++index) {
                        auto utf8 = cppx::unicode::wide_to_utf8(g_ime.candidates[index])
                                        .value_or(std::string{});
                        if (utf8.empty())
                            continue;
                        float measured = text_measure(
                            snapshot.font_size,
                            snapshot.mono,
                            utf8.c_str(),
                            static_cast<unsigned int>(utf8.size()));
                        if (measured > content_width)
                            content_width = measured;
                    }

                    float panel_width = content_width + 24.0f;
                    if (panel_width < snapshot.width)
                        panel_width = snapshot.width;
                    if (panel_width < 160.0f)
                        panel_width = 160.0f;

                    float footer_height = (has_prev || has_next) ? row_height : 0.0f;
                    float panel_height = visible_count * row_height + footer_height;
                    float panel_x = snapshot.x;
                    if (panel_x + panel_width > static_cast<float>(winw) - 8.0f)
                        panel_x = static_cast<float>(winw) - panel_width - 8.0f;
                    if (panel_x < 8.0f)
                        panel_x = 8.0f;

                    float panel_y = layout.draw_y + snapshot.height + 4.0f;
                    if (panel_y + panel_height > static_cast<float>(winh) - 8.0f)
                        panel_y = layout.draw_y - panel_height - 4.0f;
                    if (panel_y < 8.0f)
                        panel_y = 8.0f;

                    g_ime.overlay.visible = true;
                    g_ime.overlay.x = panel_x;
                    g_ime.overlay.y = panel_y;
                    g_ime.overlay.w = panel_width;
                    g_ime.overlay.h = panel_height;
                    g_ime.overlay.row_height = row_height;
                    g_ime.overlay.hits.clear();
                    g_ime.overlay.hits.reserve(
                        static_cast<std::size_t>(visible_count)
                            + (has_prev ? 1u : 0u)
                            + (has_next ? 1u : 0u));

                    for (unsigned int row = 0; row < visible_count; ++row) {
                        g_ime.overlay.hits.push_back({
                            CandidateHitKind::item,
                            page_start + row,
                            panel_x,
                            panel_y + row * row_height,
                            panel_width,
                            row_height,
                        });
                    }

                    if (footer_height > 0.0f) {
                        float footer_y = panel_y + visible_count * row_height;
                        float button_width = (panel_width - 24.0f) * 0.5f;
                        if (button_width < 56.0f)
                            button_width = 56.0f;
                        if (has_prev) {
                            g_ime.overlay.hits.push_back({
                                CandidateHitKind::prev_page,
                                0,
                                panel_x + 8.0f,
                                footer_y + 4.0f,
                                button_width,
                                footer_height - 8.0f,
                            });
                        }
                        if (has_next) {
                            g_ime.overlay.hits.push_back({
                                CandidateHitKind::next_page,
                                0,
                                panel_x + panel_width - button_width - 8.0f,
                                footer_y + 4.0f,
                                button_width,
                                footer_height - 8.0f,
                            });
                        }
                    }

                    if (g_ime.hovered_kind == CandidateHitKind::item) {
                        if (g_ime.hovered_candidate < static_cast<int>(page_start)
                            || g_ime.hovered_candidate >= static_cast<int>(page_end)) {
                            g_ime.hovered_candidate = -1;
                            g_ime.hovered_kind = CandidateHitKind::none;
                        }
                    } else if (
                        (g_ime.hovered_kind == CandidateHitKind::prev_page && !has_prev)
                        || (g_ime.hovered_kind == CandidateHitKind::next_page && !has_next)) {
                        g_ime.hovered_kind = CandidateHitKind::none;
                    }
                }
            }
        }
    }

    drain_deferred_window_repaint();
}

inline bool handle_candidate_click(CandidateHit const& hit) {
    if (!g_ime.hwnd)
        return false;
    auto himc = ImmGetContext(g_ime.hwnd);
    if (!himc)
        return false;

    BOOL ok = FALSE;
    if (hit.kind == CandidateHitKind::item) {
        ok = ImmNotifyIME(himc, NI_SELECTCANDIDATESTR, 0, hit.index);
    } else if (hit.kind == CandidateHitKind::prev_page) {
        auto next = (g_ime.candidate_page_start > g_ime.candidate_page_size)
            ? (g_ime.candidate_page_start - g_ime.candidate_page_size)
            : 0;
        ok = ImmNotifyIME(himc, NI_SETCANDIDATE_PAGESTART, 0, next);
    } else if (hit.kind == CandidateHitKind::next_page) {
        auto next = g_ime.candidate_page_start + g_ime.candidate_page_size;
        ok = ImmNotifyIME(himc, NI_SETCANDIDATE_PAGESTART, 0, next);
    }
    ImmReleaseContext(g_ime.hwnd, himc);
    return ok != FALSE;
}

inline bool ime_notify_updates_candidates(WPARAM command) {
    return command == IMN_OPENCANDIDATE || command == IMN_CHANGECANDIDATE;
}

inline bool ime_notify_needs_repaint(WPARAM command) {
    return ime_notify_updates_candidates(command)
        || command == IMN_CLOSECANDIDATE;
}

inline LRESULT CALLBACK ime_wndproc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    FlagGuard wndproc_guard{g_ime.in_wndproc};

    switch (msg) {
    case WM_IME_SETCONTEXT: {
        LPARAM filtered = lparam;
        filtered &= ~static_cast<LPARAM>(ISC_SHOWUICOMPOSITIONWINDOW);
        filtered &= ~static_cast<LPARAM>(ISC_SHOWUICANDIDATEWINDOW);
        filtered &= ~static_cast<LPARAM>(ISC_SHOWUICANDIDATEWINDOW << 1);
        filtered &= ~static_cast<LPARAM>(ISC_SHOWUICANDIDATEWINDOW << 2);
        filtered &= ~static_cast<LPARAM>(ISC_SHOWUICANDIDATEWINDOW << 3);
        return CallWindowProcW(g_ime.prev_wndproc, hwnd, msg, wparam, filtered);
    }
    case WM_IME_STARTCOMPOSITION:
        capture_composition_anchor();
        g_ime.composition_active = true;
        g_ime.composition_text.clear();
        g_ime.composition_cursor = 0;
        request_window_repaint();
        return 0;
    case WM_IME_COMPOSITION:
        update_composition_state(hwnd, lparam);
        request_window_repaint();
        return 0;
    case WM_IME_ENDCOMPOSITION:
        clear_ime_state();
        request_window_repaint();
        return 0;
    case WM_IME_NOTIFY: {
        auto himc = ImmGetContext(hwnd);
        if (himc) {
            if (ime_notify_updates_candidates(wparam)) {
                update_candidate_state(himc);
            } else if (wparam == IMN_CLOSECANDIDATE) {
                g_ime.candidate_open = false;
                g_ime.candidates.clear();
                g_ime.overlay = {};
            }
            ImmReleaseContext(hwnd, himc);
        }
        if (ime_notify_needs_repaint(wparam))
            request_window_repaint();
        return 0;
    }
    case WM_PHENOTYPE_IMAGE_READY:
        request_window_repaint();
        return 0;
    case WM_KILLFOCUS:
        clear_ime_state();
        break;
    default:
        break;
    }
    return CallWindowProcW(g_ime.prev_wndproc, hwnd, msg, wparam, lparam);
}

inline void input_attach(native_surface_handle handle, void (*request_repaint)()) {
    auto* window = static_cast<GLFWwindow*>(handle);
    g_ime.window = window;
    g_ime.request_repaint = request_repaint;
    g_ime.hwnd = window ? glfwGetWin32Window(window) : nullptr;
    g_ime.ui_thread_id = GetCurrentThreadId();
    g_ime.in_wndproc = false;
    g_ime.sync_in_progress = false;
    g_ime.repaint_pending = false;
    g_ime.repaint_dispatch_in_progress = false;
    invalidate_ime_window_positions();
    if (!g_ime.hwnd || g_ime.attached)
        return;
    g_ime.prev_wndproc = reinterpret_cast<WNDPROC>(
        SetWindowLongPtrW(g_ime.hwnd, GWLP_WNDPROC,
                          reinterpret_cast<LONG_PTR>(ime_wndproc)));
    g_ime.attached = (g_ime.prev_wndproc != nullptr);
}

inline void input_detach() {
    if (g_ime.hwnd && g_ime.prev_wndproc) {
        SetWindowLongPtrW(g_ime.hwnd, GWLP_WNDPROC,
                          reinterpret_cast<LONG_PTR>(g_ime.prev_wndproc));
    }
    clear_ime_state();
    g_ime.window = nullptr;
    g_ime.hwnd = nullptr;
    g_ime.prev_wndproc = nullptr;
    g_ime.request_repaint = nullptr;
    g_ime.ui_thread_id = 0;
    g_ime.in_wndproc = false;
    g_ime.sync_in_progress = false;
    g_ime.repaint_pending = false;
    g_ime.repaint_dispatch_in_progress = false;
    g_ime.attached = false;
}

inline bool input_handle_cursor_pos(float x, float y) {
    if (!g_ime.overlay.visible)
        return false;
    auto hit = find_candidate_hit(x, y);
    int next_hover = -1;
    CandidateHitKind next_kind = CandidateHitKind::none;
    if (hit.has_value()) {
        next_kind = hit->kind;
        if (hit->kind == CandidateHitKind::item)
            next_hover = static_cast<int>(hit->index);
    }
    bool inside_panel = hit.has_value()
        || (x >= g_ime.overlay.x && x <= g_ime.overlay.x + g_ime.overlay.w
            && y >= g_ime.overlay.y && y <= g_ime.overlay.y + g_ime.overlay.h);
    if (next_hover != g_ime.hovered_candidate || next_kind != g_ime.hovered_kind) {
        g_ime.hovered_candidate = next_hover;
        g_ime.hovered_kind = next_kind;
        request_window_repaint();
    }
    return inside_panel;
}

inline bool input_handle_mouse_button(float x, float y,
                                      int button, int action, int) {
    if (button != GLFW_MOUSE_BUTTON_LEFT || action != GLFW_PRESS)
        return false;
    auto hit = find_candidate_hit(x, y);
    bool inside_panel = g_ime.overlay.visible
        && x >= g_ime.overlay.x && x <= g_ime.overlay.x + g_ime.overlay.w
        && y >= g_ime.overlay.y && y <= g_ime.overlay.y + g_ime.overlay.h;
    if (!hit.has_value())
        return inside_panel;
    auto ok = handle_candidate_click(*hit);
    request_window_repaint();
    return inside_panel || ok;
}

inline bool input_dismiss_transient() {
    bool had_transient = g_ime.composition_active
        || !g_ime.composition_text.empty()
        || g_ime.candidate_open
        || !g_ime.candidates.empty()
        || g_ime.overlay.visible;
    if (!had_transient)
        return false;
    clear_ime_state();
    request_window_repaint();
    return true;
}

inline std::expected<DecodedImage, std::string> decode_image_with_decoder(
        IWICImagingFactory* factory,
        IWICBitmapDecoder* decoder) {
    ComPtr<IWICBitmapFrameDecode> frame;
    auto hr = decoder->GetFrame(0, &frame);
    if (FAILED(hr))
        return std::unexpected(format_hresult_detail("GetFrame", hr));

    UINT width = 0;
    UINT height = 0;
    hr = frame->GetSize(&width, &height);
    if (FAILED(hr))
        return std::unexpected(format_hresult_detail("GetSize", hr));
    if (width == 0 || height == 0)
        return std::unexpected("GetSize returned zero dimensions");
    if (width > static_cast<UINT>(ImageAtlasState::atlas_size)
        || height > static_cast<UINT>(ImageAtlasState::atlas_size)) {
        return std::unexpected("Image dimensions exceed the 2048x2048 atlas");
    }

    ComPtr<IWICFormatConverter> converter;
    hr = factory->CreateFormatConverter(&converter);
    if (FAILED(hr))
        return std::unexpected(format_hresult_detail("CreateFormatConverter", hr));

    hr = converter->Initialize(
        frame.Get(),
        GUID_WICPixelFormat32bppRGBA,
        WICBitmapDitherTypeNone,
        nullptr,
        0.0,
        WICBitmapPaletteTypeCustom);
    if (FAILED(hr))
        return std::unexpected(format_hresult_detail("IWICFormatConverter::Initialize", hr));

    auto byte_size = rgba_byte_size(width, height);
    if (!byte_size)
        return std::unexpected(byte_size.error());

    auto stride = rgba_stride(width);
    if (!stride)
        return std::unexpected(stride.error());
    if (*byte_size > (std::numeric_limits<UINT>::max)())
        return std::unexpected("Decoded image buffer is too large for WIC CopyPixels");

    DecodedImage decoded;
    decoded.width = static_cast<int>(width);
    decoded.height = static_cast<int>(height);
    decoded.pixels.resize(*byte_size);
    hr = converter->CopyPixels(
        nullptr,
        *stride,
        static_cast<UINT>(*byte_size),
        decoded.pixels.data());
    if (FAILED(hr))
        return std::unexpected(format_hresult_detail("IWICBitmapSource::CopyPixels", hr));
    if (decoded.pixels.size() != *byte_size)
        return std::unexpected("Decoded image buffer size mismatch");
    return decoded;
}

inline std::expected<DecodedImage, std::string> decode_image_file(
        std::filesystem::path const& path) {
    ComPtr<IWICImagingFactory> factory;
    auto hr = CoCreateInstance(
        CLSID_WICImagingFactory,
        nullptr,
        CLSCTX_INPROC_SERVER,
        IID_PPV_ARGS(&factory));
    if (FAILED(hr))
        return std::unexpected(format_hresult_detail("CoCreateInstance(IWICImagingFactory)", hr));

    ComPtr<IWICBitmapDecoder> decoder;
    auto wide = path.wstring();
    hr = factory->CreateDecoderFromFilename(
        wide.c_str(),
        nullptr,
        GENERIC_READ,
        WICDecodeMetadataCacheOnLoad,
        &decoder);
    if (FAILED(hr))
        return std::unexpected(format_hresult_detail("CreateDecoderFromFilename", hr));
    return decode_image_with_decoder(factory.Get(), decoder.Get());
}

inline std::expected<DecodedImage, std::string> decode_image_memory(
        std::vector<unsigned char> const& bytes) {
    if (bytes.empty())
        return std::unexpected("Remote image response body is empty");

    ComPtr<IWICImagingFactory> factory;
    auto hr = CoCreateInstance(
        CLSID_WICImagingFactory,
        nullptr,
        CLSCTX_INPROC_SERVER,
        IID_PPV_ARGS(&factory));
    if (FAILED(hr)) {
        return std::unexpected(
            format_hresult_detail("CoCreateInstance(IWICImagingFactory)", hr));
    }

    HGLOBAL buffer = GlobalAlloc(GMEM_MOVEABLE, bytes.size());
    if (!buffer)
        return std::unexpected("GlobalAlloc failed for remote image body");

    struct GlobalBufferGuard {
        HGLOBAL handle = nullptr;

        ~GlobalBufferGuard() {
            if (handle)
                GlobalFree(handle);
        }
    } guard{buffer};

    auto* locked = GlobalLock(buffer);
    if (!locked)
        return std::unexpected("GlobalLock failed for remote image body");

    std::memcpy(locked, bytes.data(), bytes.size());
    GlobalUnlock(buffer);

    ComPtr<IStream> stream;
    hr = CreateStreamOnHGlobal(buffer, TRUE, &stream);
    if (FAILED(hr))
        return std::unexpected(format_hresult_detail("CreateStreamOnHGlobal", hr));
    guard.handle = nullptr;

    ComPtr<IWICBitmapDecoder> decoder;
    hr = factory->CreateDecoderFromStream(
        stream.Get(),
        nullptr,
        WICDecodeMetadataCacheOnLoad,
        &decoder);
    if (FAILED(hr))
        return std::unexpected(format_hresult_detail("CreateDecoderFromStream", hr));
    return decode_image_with_decoder(factory.Get(), decoder.Get());
}

inline void store_decoded_image(DecodedImage decoded) {
    auto& entry = ensure_image_entry(decoded.url);
    clear_image_entry(entry);

    if (decoded.failed) {
        mark_image_entry_failed(
            entry,
            decoded.url,
            decoded.error_detail.empty()
                ? "Remote image decode failed"
                : decoded.error_detail);
        return;
    }

    if (decoded.width <= 0 || decoded.height <= 0) {
        mark_image_entry_failed(entry, decoded.url, "Decoded image has invalid dimensions");
        return;
    }

    auto expected_bytes = rgba_byte_size(
        static_cast<UINT>(decoded.width),
        static_cast<UINT>(decoded.height));
    if (!expected_bytes) {
        mark_image_entry_failed(entry, decoded.url, expected_bytes.error());
        return;
    }
    if (decoded.pixels.size() != *expected_bytes) {
        mark_image_entry_failed(entry, decoded.url, "Decoded image buffer size mismatch");
        return;
    }
    if (decoded.width > ImageAtlasState::atlas_size
        || decoded.height > ImageAtlasState::atlas_size) {
        mark_image_entry_failed(entry, decoded.url, "Image dimensions exceed the 2048x2048 atlas");
        return;
    }

    if (g_images.pixels.empty()) {
        g_images.pixels.resize(image_atlas_byte_size(), 0);
    } else if (g_images.pixels.size() != image_atlas_byte_size()) {
        mark_image_entry_failed(entry, decoded.url, "Image atlas backing store has an unexpected size");
        return;
    }

    auto slot_x = g_images.cursor_x;
    auto slot_y = g_images.cursor_y;
    auto row_height = g_images.row_height;
    if (slot_x < 0 || slot_y < 0 || row_height < 0) {
        mark_image_entry_failed(entry, decoded.url, "Image atlas packing state is invalid");
        return;
    }

    auto const atlas_size = static_cast<long long>(ImageAtlasState::atlas_size);
    if (static_cast<long long>(slot_x) + decoded.width + 1 > atlas_size) {
        slot_x = 0;
        auto next_row = static_cast<long long>(slot_y) + row_height;
        if (next_row > atlas_size) {
            mark_image_entry_failed(entry, decoded.url, "Image atlas packing overflow");
            return;
        }
        slot_y = static_cast<int>(next_row);
        row_height = 0;
    }
    if (static_cast<long long>(slot_y) + decoded.height > atlas_size) {
        mark_image_entry_failed(entry, decoded.url, "Image atlas overflow");
        return;
    }

    auto next_cursor_x = static_cast<long long>(slot_x) + decoded.width + 1;
    if (next_cursor_x > atlas_size) {
        mark_image_entry_failed(entry, decoded.url, "Image atlas packing overflow");
        return;
    }
    auto next_row_height = (std::max)(row_height, decoded.height + 1);

    for (int row = 0; row < decoded.height; ++row) {
        auto* dst = g_images.pixels.data()
            + static_cast<std::size_t>(((slot_y + row) * ImageAtlasState::atlas_size + slot_x) * 4);
        auto const* src = decoded.pixels.data()
            + static_cast<std::size_t>(row * decoded.width * 4);
        std::memcpy(dst, src, static_cast<std::size_t>(decoded.width) * 4);
    }

    entry.state = ImageEntryState::ready;
    entry.u = static_cast<float>(slot_x) / ImageAtlasState::atlas_size;
    entry.v = static_cast<float>(slot_y) / ImageAtlasState::atlas_size;
    entry.uw = static_cast<float>(decoded.width) / ImageAtlasState::atlas_size;
    entry.vh = static_cast<float>(decoded.height) / ImageAtlasState::atlas_size;
    g_images.cursor_x = static_cast<int>(next_cursor_x);
    g_images.cursor_y = slot_y;
    g_images.row_height = next_row_height;
    g_images.atlas_dirty = true;
}

inline bool process_completed_images() {
    std::vector<DecodedImage> completed;
    {
        std::lock_guard lock(g_images.mutex);
        if (g_images.completed_jobs.empty())
            return false;
        completed.swap(g_images.completed_jobs);
    }

    bool changed = false;
    for (auto& decoded : completed) {
        store_decoded_image(std::move(decoded));
        changed = true;
    }
    return changed;
}

inline void image_worker_main() {
    g_images.worker_thread_id = GetCurrentThreadId();
    auto co_hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (FAILED(co_hr) && co_hr != RPC_E_CHANGED_MODE) {
        std::fprintf(stderr, "[windows] image worker CoInitializeEx failed (hr=0x%08lx)\n",
                     static_cast<unsigned long>(co_hr));
    }

    for (;;) {
        std::string url;
        {
            std::unique_lock lock(g_images.mutex);
            g_images.cv.wait(lock, [] {
                return g_images.stop_worker || !pending_jobs_empty_unlocked();
            });
            if (pending_jobs_empty_unlocked()) {
                if (g_images.stop_worker)
                    break;
                continue;
            }
            url = pop_pending_job_unlocked();
        }

        DecodedImage decoded;
        decoded.url = url;
        try {
            auto response = cppx::http::system::get(url);
            if (!response) {
                decoded.failed = true;
                decoded.error_detail = std::string(cppx::http::to_string(response.error()));
            } else {
                if (!response->stat.ok()) {
                    decoded.failed = true;
                    decoded.error_detail = "HTTP status " + std::to_string(response->stat.code);
                } else {
                    std::vector<unsigned char> body;
                    body.reserve(response->body.size());
                    auto const* body_data = response->body.data();
                    for (std::size_t i = 0, n = response->body.size(); i < n; ++i)
                        body.push_back(static_cast<unsigned char>(body_data[i]));
                    auto result = decode_image_memory(body);
                    if (result) {
                        decoded = std::move(*result);
                        decoded.url = url;
                    } else {
                        decoded.failed = true;
                        decoded.error_detail = result.error();
                    }
                }
            }
        } catch (std::exception const& ex) {
            decoded.failed = true;
            decoded.error_detail = std::string("Worker exception: ") + ex.what();
            std::fprintf(stderr,
                         "[windows] image worker exception for %s: %s\n",
                         url.c_str(),
                         ex.what());
        } catch (...) {
            decoded.failed = true;
            decoded.error_detail = "Worker exception: unknown";
            std::fprintf(stderr,
                         "[windows] image worker exception for %s: unknown\n",
                         url.c_str());
        }

        if (!decoded.failed && decoded.error_detail.empty() && decoded.url.empty()) {
            decoded.failed = true;
            decoded.url = url;
            decoded.error_detail = "Worker produced an empty image result";
        }
        if (decoded.url.empty())
            decoded.url = url;

        if (decoded.failed)
            std::fflush(stderr);

        {
            std::lock_guard lock(g_images.mutex);
            g_images.completed_jobs.push_back(std::move(decoded));
        }

        auto hwnd = g_ime.hwnd;
        if (hwnd)
            PostMessageW(hwnd, WM_PHENOTYPE_IMAGE_READY, 0, 0);
    }

    if (SUCCEEDED(co_hr))
        CoUninitialize();
    g_images.worker_thread_id = 0;
}

inline void ensure_image_worker() {
    std::lock_guard lock(g_images.mutex);
    if (g_images.worker_started || g_images.queue_only_for_tests)
        return;
    g_images.stop_worker = false;
    g_images.worker = std::thread(image_worker_main);
    g_images.worker_started = true;
}

inline void shutdown_image_worker() {
    std::thread worker;
    {
        std::lock_guard lock(g_images.mutex);
        if (!g_images.worker_started) {
            g_images.stop_worker = true;
            g_images.pending_jobs.clear();
            g_images.pending_head = 0;
            return;
        }
        g_images.stop_worker = true;
        g_images.pending_jobs.clear();
        g_images.pending_head = 0;
        worker = std::move(g_images.worker);
        g_images.worker_started = false;
    }
    g_images.cv.notify_all();
    if (worker.joinable())
        worker.join();
}

inline void queue_remote_image_load(std::string const& url) {
    {
        std::lock_guard lock(g_images.mutex);
        if (!is_remote_image_queued_unlocked(url))
            g_images.pending_jobs.push_back(url);
    }
    ensure_image_worker();
    g_images.cv.notify_one();
}

inline void load_image_entry(std::string const& url) {
    auto image_url = std::string(url);
    if (find_image_entry(image_url))
        return;
    auto remote = is_http_url(image_url);
    auto& entry = ensure_image_entry(image_url);

    if (remote) {
        entry.state = ImageEntryState::pending;
        clear_image_entry(entry);
        queue_remote_image_load(image_url);
        return;
    }

    auto path = resolve_image_path(image_url);
    auto loaded = decode_image_file(path);
    if (!loaded) {
        mark_image_entry_failed(entry, image_url, loaded.error());
        return;
    }

    loaded->url = image_url;
    store_decoded_image(std::move(*loaded));
}

inline void wait_for_fence(UINT64 value) {
    if (!g_renderer.fence || value == 0) return;
    if (g_renderer.fence->GetCompletedValue() >= value) return;
    g_renderer.fence->SetEventOnCompletion(value, g_renderer.fence_event);
    WaitForSingleObject(g_renderer.fence_event, INFINITE);
}

inline void transition_resource(ID3D12Resource* resource,
                                D3D12_RESOURCE_STATES before,
                                D3D12_RESOURCE_STATES after) {
    if (!resource || before == after) return;
    D3D12_RESOURCE_BARRIER barrier{};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = resource;
    barrier.Transition.StateBefore = before;
    barrier.Transition.StateAfter = after;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    g_renderer.command_list->ResourceBarrier(1, &barrier);
}

inline std::pair<void*, UINT64> upload_alloc(UINT64 bytes, UINT64 alignment) {
    auto aligned = align_up(static_cast<UINT64>(g_renderer.upload.offset), alignment);
    if (aligned + bytes > g_renderer.upload.capacity) {
        std::fprintf(stderr, "[dx12] upload buffer exhausted (%llu bytes requested)\n",
                     static_cast<unsigned long long>(bytes));
        return {nullptr, 0};
    }
    g_renderer.upload.offset = static_cast<std::size_t>(aligned + bytes);
    return {g_renderer.upload.mapped + aligned, aligned};
}

inline HRESULT compile_shader(
        char const* entry,
        char const* target,
        ComPtr<ID3DBlob>& blob) {
    UINT flags = D3DCOMPILE_ENABLE_STRICTNESS;
    if (g_renderer.debug_enabled)
        flags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
    ComPtr<ID3DBlob> errors;
    auto hr = D3DCompile(
        HLSL_SHADERS,
        std::strlen(HLSL_SHADERS),
        nullptr,
        nullptr,
        nullptr,
        entry,
        target,
        flags,
        0,
        &blob,
        &errors);
    if (FAILED(hr) && errors) {
        std::fprintf(stderr, "[dx12] shader compile %s/%s: %s\n",
                     entry, target,
                     static_cast<char const*>(errors->GetBufferPointer()));
    }
    return hr;
}

inline HRESULT create_root_signature() {
    D3D12_DESCRIPTOR_RANGE srv_range{};
    srv_range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    srv_range.NumDescriptors = 1;
    srv_range.BaseShaderRegister = 0;
    srv_range.RegisterSpace = 0;
    srv_range.OffsetInDescriptorsFromTableStart =
        D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    D3D12_ROOT_PARAMETER params[2]{};
    params[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
    params[0].Constants.Num32BitValues = 4;
    params[0].Constants.ShaderRegister = 0;
    params[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

    params[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    params[1].DescriptorTable.NumDescriptorRanges = 1;
    params[1].DescriptorTable.pDescriptorRanges = &srv_range;
    params[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    D3D12_STATIC_SAMPLER_DESC samplers[2]{};
    samplers[0].Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    samplers[0].AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    samplers[0].AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    samplers[0].AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    samplers[0].ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;
    samplers[0].ShaderRegister = 0;
    samplers[0].RegisterSpace = 0;
    samplers[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    samplers[0].MaxLOD = D3D12_FLOAT32_MAX;

    samplers[1] = samplers[0];
    samplers[1].Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
    samplers[1].ShaderRegister = 1;

    D3D12_ROOT_SIGNATURE_DESC desc{};
    desc.NumParameters = 2;
    desc.pParameters = params;
    desc.NumStaticSamplers = static_cast<UINT>(std::size(samplers));
    desc.pStaticSamplers = samplers;
    desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    ComPtr<ID3DBlob> serialized;
    ComPtr<ID3DBlob> errors;
    auto hr = D3D12SerializeRootSignature(
        &desc,
        D3D_ROOT_SIGNATURE_VERSION_1,
        &serialized,
        &errors);
    if (FAILED(hr)) {
        if (errors) {
            std::fprintf(stderr, "[dx12] root signature: %s\n",
                         static_cast<char const*>(errors->GetBufferPointer()));
        }
        return hr;
    }
    return g_renderer.device->CreateRootSignature(
        0,
        serialized->GetBufferPointer(),
        serialized->GetBufferSize(),
        IID_PPV_ARGS(&g_renderer.root_signature));
}

inline HRESULT create_pipelines() {
    ComPtr<ID3DBlob> color_vs;
    ComPtr<ID3DBlob> color_ps;
    ComPtr<ID3DBlob> text_vs;
    ComPtr<ID3DBlob> image_ps;
    ComPtr<ID3DBlob> text_ps;
    if (FAILED(compile_shader("vs_color", "vs_5_0", color_vs))) return E_FAIL;
    if (FAILED(compile_shader("fs_color", "ps_5_0", color_ps))) return E_FAIL;
    if (FAILED(compile_shader("vs_text", "vs_5_0", text_vs))) return E_FAIL;
    if (FAILED(compile_shader("fs_image", "ps_5_0", image_ps))) return E_FAIL;
    if (FAILED(compile_shader("fs_text", "ps_5_0", text_ps))) return E_FAIL;

    D3D12_BLEND_DESC blend{};
    blend.AlphaToCoverageEnable = FALSE;
    blend.IndependentBlendEnable = FALSE;
    auto& target = blend.RenderTarget[0];
    target.BlendEnable = TRUE;
    target.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    target.SrcBlend = D3D12_BLEND_SRC_ALPHA;
    target.DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
    target.BlendOp = D3D12_BLEND_OP_ADD;
    target.SrcBlendAlpha = D3D12_BLEND_ONE;
    target.DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
    target.BlendOpAlpha = D3D12_BLEND_OP_ADD;

    D3D12_RASTERIZER_DESC raster{};
    raster.FillMode = D3D12_FILL_MODE_SOLID;
    raster.CullMode = D3D12_CULL_MODE_NONE;
    raster.FrontCounterClockwise = FALSE;
    raster.DepthClipEnable = TRUE;

    D3D12_DEPTH_STENCIL_DESC depth{};
    depth.DepthEnable = FALSE;
    depth.StencilEnable = FALSE;

    D3D12_GRAPHICS_PIPELINE_STATE_DESC base{};
    base.pRootSignature = g_renderer.root_signature.Get();
    base.BlendState = blend;
    base.RasterizerState = raster;
    base.DepthStencilState = depth;
    base.SampleMask = UINT_MAX;
    base.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    base.NumRenderTargets = 1;
    base.RTVFormats[0] = DXGI_FORMAT_B8G8R8A8_UNORM;
    base.SampleDesc.Count = 1;

    D3D12_INPUT_ELEMENT_DESC color_layout[] = {
        {"RECT", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0,
         D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1},
        {"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 16,
         D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1},
        {"PARAMS", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 32,
         D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1},
    };
    auto color_desc = base;
    color_desc.InputLayout = {color_layout, static_cast<UINT>(std::size(color_layout))};
    color_desc.VS = {color_vs->GetBufferPointer(), color_vs->GetBufferSize()};
    color_desc.PS = {color_ps->GetBufferPointer(), color_ps->GetBufferSize()};
    auto hr = g_renderer.device->CreateGraphicsPipelineState(
        &color_desc, IID_PPV_ARGS(&g_renderer.color_pipeline));
    if (FAILED(hr)) return hr;

    D3D12_INPUT_ELEMENT_DESC text_layout[] = {
        {"RECT", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0,
         D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1},
        {"UVRECT", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 16,
         D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1},
    };
    auto image_desc = base;
    image_desc.InputLayout = {text_layout, static_cast<UINT>(std::size(text_layout))};
    image_desc.VS = {text_vs->GetBufferPointer(), text_vs->GetBufferSize()};
    image_desc.PS = {image_ps->GetBufferPointer(), image_ps->GetBufferSize()};
    hr = g_renderer.device->CreateGraphicsPipelineState(
        &image_desc, IID_PPV_ARGS(&g_renderer.image_pipeline));
    if (FAILED(hr)) return hr;

    auto text_desc = image_desc;
    text_desc.PS = {text_ps->GetBufferPointer(), text_ps->GetBufferSize()};
    return g_renderer.device->CreateGraphicsPipelineState(
        &text_desc, IID_PPV_ARGS(&g_renderer.text_pipeline));
}

inline HRESULT create_upload_buffer() {
    // A single frame can upload the full 2048x2048 image atlas (16 MiB)
    // and a large text atlas in the same command list. Keep enough shared
    // upload space for both plus vertex data to avoid runtime exhaustion
    // when remote images become ready mid-scene.
    constexpr UINT64 upload_capacity = 64ull * 1024ull * 1024ull;
    D3D12_HEAP_PROPERTIES heap_props{};
    heap_props.Type = D3D12_HEAP_TYPE_UPLOAD;

    D3D12_RESOURCE_DESC desc{};
    desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    desc.Width = upload_capacity;
    desc.Height = 1;
    desc.DepthOrArraySize = 1;
    desc.MipLevels = 1;
    desc.SampleDesc.Count = 1;
    desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

    auto hr = g_renderer.device->CreateCommittedResource(
        &heap_props,
        D3D12_HEAP_FLAG_NONE,
        &desc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&g_renderer.upload.resource));
    if (FAILED(hr)) return hr;
    set_debug_name(g_renderer.upload.resource, L"phenotype.upload_buffer");

    hr = g_renderer.upload.resource->Map(0, nullptr,
        reinterpret_cast<void**>(&g_renderer.upload.mapped));
    if (FAILED(hr)) return hr;

    g_renderer.upload.capacity = static_cast<std::size_t>(upload_capacity);
    g_renderer.upload.offset = 0;
    return S_OK;
}

inline HRESULT create_frame_resources() {
    D3D12_DESCRIPTOR_HEAP_DESC rtv_desc{};
    rtv_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    rtv_desc.NumDescriptors = RendererState::frame_count;
    auto hr = g_renderer.device->CreateDescriptorHeap(
        &rtv_desc, IID_PPV_ARGS(&g_renderer.rtv_heap));
    if (FAILED(hr)) return hr;
    set_debug_name(g_renderer.rtv_heap, L"phenotype.rtv_heap");

    D3D12_DESCRIPTOR_HEAP_DESC srv_desc{};
    srv_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    srv_desc.NumDescriptors = SRV_SLOT_COUNT;
    srv_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    hr = g_renderer.device->CreateDescriptorHeap(
        &srv_desc, IID_PPV_ARGS(&g_renderer.srv_heap));
    if (FAILED(hr)) return hr;
    set_debug_name(g_renderer.srv_heap, L"phenotype.srv_heap");

    g_renderer.rtv_descriptor_size =
        g_renderer.device->GetDescriptorHandleIncrementSize(
            D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    g_renderer.srv_descriptor_size =
        g_renderer.device->GetDescriptorHandleIncrementSize(
            D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    auto handle = g_renderer.rtv_heap->GetCPUDescriptorHandleForHeapStart();
    for (UINT i = 0; i < RendererState::frame_count; ++i) {
        hr = g_renderer.swap_chain->GetBuffer(
            i, IID_PPV_ARGS(&g_renderer.frames[i].render_target));
        if (FAILED(hr)) return hr;
        g_renderer.frames[i].text_srv_slot = frame_text_srv_slot(i);
        g_renderer.frames[i].rtv_handle = handle;
        g_renderer.device->CreateRenderTargetView(
            g_renderer.frames[i].render_target.Get(),
            nullptr,
            handle);
        auto render_target_name = std::wstring(L"phenotype.backbuffer.frame")
            + std::to_wstring(i);
        set_debug_name(g_renderer.frames[i].render_target, render_target_name.c_str());
        handle.ptr += g_renderer.rtv_descriptor_size;

        hr = g_renderer.device->CreateCommandAllocator(
            D3D12_COMMAND_LIST_TYPE_DIRECT,
            IID_PPV_ARGS(&g_renderer.frames[i].allocator));
        if (FAILED(hr)) return hr;
        auto allocator_name = std::wstring(L"phenotype.command_allocator.frame")
            + std::to_wstring(i);
        set_debug_name(g_renderer.frames[i].allocator, allocator_name.c_str());
    }

    hr = g_renderer.device->CreateCommandList(
        0,
        D3D12_COMMAND_LIST_TYPE_DIRECT,
        g_renderer.frames[0].allocator.Get(),
        nullptr,
        IID_PPV_ARGS(&g_renderer.command_list));
    if (FAILED(hr)) return hr;
    set_debug_name(g_renderer.command_list, L"phenotype.command_list");
    hr = g_renderer.command_list->Close();
    if (FAILED(hr)) return hr;

    hr = g_renderer.device->CreateFence(
        0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&g_renderer.fence));
    if (FAILED(hr)) return hr;
    set_debug_name(g_renderer.fence, L"phenotype.frame_fence");

    g_renderer.fence_event = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    if (!g_renderer.fence_event) return HRESULT_FROM_WIN32(GetLastError());

    return create_upload_buffer();
}

inline HRESULT enable_debug_layers() {
    g_renderer.dred_enabled = should_enable_dred();
    g_renderer.gpu_validation_enabled = false;

    if (g_renderer.debug_enabled) {
        ComPtr<ID3D12Debug1> debug;
        auto hr = D3D12GetDebugInterface(IID_PPV_ARGS(&debug));
        if (FAILED(hr)) {
            log_hresult("D3D12GetDebugInterface(debug)", hr);
        } else {
            debug->EnableDebugLayer();
            debug->SetEnableGPUBasedValidation(TRUE);
            g_renderer.gpu_validation_enabled = true;
        }
    }

    if (g_renderer.dred_enabled) {
        ComPtr<ID3D12DeviceRemovedExtendedDataSettings1> dred;
        if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&dred)))) {
        dred->SetAutoBreadcrumbsEnablement(D3D12_DRED_ENABLEMENT_FORCED_ON);
        dred->SetPageFaultEnablement(D3D12_DRED_ENABLEMENT_FORCED_ON);
        dred->SetBreadcrumbContextEnablement(D3D12_DRED_ENABLEMENT_FORCED_ON);
        } else {
            std::fprintf(stderr, "[dx12] DRED settings interface unavailable\n");
        }
    }

    return S_OK;
}

inline HRESULT create_factory_and_device() {
    UINT factory_flags = g_renderer.debug_enabled ? DXGI_CREATE_FACTORY_DEBUG : 0;
    auto hr = CreateDXGIFactory2(
        factory_flags, IID_PPV_ARGS(&g_renderer.factory));
    if (FAILED(hr)) return hr;

    if (g_renderer.warp_enabled) {
        hr = g_renderer.factory->EnumWarpAdapter(IID_PPV_ARGS(&g_renderer.adapter));
        if (FAILED(hr)) return hr;
    } else {
        for (UINT i = 0;; ++i) {
            ComPtr<IDXGIAdapter1> adapter;
            if (g_renderer.factory->EnumAdapters1(i, &adapter) == DXGI_ERROR_NOT_FOUND)
                break;
            auto desc = describe_adapter(adapter.Get());
            if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) continue;
            if (SUCCEEDED(D3D12CreateDevice(
                    adapter.Get(),
                    D3D_FEATURE_LEVEL_11_0,
                    __uuidof(ID3D12Device),
                    nullptr))) {
                g_renderer.adapter = adapter;
                break;
            }
        }
        if (!g_renderer.adapter) {
            g_renderer.warp_enabled = true;
            hr = g_renderer.factory->EnumWarpAdapter(IID_PPV_ARGS(&g_renderer.adapter));
            if (FAILED(hr)) return hr;
        }
    }

    hr = D3D12CreateDevice(
        g_renderer.adapter.Get(),
        D3D_FEATURE_LEVEL_11_0,
        IID_PPV_ARGS(&g_renderer.device));
    if (FAILED(hr)) return hr;
    set_debug_name(g_renderer.device, L"phenotype.device");

    if (g_renderer.debug_enabled) {
        ComPtr<ID3D12InfoQueue> info_queue;
        if (SUCCEEDED(g_renderer.device.As(&info_queue))) {
            info_queue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, TRUE);
            info_queue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, TRUE);
        }
    }

    D3D12_COMMAND_QUEUE_DESC queue_desc{};
    queue_desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    hr = g_renderer.device->CreateCommandQueue(
        &queue_desc, IID_PPV_ARGS(&g_renderer.queue));
    if (FAILED(hr)) return hr;
    set_debug_name(g_renderer.queue, L"phenotype.graphics_queue");

    return S_OK;
}

inline HRESULT create_swap_chain(GLFWwindow* window) {
    HWND hwnd = glfwGetWin32Window(window);
    if (!hwnd) return E_FAIL;

    int fbw = 0;
    int fbh = 0;
    glfwGetFramebufferSize(window, &fbw, &fbh);
    if (fbw <= 0) fbw = 1;
    if (fbh <= 0) fbh = 1;

    DXGI_SWAP_CHAIN_DESC1 desc{};
    desc.Width = static_cast<UINT>(fbw);
    desc.Height = static_cast<UINT>(fbh);
    desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    desc.BufferCount = RendererState::frame_count;
    desc.SampleDesc.Count = 1;
    desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    desc.Scaling = DXGI_SCALING_STRETCH;
    desc.AlphaMode = DXGI_ALPHA_MODE_IGNORE;

    ComPtr<IDXGISwapChain1> swap1;
    auto hr = g_renderer.factory->CreateSwapChainForHwnd(
        g_renderer.queue.Get(),
        hwnd,
        &desc,
        nullptr,
        nullptr,
        &swap1);
    if (FAILED(hr)) return hr;

    g_renderer.factory->MakeWindowAssociation(hwnd, DXGI_MWA_NO_ALT_ENTER);
    return swap1.As(&g_renderer.swap_chain);
}

inline void release_swap_chain_targets() {
    for (auto& frame : g_renderer.frames) {
        frame.render_target.Reset();
    }
    g_renderer.last_frame_available = false;
    g_renderer.last_render_width = 0;
    g_renderer.last_render_height = 0;
}

inline HRESULT resize_swap_chain() {
    if (!g_renderer.swap_chain || !g_renderer.device || !g_renderer.window)
        return E_FAIL;

    for (auto& frame : g_renderer.frames)
        wait_for_fence(frame.fence_value);

    release_swap_chain_targets();
    int fbw = 0;
    int fbh = 0;
    glfwGetFramebufferSize(g_renderer.window, &fbw, &fbh);
    if (fbw <= 0 || fbh <= 0) return S_OK;

    auto hr = g_renderer.swap_chain->ResizeBuffers(
        RendererState::frame_count,
        static_cast<UINT>(fbw),
        static_cast<UINT>(fbh),
        DXGI_FORMAT_B8G8R8A8_UNORM,
        0);
    if (FAILED(hr))
        return mark_renderer_lost("IDXGISwapChain::ResizeBuffers", hr);

    auto handle = g_renderer.rtv_heap->GetCPUDescriptorHandleForHeapStart();
    for (UINT i = 0; i < RendererState::frame_count; ++i) {
        hr = g_renderer.swap_chain->GetBuffer(
            i, IID_PPV_ARGS(&g_renderer.frames[i].render_target));
        if (FAILED(hr))
            return mark_renderer_lost("IDXGISwapChain::GetBuffer", hr);
        g_renderer.frames[i].rtv_handle = handle;
        g_renderer.device->CreateRenderTargetView(
            g_renderer.frames[i].render_target.Get(),
            nullptr,
            handle);
        auto render_target_name = std::wstring(L"phenotype.backbuffer.frame")
            + std::to_wstring(i);
        set_debug_name(g_renderer.frames[i].render_target, render_target_name.c_str());
        handle.ptr += g_renderer.rtv_descriptor_size;
    }
    return S_OK;
}

inline HRESULT create_atlas_texture(
        TextAtlas const& atlas,
        FrameContext& frame) {
    if (atlas.quads.empty() || atlas.pixels.empty()) return S_OK;

    D3D12_RESOURCE_DESC tex_desc{};
    tex_desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    tex_desc.Width = static_cast<UINT64>(atlas.width);
    tex_desc.Height = static_cast<UINT>(atlas.height);
    tex_desc.DepthOrArraySize = 1;
    tex_desc.MipLevels = 1;
    tex_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    tex_desc.SampleDesc.Count = 1;
    tex_desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;

    D3D12_HEAP_PROPERTIES default_heap{};
    default_heap.Type = D3D12_HEAP_TYPE_DEFAULT;
    auto recreate = true;
    if (frame.atlas_texture) {
        auto current = frame.atlas_texture->GetDesc();
        recreate = current.Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE2D
            || current.Width != tex_desc.Width
            || current.Height != tex_desc.Height
            || current.Format != tex_desc.Format;
    }
    auto hr = S_OK;
    if (recreate) {
        frame.atlas_texture.Reset();
        hr = g_renderer.device->CreateCommittedResource(
            &default_heap,
            D3D12_HEAP_FLAG_NONE,
            &tex_desc,
            D3D12_RESOURCE_STATE_COPY_DEST,
            nullptr,
            IID_PPV_ARGS(&frame.atlas_texture));
        if (FAILED(hr))
            return mark_renderer_lost("CreateCommittedResource(text atlas)", hr);
        auto atlas_name = std::wstring(L"phenotype.text_atlas.frame")
            + std::to_wstring(frame.text_srv_slot - FIRST_TEXT_SRV_SLOT);
        set_debug_name(frame.atlas_texture, atlas_name.c_str());
    } else {
        transition_resource(
            frame.atlas_texture.Get(),
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
            D3D12_RESOURCE_STATE_COPY_DEST);
    }

    UINT64 upload_size = 0;
    D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint{};
    UINT num_rows = 0;
    UINT64 row_size = 0;
    g_renderer.device->GetCopyableFootprints(
        &tex_desc, 0, 1, 0,
        &footprint,
        &num_rows,
        &row_size,
        &upload_size);

    auto [mapped, offset] = upload_alloc(upload_size, D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT);
    if (!mapped)
        return mark_renderer_lost("upload_alloc(text atlas)", E_OUTOFMEMORY);
    for (UINT row = 0; row < num_rows; ++row) {
        auto* dst = static_cast<unsigned char*>(mapped) + row * footprint.Footprint.RowPitch;
        auto const* src = atlas.pixels.data() + static_cast<std::size_t>(row) * atlas.width * 4;
        std::memcpy(dst, src, static_cast<std::size_t>(atlas.width) * 4);
    }

    D3D12_TEXTURE_COPY_LOCATION dst{};
    dst.pResource = frame.atlas_texture.Get();
    dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    dst.SubresourceIndex = 0;

    D3D12_TEXTURE_COPY_LOCATION src{};
    src.pResource = g_renderer.upload.resource.Get();
    src.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
    src.PlacedFootprint = footprint;
    src.PlacedFootprint.Offset = offset;

    g_renderer.command_list->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);
    transition_resource(
        frame.atlas_texture.Get(),
        D3D12_RESOURCE_STATE_COPY_DEST,
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

    D3D12_SHADER_RESOURCE_VIEW_DESC srv{};
    srv.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    srv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srv.Texture2D.MipLevels = 1;
    g_renderer.device->CreateShaderResourceView(
        frame.atlas_texture.Get(),
        &srv,
        srv_cpu_handle(frame.text_srv_slot));
    return S_OK;
}

inline HRESULT ensure_image_atlas_texture() {
    if (g_images.texture)
        return S_OK;
    constexpr std::size_t atlas_bytes =
        static_cast<std::size_t>(ImageAtlasState::atlas_size)
        * ImageAtlasState::atlas_size * 4;
    if (g_images.pixels.empty()) {
        g_images.pixels.resize(atlas_bytes, 0);
    }

    D3D12_RESOURCE_DESC tex_desc{};
    tex_desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    tex_desc.Width = static_cast<UINT64>(ImageAtlasState::atlas_size);
    tex_desc.Height = static_cast<UINT>(ImageAtlasState::atlas_size);
    tex_desc.DepthOrArraySize = 1;
    tex_desc.MipLevels = 1;
    tex_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    tex_desc.SampleDesc.Count = 1;
    tex_desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;

    D3D12_HEAP_PROPERTIES default_heap{};
    default_heap.Type = D3D12_HEAP_TYPE_DEFAULT;
    auto hr = g_renderer.device->CreateCommittedResource(
        &default_heap,
        D3D12_HEAP_FLAG_NONE,
        &tex_desc,
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
        nullptr,
        IID_PPV_ARGS(&g_images.texture));
    if (FAILED(hr))
        return hr;
    set_debug_name(g_images.texture, L"phenotype.image_atlas");

    D3D12_SHADER_RESOURCE_VIEW_DESC srv{};
    srv.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    srv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srv.Texture2D.MipLevels = 1;
    g_renderer.device->CreateShaderResourceView(
        g_images.texture.Get(),
        &srv,
        srv_cpu_handle(IMAGE_SRV_SLOT));
    return S_OK;
}

inline HRESULT upload_image_atlas() {
    if (!g_images.atlas_dirty)
        return S_OK;
    auto hr = ensure_image_atlas_texture();
    if (FAILED(hr))
        return hr;

    D3D12_RESOURCE_DESC tex_desc{};
    tex_desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    tex_desc.Width = static_cast<UINT64>(ImageAtlasState::atlas_size);
    tex_desc.Height = static_cast<UINT>(ImageAtlasState::atlas_size);
    tex_desc.DepthOrArraySize = 1;
    tex_desc.MipLevels = 1;
    tex_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    tex_desc.SampleDesc.Count = 1;
    tex_desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;

    UINT64 upload_size = 0;
    D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint{};
    UINT num_rows = 0;
    UINT64 row_size = 0;
    g_renderer.device->GetCopyableFootprints(
        &tex_desc, 0, 1, 0,
        &footprint,
        &num_rows,
        &row_size,
        &upload_size);

    auto [mapped, offset] = upload_alloc(
        upload_size,
        D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT);
    if (!mapped)
        return mark_renderer_lost("upload_alloc(image atlas)", E_OUTOFMEMORY);

    for (UINT row = 0; row < num_rows; ++row) {
        auto* dst = static_cast<unsigned char*>(mapped)
            + row * footprint.Footprint.RowPitch;
        auto const* src = g_images.pixels.data()
            + static_cast<std::size_t>(row) * ImageAtlasState::atlas_size * 4;
        std::memcpy(
            dst,
            src,
            static_cast<std::size_t>(ImageAtlasState::atlas_size) * 4);
    }

    D3D12_TEXTURE_COPY_LOCATION dst{};
    dst.pResource = g_images.texture.Get();
    dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    dst.SubresourceIndex = 0;

    D3D12_TEXTURE_COPY_LOCATION src{};
    src.pResource = g_renderer.upload.resource.Get();
    src.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
    src.PlacedFootprint = footprint;
    src.PlacedFootprint.Offset = offset;

    transition_resource(
        g_images.texture.Get(),
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
        D3D12_RESOURCE_STATE_COPY_DEST);
    g_renderer.command_list->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);
    transition_resource(
        g_images.texture.Get(),
        D3D12_RESOURCE_STATE_COPY_DEST,
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    metrics::inst::native_texture_upload_bytes.add(
        static_cast<std::uint64_t>(upload_size),
        native_platform_attrs());
    g_images.atlas_dirty = false;
    return S_OK;
}

inline void begin_frame(FrameContext& frame) {
    // The upload heap is shared across all frames. Reusing it before every
    // in-flight copy finishes can race WARP's background copy worker.
    wait_for_all_frames();
    auto hr = frame.allocator->Reset();
    if (FAILED(hr)) {
        (void)mark_renderer_lost("ID3D12CommandAllocator::Reset", hr);
        return;
    }
    hr = g_renderer.command_list->Reset(frame.allocator.Get(), nullptr);
    if (FAILED(hr)) {
        (void)mark_renderer_lost("ID3D12GraphicsCommandList::Reset", hr);
        return;
    }
    g_renderer.upload.offset = 0;
}

inline void end_frame(FrameContext& frame) {
    auto hr = g_renderer.command_list->Close();
    g_renderer.last_close_hr = hr;
    if (FAILED(hr)) {
        (void)mark_renderer_lost("ID3D12GraphicsCommandList::Close", hr);
        return;
    }
    ID3D12CommandList* lists[] = {g_renderer.command_list.Get()};
    g_renderer.queue->ExecuteCommandLists(1, lists);
    hr = g_renderer.swap_chain->Present(1, 0);
    g_renderer.last_present_hr = hr;
    if (FAILED(hr)) {
        (void)mark_renderer_lost("IDXGISwapChain::Present", hr);
        return;
    }
    frame.fence_value = g_renderer.next_fence_value++;
    hr = g_renderer.queue->Signal(g_renderer.fence.Get(), frame.fence_value);
    g_renderer.last_signal_hr = hr;
    if (FAILED(hr)) {
        (void)mark_renderer_lost("ID3D12CommandQueue::Signal", hr);
    }
}

inline void renderer_init(native_surface_handle handle) {
    install_process_diagnostics();
    if (g_renderer.initialized) return;
    auto* window = static_cast<GLFWwindow*>(handle);
    g_renderer.window = window;
    g_renderer.debug_preset_enabled = env_enabled("PHENOTYPE_WINDOWS_DEBUG");
    g_renderer.debug_enabled = g_renderer.debug_preset_enabled
        || env_enabled("PHENOTYPE_DX12_DEBUG");
    g_renderer.warp_enabled = env_enabled("PHENOTYPE_DX12_WARP");
    if (g_renderer.debug_preset_enabled
        && static_cast<int>(::phenotype::log::current_level())
            > static_cast<int>(::phenotype::log::Severity::debug)) {
        ::phenotype::log::set_level(::phenotype::log::Severity::debug);
    }

    auto co_hr = CoInitializeEx(
        nullptr,
        COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    if (SUCCEEDED(co_hr)) {
        g_renderer.com_initialized = true;
    } else if (co_hr != RPC_E_CHANGED_MODE) {
        std::fprintf(stderr, "[windows] CoInitializeEx failed (hr=0x%08lx)\n",
                     static_cast<unsigned long>(co_hr));
    }

    enable_debug_layers();
    if (FAILED(create_factory_and_device())
        || FAILED(create_swap_chain(window))
        || FAILED(create_root_signature())
        || FAILED(create_pipelines())
        || FAILED(create_frame_resources())
        || FAILED(ensure_image_atlas_texture())) {
        std::fprintf(stderr, "[dx12] renderer initialization failed\n");
        return;
    }

    auto desc = describe_adapter(g_renderer.adapter.Get());
    std::printf("[phenotype-native] Direct3D 12 initialized (%ls%s)\n",
                desc.Description,
                g_renderer.warp_enabled ? ", WARP" : "");
    g_renderer.initialized = true;
}

inline void renderer_flush(unsigned char const* buf, unsigned int len) {
    if (len == 0 || !g_renderer.initialized || g_renderer.lost) return;

    int fbw = 0;
    int fbh = 0;
    glfwGetFramebufferSize(g_renderer.window, &fbw, &fbh);
    if (fbw <= 0 || fbh <= 0) return;

    auto current_index = g_renderer.swap_chain->GetCurrentBackBufferIndex();
    auto& frame = g_renderer.frames[current_index];
    if (frame.render_target == nullptr) {
        if (FAILED(resize_swap_chain())) return;
    } else {
        auto desc = frame.render_target->GetDesc();
        if (static_cast<int>(desc.Width) != fbw || static_cast<int>(desc.Height) != fbh) {
            if (FAILED(resize_swap_chain())) return;
        }
    }
    if (frame.render_target == nullptr) {
        if (FAILED(resize_swap_chain())) return;
    }

        float text_scale = glfw_backing_scale(g_renderer.window);
    float line_height_ratio = ::phenotype::detail::g_app.theme.line_height_ratio;
    DecodedFrame decoded;
    if (!decode_frame_commands(buf, len, line_height_ratio, decoded)) {
        if (g_renderer.debug_enabled) {
            std::fprintf(stderr,
                         "[phenotype-native] dropped invalid draw command stream (%u bytes)\n",
                         len);
        }
        return;
    }
    suppress_focused_input_base_text_for_composition(decoded);
    (void)process_completed_images();

    for (auto const& image : decoded.images) {
        auto* entry = find_image_entry(image.url);
        if (!entry) {
            load_image_entry(image.url);
            entry = find_image_entry(image.url);
        }
        if (entry && entry->state == ImageEntryState::ready) {
            append_textured_quad(
                decoded.image_data,
                image.x, image.y, image.w, image.h,
                entry->u, entry->v, entry->uw, entry->vh);
        } else {
            bool failed = entry != nullptr
                && entry->state == ImageEntryState::failed;
            float fill = failed ? 0.90f : 0.94f;
            float edge = failed ? 0.78f : 0.82f;
            append_color_instance(
                decoded.color_data,
                image.x, image.y, image.w, image.h,
                fill, fill, fill, 1.0f,
                6.0f, 0.0f, 2.0f);
            append_color_instance(
                decoded.color_data,
                image.x, image.y, image.w, image.h,
                edge, edge, edge, 1.0f,
                0.0f, 1.0f, 1.0f);
        }
    }

    auto append_overlay_text = [&](float x, float y, float font_size, bool mono,
                                   Color color, std::string text, float line_height) {
        if (text.empty())
            return;
        decoded.text_entries.push_back({
            x,
            y,
            font_size,
            mono,
            color.r / 255.0f,
            color.g / 255.0f,
            color.b / 255.0f,
            color.a / 255.0f,
            std::move(text),
            line_height,
        });
    };

    auto layout = current_windows_caret_layout();
    if (layout.valid) {
        auto const& snapshot = layout.snapshot;
        auto composition = current_composition_visual_state(snapshot);
        if (composition.valid) {
            for (auto const& entry : composition_overlay_text_entries()) {
                append_overlay_text(
                    entry.x,
                    entry.y,
                    entry.font_size,
                    entry.mono,
                    {
                        static_cast<unsigned char>(std::lround(entry.r * 255.0f)),
                        static_cast<unsigned char>(std::lround(entry.g * 255.0f)),
                        static_cast<unsigned char>(std::lround(entry.b * 255.0f)),
                        static_cast<unsigned char>(std::lround(entry.a * 255.0f)),
                    },
                    entry.text,
                    entry.line_height);
            }

            if (composition.underline_width > 0.0f) {
                append_color_instance(
                    decoded.overlay_color_data,
                    composition.underline_x,
                    composition.text_y + composition.snapshot.line_height - 2.0f,
                    composition.underline_width,
                    1.5f,
                    composition.snapshot.accent.r / 255.0f,
                    composition.snapshot.accent.g / 255.0f,
                    composition.snapshot.accent.b / 255.0f,
                    1.0f);
            }

            append_color_instance(
                decoded.overlay_color_data,
                composition.caret_x,
                composition.text_y + 2.0f,
                1.5f,
                (composition.snapshot.line_height > 4.0f)
                    ? (composition.snapshot.line_height - 4.0f)
                    : composition.snapshot.line_height,
                composition.snapshot.accent.r / 255.0f,
                composition.snapshot.accent.g / 255.0f,
                composition.snapshot.accent.b / 255.0f,
                1.0f);
        } else if (snapshot.caret_visible) {
            append_color_instance(
                decoded.overlay_color_data,
                layout.draw_x,
                layout.draw_y + 2.0f,
                1.5f,
                (snapshot.line_height > 4.0f) ? (snapshot.line_height - 4.0f) : snapshot.line_height,
                snapshot.accent.r / 255.0f,
                snapshot.accent.g / 255.0f,
                snapshot.accent.b / 255.0f,
                1.0f);
        }

        if (g_ime.overlay.visible) {
            append_color_instance(
                decoded.color_data,
                g_ime.overlay.x, g_ime.overlay.y, g_ime.overlay.w, g_ime.overlay.h,
                1.0f, 1.0f, 1.0f, 0.98f,
                8.0f, 0.0f, 2.0f);
            append_color_instance(
                decoded.color_data,
                g_ime.overlay.x, g_ime.overlay.y, g_ime.overlay.w, g_ime.overlay.h,
                0.80f, 0.84f, 0.90f, 1.0f,
                0.0f, 1.0f, 1.0f);

            for (auto const& hit : g_ime.overlay.hits) {
                if (hit.kind == CandidateHitKind::item) {
                    bool selected = hit.index == g_ime.candidate_selection;
                    bool hovered = static_cast<int>(hit.index) == g_ime.hovered_candidate;
                    if (selected || hovered) {
                        float alpha = selected ? 0.22f : 0.12f;
                        append_color_instance(
                            decoded.color_data,
                            hit.x + 2.0f, hit.y + 1.0f, hit.w - 4.0f, hit.h - 2.0f,
                            snapshot.accent.r / 255.0f,
                            snapshot.accent.g / 255.0f,
                            snapshot.accent.b / 255.0f,
                            alpha,
                            6.0f, 0.0f, 2.0f);
                    }
                    if (hit.index < g_ime.candidates.size()) {
                        append_overlay_text(
                            hit.x + 10.0f,
                            hit.y + (hit.h - snapshot.line_height) * 0.5f,
                            snapshot.font_size,
                            snapshot.mono,
                            snapshot.foreground,
                            cppx::unicode::wide_to_utf8(g_ime.candidates[hit.index])
                                .value_or(std::string{}),
                            snapshot.line_height);
                    }
                } else if (hit.kind == CandidateHitKind::prev_page
                           || hit.kind == CandidateHitKind::next_page) {
                    bool hovered = g_ime.hovered_kind == hit.kind;
                    float fill = hovered ? 0.90f : 0.95f;
                    append_color_instance(
                        decoded.color_data,
                        hit.x, hit.y, hit.w, hit.h,
                        fill, fill, fill, 1.0f,
                        6.0f, 0.0f, 2.0f);
                    append_color_instance(
                        decoded.color_data,
                        hit.x, hit.y, hit.w, hit.h,
                        0.82f, 0.82f, 0.82f, 1.0f,
                        0.0f, 1.0f, 1.0f);
                    std::string label = (hit.kind == CandidateHitKind::prev_page)
                        ? "Prev" : "Next";
                    float label_w = text_measure(
                        snapshot.font_size * 0.92f,
                        false,
                        label.c_str(),
                        static_cast<unsigned int>(label.size()));
                    append_overlay_text(
                        hit.x + (hit.w - label_w) * 0.5f,
                        hit.y + (hit.h - snapshot.line_height * 0.92f) * 0.5f,
                        snapshot.font_size * 0.92f,
                        false,
                        snapshot.foreground,
                        label,
                        snapshot.line_height * 0.92f);
                }
            }
        }
    }

    begin_frame(frame);
    if (g_renderer.lost)
        return;
    auto upload_hr = upload_image_atlas();
    if (FAILED(upload_hr)) {
        (void)mark_renderer_lost("upload_image_atlas", upload_hr);
        return;
    }
    transition_resource(
        frame.render_target.Get(),
        D3D12_RESOURCE_STATE_PRESENT,
        D3D12_RESOURCE_STATE_RENDER_TARGET);

    D3D12_VIEWPORT viewport{};
    viewport.Width = static_cast<float>(fbw);
    viewport.Height = static_cast<float>(fbh);
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;
    D3D12_RECT scissor{0, 0, fbw, fbh};

    g_renderer.command_list->SetGraphicsRootSignature(g_renderer.root_signature.Get());
    g_renderer.command_list->RSSetViewports(1, &viewport);
    g_renderer.command_list->RSSetScissorRects(1, &scissor);
    g_renderer.command_list->OMSetRenderTargets(1, &frame.rtv_handle, FALSE, nullptr);
    float clear_color[4]{
        static_cast<float>(decoded.clear_r),
        static_cast<float>(decoded.clear_g),
        static_cast<float>(decoded.clear_b),
        static_cast<float>(decoded.clear_a),
    };
    g_renderer.command_list->ClearRenderTargetView(frame.rtv_handle, clear_color, 0, nullptr);

    int winw = 0;
    int winh = 0;
    glfwGetWindowSize(g_renderer.window, &winw, &winh);
    float uniforms[4]{
        static_cast<float>((winw > 0) ? winw : fbw),
        static_cast<float>((winh > 0) ? winh : fbh),
        0, 0,
    };
    g_renderer.command_list->SetGraphicsRoot32BitConstants(0, 4, uniforms, 0);
    g_renderer.command_list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    UINT color_count = static_cast<UINT>(decoded.color_data.size() / 12);
    if (color_count > 0) {
        UINT64 bytes = static_cast<UINT64>(decoded.color_data.size() * sizeof(float));
        auto [mapped, offset] = upload_alloc(bytes, 16);
        if (mapped) {
            std::memcpy(mapped, decoded.color_data.data(), static_cast<std::size_t>(bytes));
            D3D12_VERTEX_BUFFER_VIEW vbv{};
            vbv.BufferLocation = g_renderer.upload.resource->GetGPUVirtualAddress() + offset;
            vbv.SizeInBytes = static_cast<UINT>(bytes);
            vbv.StrideInBytes = 48;
            g_renderer.command_list->SetPipelineState(g_renderer.color_pipeline.Get());
            g_renderer.command_list->IASetVertexBuffers(0, 1, &vbv);
            g_renderer.command_list->DrawInstanced(6, color_count, 0, 0);
        }
    }

    if (!decoded.image_data.empty() && g_images.texture) {
        UINT64 bytes = static_cast<UINT64>(decoded.image_data.size() * sizeof(float));
        auto [mapped, offset] = upload_alloc(bytes, 16);
        if (mapped) {
            std::memcpy(mapped, decoded.image_data.data(), static_cast<std::size_t>(bytes));
            D3D12_VERTEX_BUFFER_VIEW vbv{};
            vbv.BufferLocation = g_renderer.upload.resource->GetGPUVirtualAddress() + offset;
            vbv.SizeInBytes = static_cast<UINT>(bytes);
            vbv.StrideInBytes = 32;
            ID3D12DescriptorHeap* heaps[] = {g_renderer.srv_heap.Get()};
            g_renderer.command_list->SetDescriptorHeaps(1, heaps);
            g_renderer.command_list->SetGraphicsRootDescriptorTable(
                1, srv_gpu_handle(IMAGE_SRV_SLOT));
            g_renderer.command_list->SetPipelineState(g_renderer.image_pipeline.Get());
            g_renderer.command_list->IASetVertexBuffers(0, 1, &vbv);
            g_renderer.command_list->DrawInstanced(
                6, static_cast<UINT>(decoded.image_data.size() / 8), 0, 0);
        }
    }

    if (!decoded.text_entries.empty() && g_text.initialized) {
        auto atlas = text_build_atlas(decoded.text_entries, text_scale);
        if (!atlas.quads.empty() && !atlas.pixels.empty()
            && SUCCEEDED(create_atlas_texture(atlas, frame))) {
            std::vector<float> text_data;
            text_data.reserve(atlas.quads.size() * 8);
            for (auto const& quad : atlas.quads) {
                append_textured_quad(
                    text_data,
                    quad.x, quad.y, quad.w, quad.h,
                    quad.u, quad.v, quad.uw, quad.vh);
            }

            UINT64 bytes = static_cast<UINT64>(text_data.size() * sizeof(float));
            auto [mapped, offset] = upload_alloc(bytes, 16);
            if (mapped) {
                std::memcpy(mapped, text_data.data(), static_cast<std::size_t>(bytes));
                D3D12_VERTEX_BUFFER_VIEW vbv{};
                vbv.BufferLocation = g_renderer.upload.resource->GetGPUVirtualAddress() + offset;
                vbv.SizeInBytes = static_cast<UINT>(bytes);
                vbv.StrideInBytes = 32;
                ID3D12DescriptorHeap* heaps[] = {g_renderer.srv_heap.Get()};
                g_renderer.command_list->SetDescriptorHeaps(1, heaps);
                g_renderer.command_list->SetGraphicsRootDescriptorTable(
                    1, srv_gpu_handle(frame.text_srv_slot));
                g_renderer.command_list->SetPipelineState(g_renderer.text_pipeline.Get());
                g_renderer.command_list->IASetVertexBuffers(0, 1, &vbv);
                g_renderer.command_list->DrawInstanced(
                    6, static_cast<UINT>(atlas.quads.size()), 0, 0);
            }
        }
    }

    UINT overlay_color_count = static_cast<UINT>(decoded.overlay_color_data.size() / 12);
    if (overlay_color_count > 0) {
        UINT64 bytes = static_cast<UINT64>(decoded.overlay_color_data.size() * sizeof(float));
        auto [mapped, offset] = upload_alloc(bytes, 16);
        if (mapped) {
            std::memcpy(mapped, decoded.overlay_color_data.data(), static_cast<std::size_t>(bytes));
            D3D12_VERTEX_BUFFER_VIEW vbv{};
            vbv.BufferLocation = g_renderer.upload.resource->GetGPUVirtualAddress() + offset;
            vbv.SizeInBytes = static_cast<UINT>(bytes);
            vbv.StrideInBytes = 48;
            g_renderer.command_list->SetPipelineState(g_renderer.color_pipeline.Get());
            g_renderer.command_list->IASetVertexBuffers(0, 1, &vbv);
            g_renderer.command_list->DrawInstanced(6, overlay_color_count, 0, 0);
        }
    }

    transition_resource(
        frame.render_target.Get(),
        D3D12_RESOURCE_STATE_RENDER_TARGET,
        D3D12_RESOURCE_STATE_PRESENT);
    end_frame(frame);
    if (g_renderer.lost)
        return;
    g_renderer.last_presented_frame_index = current_index;
    g_renderer.last_render_width = static_cast<UINT>(fbw);
    g_renderer.last_render_height = static_cast<UINT>(fbh);
    g_renderer.last_frame_available = true;
    g_renderer.hit_regions.swap(decoded.hit_regions);
}

inline void renderer_shutdown() {
    wait_for_all_frames();
    shutdown_image_worker();
    g_images.texture.Reset();
    if (!g_images.pixels.empty()) {
        std::fill(
            g_images.pixels.begin(),
            g_images.pixels.end(),
            static_cast<unsigned char>(0));
    }
    std::vector<CachedImageRecord>().swap(g_images.cache);
    std::vector<std::string>().swap(g_images.pending_jobs);
    g_images.pending_head = 0;
    std::vector<DecodedImage>().swap(g_images.completed_jobs);
    g_images.atlas_dirty = false;
    g_images.worker_started = false;
    g_images.queue_only_for_tests = false;
    g_images.stop_worker = false;
    g_images.cursor_x = 0;
    g_images.cursor_y = 0;
    g_images.row_height = 0;
    g_ime.overlay = {};
    if (g_renderer.upload.resource && g_renderer.upload.mapped) {
        g_renderer.upload.resource->Unmap(0, nullptr);
    }
    if (g_renderer.fence_event) {
        CloseHandle(g_renderer.fence_event);
        g_renderer.fence_event = nullptr;
    }
    if (g_renderer.com_initialized)
        CoUninitialize();
    g_renderer = {};
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

inline float windows_scroll_delta_y(double dy,
                                    float line_height,
                                    float viewport_height) {
    if (dy == 0.0) return 0.0f;

    if (line_height <= 0.0f)
        line_height = 1.0f;

    UINT lines = 3;
    if (!SystemParametersInfoW(SPI_GETWHEELSCROLLLINES, 0, &lines, 0))
        lines = 3;

    if (lines == 0u)
        return 0.0f;

    if (lines == WHEEL_PAGESCROLL) {
        float page = viewport_height - line_height;
        if (page < line_height)
            page = line_height;
        return static_cast<float>(dy) * page;
    }

    return static_cast<float>(dy) * static_cast<float>(lines) * line_height;
}

inline bool windows_uses_shared_caret_blink() {
    return true;
}

inline ::phenotype::diag::PlatformCapabilitiesSnapshot windows_debug_capabilities() {
    return {
        "windows",
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
    };
}

inline json::Object windows_renderer_runtime_json() {
    json::Object renderer;
    renderer.emplace("initialized", json::Value{g_renderer.initialized});
    renderer.emplace("preset_enabled", json::Value{g_renderer.debug_preset_enabled});
    renderer.emplace("debug_enabled", json::Value{g_renderer.debug_enabled});
    renderer.emplace(
        "gpu_based_validation_enabled",
        json::Value{g_renderer.gpu_validation_enabled});
    renderer.emplace("warp_enabled", json::Value{g_renderer.warp_enabled});
    renderer.emplace("dred_enabled", json::Value{g_renderer.dred_enabled});
    renderer.emplace("lost", json::Value{g_renderer.lost});
    renderer.emplace("last_frame_available", json::Value{g_renderer.last_frame_available});
    renderer.emplace(
        "last_presented_frame_index",
        json::Value{static_cast<std::int64_t>(g_renderer.last_presented_frame_index)});
    renderer.emplace(
        "last_render_width",
        json::Value{static_cast<std::int64_t>(g_renderer.last_render_width)});
    renderer.emplace(
        "last_render_height",
        json::Value{static_cast<std::int64_t>(g_renderer.last_render_height)});
    renderer.emplace(
        "last_failure_hr",
        json::Value{static_cast<std::int64_t>(g_renderer.last_failure_hr)});
    renderer.emplace(
        "device_removed_reason",
        json::Value{static_cast<std::int64_t>(g_renderer.device_removed_reason)});
    renderer.emplace(
        "last_close_hr",
        json::Value{static_cast<std::int64_t>(g_renderer.last_close_hr)});
    renderer.emplace(
        "last_present_hr",
        json::Value{static_cast<std::int64_t>(g_renderer.last_present_hr)});
    renderer.emplace(
        "last_signal_hr",
        json::Value{static_cast<std::int64_t>(g_renderer.last_signal_hr)});
    renderer.emplace(
        "dred_device_state",
        json::Value{static_cast<std::int64_t>(g_renderer.dred_device_state)});
    renderer.emplace("failure_label", json::Value{g_renderer.last_failure_label});
    return renderer;
}

inline json::Object windows_ime_runtime_json() {
    json::Object ime;
    ime.emplace("hwnd_attached", json::Value{g_ime.hwnd != nullptr});
    ime.emplace("composition_active", json::Value{g_ime.composition_active});
    ime.emplace(
        "composition_text",
        json::Value{
            cppx::unicode::wide_to_utf8(g_ime.composition_text).value_or(std::string{})});
    ime.emplace(
        "composition_cursor",
        json::Value{static_cast<std::int64_t>(g_ime.composition_cursor)});
    ime.emplace(
        "composition_anchor",
        json::Value{static_cast<std::int64_t>(g_ime.composition_anchor)});
    ime.emplace("overlay_visible", json::Value{g_ime.overlay.visible});
    ime.emplace(
        "sync_call_count",
        json::Value{static_cast<std::int64_t>(g_ime.sync_call_count)});
    ime.emplace(
        "repaint_request_count",
        json::Value{static_cast<std::int64_t>(g_ime.repaint_request_count)});
    ime.emplace(
        "deferred_repaint_count",
        json::Value{static_cast<std::int64_t>(g_ime.deferred_repaint_count)});
    ime.emplace("repaint_pending", json::Value{g_ime.repaint_pending});
    return ime;
}

inline json::Object windows_images_runtime_json() {
    std::size_t pending_jobs = 0;
    std::size_t completed_jobs = 0;
    bool worker_started = false;
    {
        std::lock_guard lock(g_images.mutex);
        pending_jobs = pending_job_count_unlocked();
        completed_jobs = g_images.completed_jobs.size();
        worker_started = g_images.worker_started;
    }

    json::Array remote_entries;
    for (auto const& record : g_images.cache) {
        if (!is_http_url(record.url))
            continue;
        json::Object entry;
        entry.emplace("url", json::Value{record.url});
        entry.emplace(
            "state",
            json::Value{image_entry_state_name(record.entry.state)});
        entry.emplace(
            "failure_reason",
            json::Value{record.entry.failure_reason});
        remote_entries.push_back(json::Value{std::move(entry)});
    }

    json::Object images;
    images.emplace(
        "pending_queue_count",
        json::Value{static_cast<std::int64_t>(pending_jobs)});
    images.emplace(
        "completed_queue_count",
        json::Value{static_cast<std::int64_t>(completed_jobs)});
    images.emplace("worker_started", json::Value{worker_started});
    images.emplace(
        "remote_entry_count",
        json::Value{static_cast<std::int64_t>(remote_entries.size())});
    images.emplace("remote_entries", json::Value{std::move(remote_entries)});
    return images;
}

inline json::Value windows_platform_runtime_details_json_with_reason(
        std::string_view artifact_reason) {
#ifdef _WIN32
    json::Object runtime;
    runtime.emplace("renderer", json::Value{windows_renderer_runtime_json()});
    runtime.emplace("ime", json::Value{windows_ime_runtime_json()});
    runtime.emplace("images", json::Value{windows_images_runtime_json()});
    if (!artifact_reason.empty()) {
        runtime.emplace(
            "artifact_reason",
            json::Value{std::string(artifact_reason)});
    }
    return json::Value{std::move(runtime)};
#else
    (void)artifact_reason;
    return json::Value{json::Object{}};
#endif
}

inline json::Value windows_platform_runtime_details_json() {
    return windows_platform_runtime_details_json_with_reason({});
}

inline std::string windows_snapshot_json() {
    return ::phenotype::detail::serialize_diag_snapshot_with_debug(
        windows_debug_capabilities(),
        windows_platform_runtime_details_json());
}

inline std::optional<DebugFrameCapture> windows_capture_frame_rgba() {
#ifdef _WIN32
    if (!g_renderer.initialized || g_renderer.lost || !g_renderer.last_frame_available)
        return std::nullopt;
    if (!g_renderer.device || !g_renderer.queue || !g_renderer.command_list || !g_renderer.fence)
        return std::nullopt;

    auto frame_index = g_renderer.last_presented_frame_index;
    if (frame_index >= RendererState::frame_count)
        return std::nullopt;

    auto& frame = g_renderer.frames[frame_index];
    if (!frame.render_target || !frame.allocator)
        return std::nullopt;

    auto desc = frame.render_target->GetDesc();
    if (desc.Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE2D
        || desc.Width == 0
        || desc.Height == 0
        || desc.Format != DXGI_FORMAT_B8G8R8A8_UNORM) {
        return std::nullopt;
    }

    wait_for_all_frames();

    D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint{};
    UINT num_rows = 0;
    UINT64 row_size = 0;
    UINT64 total_size = 0;
    g_renderer.device->GetCopyableFootprints(
        &desc,
        0,
        1,
        0,
        &footprint,
        &num_rows,
        &row_size,
        &total_size);
    auto const packed_row_size =
        static_cast<UINT64>(desc.Width) * static_cast<UINT64>(sizeof(std::uint32_t));
    if (total_size == 0 || num_rows < desc.Height || row_size < packed_row_size)
        return std::nullopt;
    if (FAILED(ensure_readback_buffer(total_size)))
        return std::nullopt;

    auto hr = frame.allocator->Reset();
    if (FAILED(hr)) {
        (void)mark_renderer_lost("ID3D12CommandAllocator::Reset(frame capture)", hr);
        return std::nullopt;
    }
    hr = g_renderer.command_list->Reset(frame.allocator.Get(), nullptr);
    if (FAILED(hr)) {
        (void)mark_renderer_lost("ID3D12GraphicsCommandList::Reset(frame capture)", hr);
        return std::nullopt;
    }

    transition_resource(
        frame.render_target.Get(),
        D3D12_RESOURCE_STATE_PRESENT,
        D3D12_RESOURCE_STATE_COPY_SOURCE);

    D3D12_TEXTURE_COPY_LOCATION dst{};
    dst.pResource = g_renderer.readback.resource.Get();
    dst.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
    dst.PlacedFootprint = footprint;

    D3D12_TEXTURE_COPY_LOCATION src{};
    src.pResource = frame.render_target.Get();
    src.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    src.SubresourceIndex = 0;

    g_renderer.command_list->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);
    transition_resource(
        frame.render_target.Get(),
        D3D12_RESOURCE_STATE_COPY_SOURCE,
        D3D12_RESOURCE_STATE_PRESENT);

    hr = g_renderer.command_list->Close();
    g_renderer.last_close_hr = hr;
    if (FAILED(hr)) {
        (void)mark_renderer_lost("ID3D12GraphicsCommandList::Close(frame capture)", hr);
        return std::nullopt;
    }

    ID3D12CommandList* lists[] = {g_renderer.command_list.Get()};
    g_renderer.queue->ExecuteCommandLists(1, lists);

    auto capture_fence_value = g_renderer.next_fence_value++;
    hr = g_renderer.queue->Signal(g_renderer.fence.Get(), capture_fence_value);
    g_renderer.last_signal_hr = hr;
    if (FAILED(hr)) {
        (void)mark_renderer_lost("ID3D12CommandQueue::Signal(frame capture)", hr);
        return std::nullopt;
    }
    frame.fence_value = capture_fence_value;
    wait_for_fence(capture_fence_value);

    D3D12_RANGE read_range{
        0,
        static_cast<SIZE_T>(total_size),
    };
    std::uint8_t* mapped = nullptr;
    hr = g_renderer.readback.resource->Map(
        0,
        &read_range,
        reinterpret_cast<void**>(&mapped));
    if (FAILED(hr)) {
        (void)mark_renderer_lost("ID3D12Resource::Map(frame readback)", hr);
        return std::nullopt;
    }

    DebugFrameCapture capture{};
    capture.width = static_cast<unsigned int>(desc.Width);
    capture.height = desc.Height;
    capture.rgba.resize(
        static_cast<std::size_t>(capture.width)
        * static_cast<std::size_t>(capture.height) * 4u);

    for (UINT y = 0; y < capture.height; ++y) {
        auto const* src_row =
            mapped + footprint.Offset
            + static_cast<std::size_t>(y) * footprint.Footprint.RowPitch;
        auto* dst_row =
            capture.rgba.data()
            + static_cast<std::size_t>(y) * static_cast<std::size_t>(capture.width) * 4u;
        for (UINT x = 0; x < capture.width; ++x) {
            auto const pixel_offset = static_cast<std::size_t>(x) * 4u;
            dst_row[pixel_offset + 0] = src_row[pixel_offset + 2];
            dst_row[pixel_offset + 1] = src_row[pixel_offset + 1];
            dst_row[pixel_offset + 2] = src_row[pixel_offset + 0];
            dst_row[pixel_offset + 3] = src_row[pixel_offset + 3];
        }
    }

    D3D12_RANGE write_range{0, 0};
    g_renderer.readback.resource->Unmap(0, &write_range);
    return capture;
#else
    return std::nullopt;
#endif
}

inline DebugArtifactBundleResult windows_write_artifact_bundle(
        char const* directory,
        char const* reason) {
    auto snapshot = windows_snapshot_json();
    auto runtime_json = json::emit(
        windows_platform_runtime_details_json_with_reason(
            reason ? std::string_view(reason) : std::string_view{}));
    auto frame = windows_capture_frame_rgba();
    return ::phenotype::diag::detail::write_debug_artifact_bundle(
        directory ? std::string_view(directory) : std::string_view{},
        "windows",
        snapshot,
        runtime_json,
        frame ? &*frame : nullptr);
}

inline void install_windows_debug_providers() {
    ::phenotype::diag::detail::set_platform_capabilities_provider(
        windows_debug_capabilities);
    ::phenotype::diag::detail::set_platform_runtime_details_provider(
        windows_platform_runtime_details_json);
}

#endif

} // namespace phenotype::native::detail

export namespace phenotype::native {

inline platform_api const& windows_platform() {
#ifdef _WIN32
    detail::install_windows_debug_providers();
    static platform_api api{
        "windows",
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
            detail::sync_ime_windows,
            detail::windows_uses_shared_caret_blink,
            detail::input_handle_cursor_pos,
            detail::input_handle_mouse_button,
            detail::input_dismiss_transient,
            detail::windows_scroll_delta_y,
        },
        {
            detail::windows_debug_capabilities,
            detail::windows_snapshot_json,
            detail::windows_capture_frame_rgba,
            detail::windows_write_artifact_bundle,
        },
        [](char const* url, unsigned int len) {
            if (!url || len == 0)
                return;
            auto opened = cppx::os::system::open_url(std::string_view(url, len));
            if (!opened) {
                auto reason = cppx::os::to_string(opened.error());
                std::fprintf(
                    stderr,
                    "[windows] failed to open url: %.*s (%.*s)\n",
                    static_cast<int>(len),
                    url,
                    static_cast<int>(reason.size()),
                    reason.data());
            }
        },
        nullptr,
    };
    return api;
#else
    static platform_api api = make_stub_platform(
        "windows",
        "[phenotype-native] using Windows stub backend; renderer/text are not implemented yet");
    return api;
#endif
}

#ifdef _WIN32
export namespace windows_test {

struct CompositionVisualDebug {
    bool valid = false;
    std::size_t composition_anchor = 0;
    std::string erase_text;
    std::string visible_text;
    std::size_t caret_bytes = 0;
    std::size_t marked_start = 0;
    std::size_t marked_end = 0;
};

struct RemoteImageDebug {
    bool entry_exists = false;
    int entry_state = -1;
    std::size_t pending_jobs = 0;
    std::size_t completed_jobs = 0;
    bool worker_started = false;
    std::string failure_reason;
};

struct Dx12DiagnosticsDebug {
    bool lost = false;
    bool dred_enabled = false;
    long last_failure_hr = S_OK;
    long device_removed_reason = S_OK;
    long last_close_hr = S_OK;
    long last_present_hr = S_OK;
    long last_signal_hr = S_OK;
    int dred_device_state = static_cast<int>(D3D12_DRED_DEVICE_STATE_UNKNOWN);
    std::string failure_label;
};

inline HWND attached_hwnd() {
    return detail::g_ime.hwnd;
}

inline void reset_input_debug_counters() {
    detail::g_ime.sync_call_count = 0;
    detail::g_ime.repaint_request_count = 0;
    detail::g_ime.deferred_repaint_count = 0;
    detail::g_ime.repaint_pending = false;
}

inline std::size_t sync_call_count() {
    return detail::g_ime.sync_call_count;
}

inline std::size_t repaint_request_count() {
    return detail::g_ime.repaint_request_count;
}

inline std::size_t deferred_repaint_count() {
    return detail::g_ime.deferred_repaint_count;
}

inline bool repaint_pending() {
    return detail::g_ime.repaint_pending;
}

inline std::size_t composition_anchor() {
    return detail::g_ime.composition_anchor;
}

inline void set_composition_for_tests(
        char const* text,
        std::size_t anchor,
        LONG cursor_units,
        std::size_t replacement_end = static_cast<std::size_t>(-1)) {
    auto snapshot = ::phenotype::detail::focused_input_snapshot();
    if (snapshot.valid) {
        anchor = ::phenotype::detail::clamp_utf8_boundary(snapshot.value, anchor);
        if (replacement_end == static_cast<std::size_t>(-1)) {
            replacement_end = anchor;
        } else {
            replacement_end = ::phenotype::detail::clamp_utf8_boundary(
                snapshot.value,
                replacement_end);
        }
        if (replacement_end < anchor)
            replacement_end = anchor;
    } else if (replacement_end == static_cast<std::size_t>(-1)) {
        replacement_end = anchor;
    }
    detail::g_ime.composition_text = cppx::unicode::utf8_to_wide(
        std::string_view{text ? text : "",
                         text ? std::strlen(text) : 0u})
                                         .value_or(std::wstring{});
    detail::g_ime.composition_active = !detail::g_ime.composition_text.empty();
    detail::g_ime.composition_anchor = anchor;
    detail::g_ime.replacement_start = anchor;
    detail::g_ime.replacement_end = replacement_end;
    detail::g_ime.composition_cursor = cursor_units;
    detail::sync_input_debug_composition_state();
}

inline void clear_composition_for_tests() {
    detail::g_ime.composition_active = false;
    detail::g_ime.composition_text.clear();
    detail::g_ime.composition_cursor = 0;
    detail::g_ime.composition_anchor = 0;
    detail::g_ime.replacement_start = 0;
    detail::g_ime.replacement_end = 0;
    detail::sync_input_debug_composition_state();
}

inline CompositionVisualDebug composition_visual_debug() {
    auto visual = detail::current_composition_visual_state();
    return {
        visual.valid,
        detail::g_ime.composition_anchor,
        std::move(visual.erase_text),
        std::move(visual.visible_text),
        visual.caret_bytes,
        visual.marked_start,
        visual.marked_end,
    };
}

inline std::vector<TextEntry> suppressed_text_entries_for_tests(
        std::vector<TextEntry> entries) {
    detail::DecodedFrame frame{};
    frame.text_entries = std::move(entries);
    detail::suppress_focused_input_base_text_for_composition(frame);
    return frame.text_entries;
}

inline std::vector<TextEntry> composition_overlay_text_entries_for_tests() {
    return detail::composition_overlay_text_entries();
}

inline void stop_image_worker() {
    detail::g_images.queue_only_for_tests = true;
    detail::shutdown_image_worker();
}

inline bool has_pending_remote_image(std::string const& url) {
    if (auto const* entry = detail::find_image_entry(url);
        entry && entry->state != detail::ImageEntryState::pending) {
        return false;
    }
    std::lock_guard lock(detail::g_images.mutex);
    for (auto index = detail::g_images.pending_head;
         index < detail::g_images.pending_jobs.size();
         ++index) {
        if (detail::g_images.pending_jobs[index] == url)
            return true;
    }
    return false;
}

inline void inject_completed_image(
        std::string url,
        int width,
        int height,
        bool failed = false,
        std::string error_detail = {}) {
    detail::DecodedImage decoded;
    decoded.url = std::move(url);
    decoded.width = width;
    decoded.height = height;
    decoded.failed = failed;
    decoded.error_detail = std::move(error_detail);
    if (!failed && width > 0 && height > 0) {
        decoded.pixels.resize(static_cast<std::size_t>(width * height * 4));
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                auto offset = static_cast<std::size_t>((y * width + x) * 4);
                decoded.pixels[offset + 0] = static_cast<unsigned char>(32 + x * 40);
                decoded.pixels[offset + 1] = static_cast<unsigned char>(96 + y * 40);
                decoded.pixels[offset + 2] = static_cast<unsigned char>(180);
                decoded.pixels[offset + 3] = static_cast<unsigned char>(255);
            }
        }
    }
    if (detail::g_images.queue_only_for_tests) {
        std::vector<std::string>().swap(detail::g_images.pending_jobs);
        detail::g_images.pending_head = 0;
        detail::store_decoded_image(std::move(decoded));
        return;
    }
    std::lock_guard lock(detail::g_images.mutex);
    detail::g_images.completed_jobs.push_back(std::move(decoded));
}

inline RemoteImageDebug remote_image_debug(std::string const& url) {
    RemoteImageDebug debug{};
    if (auto const* entry = detail::find_image_entry(url)) {
        debug.entry_exists = true;
        debug.entry_state = static_cast<int>(entry->state);
        debug.failure_reason = entry->failure_reason;
    }
    {
        std::lock_guard lock(detail::g_images.mutex);
        debug.pending_jobs = detail::pending_job_count_unlocked();
        debug.completed_jobs = detail::g_images.completed_jobs.size();
        debug.worker_started = detail::g_images.worker_started;
    }
    if (detail::g_images.queue_only_for_tests
        && debug.entry_exists
        && debug.entry_state != static_cast<int>(detail::ImageEntryState::pending)) {
        debug.pending_jobs = 0;
    }
    return debug;
}

inline Dx12DiagnosticsDebug dx12_diagnostics_debug() {
    return {
        detail::g_renderer.lost,
        detail::g_renderer.dred_enabled,
        detail::g_renderer.last_failure_hr,
        detail::g_renderer.device_removed_reason,
        detail::g_renderer.last_close_hr,
        detail::g_renderer.last_present_hr,
        detail::g_renderer.last_signal_hr,
        static_cast<int>(detail::g_renderer.dred_device_state),
        detail::g_renderer.last_failure_label,
    };
}

} // namespace windows_test
#endif

} // namespace phenotype::native
#endif
