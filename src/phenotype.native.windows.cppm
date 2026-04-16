module;
#ifndef __wasi__
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include <GLFW/glfw3.h>

#ifdef _WIN32
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>

#include <d3d12.h>
#include <d3d12sdklayers.h>
#include <d3dcompiler.h>
#include <dxgi1_6.h>
#include <dwrite.h>
#include <wrl/client.h>

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "dwrite.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "user32.lib")
#endif
#endif

export module phenotype.native.windows;

#ifndef __wasi__
import phenotype.commands;
import phenotype.state;
import phenotype.native.platform;
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

inline std::wstring utf8_to_wstring(char const* text, unsigned int len) {
    if (!text || len == 0) return {};
    int needed = MultiByteToWideChar(
        CP_UTF8, MB_ERR_INVALID_CHARS,
        text, static_cast<int>(len), nullptr, 0);
    if (needed <= 0) {
        needed = MultiByteToWideChar(
            CP_UTF8, 0,
            text, static_cast<int>(len), nullptr, 0);
    }
    if (needed <= 0) return {};
    std::wstring out(static_cast<std::size_t>(needed), L'\0');
    MultiByteToWideChar(
        CP_UTF8, 0,
        text, static_cast<int>(len),
        out.data(), needed);
    return out;
}

inline std::wstring utf8_to_wstring(std::string const& text) {
    return utf8_to_wstring(text.c_str(),
                           static_cast<unsigned int>(text.size()));
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

    auto wide = utf8_to_wstring(text_ptr, len);
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
                        COLORREF text_color)
        : refs_(1),
          target_(target),
          rendering_params_(rendering_params),
          text_color_(text_color) {
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
        *pixels_per_dip = 1.0f;
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
};

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

        auto wide = utf8_to_wstring(entry.text);
        if (wide.empty()) continue;

        float scaled_font_size = entry.font_size * scale;
        ComPtr<IDWriteTextLayout> metrics_layout;
        auto hr = make_text_layout(
            wide,
            scaled_font_size,
            entry.mono,
            16384.0f,
            scaled_font_size * 4.0f + 64.0f,
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

        float logical_width = metrics.widthIncludingTrailingWhitespace / scale;
        float ascent = line_metrics[0].baseline;
        float descent = line_metrics[0].height - line_metrics[0].baseline;
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
            scaled_font_size,
            entry.mono,
            static_cast<float>(box.slot_width - padding * 2),
            static_cast<float>(box.render_height),
            draw_layout);
        if (FAILED(hr)) continue;

        ComPtr<IDWriteBitmapRenderTarget> target;
        hr = g_text.gdi_interop->CreateBitmapRenderTarget(
            nullptr,
            box.slot_width,
            box.slot_height,
            &target);
        if (FAILED(hr) || !target) continue;

        HDC dc = target->GetMemoryDC();
        if (!dc) continue;

        RECT clear_rect{0, 0, box.slot_width, box.slot_height};
        FillRect(dc, &clear_rect, static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH)));
        SetBkMode(dc, TRANSPARENT);

        auto* renderer = new GlyphBitmapRenderer(
            target.Get(),
            g_text.rendering_params.Get(),
            RGB(255, 255, 255));
        hr = draw_layout->Draw(
            nullptr,
            renderer,
            static_cast<float>(padding),
            static_cast<float>(box.render_top));
        renderer->Release();
        if (FAILED(hr)) continue;

        HBITMAP bitmap = static_cast<HBITMAP>(GetCurrentObject(dc, OBJ_BITMAP));
        if (!bitmap) continue;

        BITMAPINFO bmi{};
        bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bmi.bmiHeader.biWidth = box.slot_width;
        bmi.bmiHeader.biHeight = -box.slot_height;
        bmi.bmiHeader.biPlanes = 1;
        bmi.bmiHeader.biBitCount = 32;
        bmi.bmiHeader.biCompression = BI_RGB;

        std::vector<std::uint32_t> pixels(
            static_cast<std::size_t>(box.slot_width * box.slot_height), 0);
        if (!GetDIBits(dc, bitmap, 0, static_cast<UINT>(box.slot_height),
                       pixels.data(), &bmi, DIB_RGB_COLORS)) {
            continue;
        }

        bool has_ink = false;
        int ink_min_x = box.slot_width;
        int ink_max_x = -1;
        for (int py = box.render_top; py < box.render_top + box.render_height; ++py) {
            if (py < 0 || py >= box.slot_height) continue;
            for (int px = 0; px < box.slot_width; ++px) {
                auto packed = pixels[static_cast<std::size_t>(py * box.slot_width + px)];
                auto b = static_cast<unsigned int>(packed & 0xFF);
                auto g = static_cast<unsigned int>((packed >> 8) & 0xFF);
                auto r = static_cast<unsigned int>((packed >> 16) & 0xFF);
                unsigned char alpha = static_cast<unsigned char>((std::max)({r, g, b}));
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
SamplerState atlas_samp : register(s0);

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

float4 fs_text(TextVsOut input) : SV_TARGET {
    float4 s = atlas_tex.Sample(atlas_samp, input.uv);
    clip(s.a - 0.01f);
    return s;
}
)";

struct FrameContext {
    ComPtr<ID3D12CommandAllocator> allocator;
    ComPtr<ID3D12Resource> render_target;
    ComPtr<ID3D12Resource> atlas_texture;
    D3D12_CPU_DESCRIPTOR_HANDLE rtv_handle{};
    UINT64 fence_value = 0;
};

struct UploadBuffer {
    ComPtr<ID3D12Resource> resource;
    unsigned char* mapped = nullptr;
    std::size_t capacity = 0;
    std::size_t offset = 0;
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
    ComPtr<ID3D12PipelineState> text_pipeline;
    ComPtr<ID3D12GraphicsCommandList> command_list;
    ComPtr<ID3D12Fence> fence;
    HANDLE fence_event = nullptr;
    UINT rtv_descriptor_size = 0;
    FrameContext frames[frame_count];
    UploadBuffer upload{};
    std::vector<HitRegionCmd> hit_regions;
    GLFWwindow* window = nullptr;
    bool initialized = false;
    bool debug_enabled = false;
    bool warp_enabled = false;
    bool com_initialized = false;
    UINT64 next_fence_value = 1;
};

static RendererState g_renderer;

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
    return sanitize_scale((sx > sy) ? sx : sy);
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

inline std::pair<void*, UINT64> upload_alloc(UINT64 bytes, UINT64 alignment = 16) {
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

    D3D12_STATIC_SAMPLER_DESC sampler{};
    sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;
    sampler.ShaderRegister = 0;
    sampler.RegisterSpace = 0;
    sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    sampler.MaxLOD = D3D12_FLOAT32_MAX;

    D3D12_ROOT_SIGNATURE_DESC desc{};
    desc.NumParameters = 2;
    desc.pParameters = params;
    desc.NumStaticSamplers = 1;
    desc.pStaticSamplers = &sampler;
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
    ComPtr<ID3DBlob> text_ps;
    if (FAILED(compile_shader("vs_color", "vs_5_0", color_vs))) return E_FAIL;
    if (FAILED(compile_shader("fs_color", "ps_5_0", color_ps))) return E_FAIL;
    if (FAILED(compile_shader("vs_text", "vs_5_0", text_vs))) return E_FAIL;
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
    auto text_desc = base;
    text_desc.InputLayout = {text_layout, static_cast<UINT>(std::size(text_layout))};
    text_desc.VS = {text_vs->GetBufferPointer(), text_vs->GetBufferSize()};
    text_desc.PS = {text_ps->GetBufferPointer(), text_ps->GetBufferSize()};
    return g_renderer.device->CreateGraphicsPipelineState(
        &text_desc, IID_PPV_ARGS(&g_renderer.text_pipeline));
}

inline HRESULT create_upload_buffer() {
    constexpr UINT64 upload_capacity = 32ull * 1024ull * 1024ull;
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

    D3D12_DESCRIPTOR_HEAP_DESC srv_desc{};
    srv_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    srv_desc.NumDescriptors = 1;
    srv_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    hr = g_renderer.device->CreateDescriptorHeap(
        &srv_desc, IID_PPV_ARGS(&g_renderer.srv_heap));
    if (FAILED(hr)) return hr;

    g_renderer.rtv_descriptor_size =
        g_renderer.device->GetDescriptorHandleIncrementSize(
            D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

    auto handle = g_renderer.rtv_heap->GetCPUDescriptorHandleForHeapStart();
    for (UINT i = 0; i < RendererState::frame_count; ++i) {
        hr = g_renderer.swap_chain->GetBuffer(
            i, IID_PPV_ARGS(&g_renderer.frames[i].render_target));
        if (FAILED(hr)) return hr;
        g_renderer.frames[i].rtv_handle = handle;
        g_renderer.device->CreateRenderTargetView(
            g_renderer.frames[i].render_target.Get(),
            nullptr,
            handle);
        handle.ptr += g_renderer.rtv_descriptor_size;

        hr = g_renderer.device->CreateCommandAllocator(
            D3D12_COMMAND_LIST_TYPE_DIRECT,
            IID_PPV_ARGS(&g_renderer.frames[i].allocator));
        if (FAILED(hr)) return hr;
    }

    hr = g_renderer.device->CreateCommandList(
        0,
        D3D12_COMMAND_LIST_TYPE_DIRECT,
        g_renderer.frames[0].allocator.Get(),
        nullptr,
        IID_PPV_ARGS(&g_renderer.command_list));
    if (FAILED(hr)) return hr;
    g_renderer.command_list->Close();

    hr = g_renderer.device->CreateFence(
        0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&g_renderer.fence));
    if (FAILED(hr)) return hr;

    g_renderer.fence_event = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    if (!g_renderer.fence_event) return HRESULT_FROM_WIN32(GetLastError());

    return create_upload_buffer();
}

inline HRESULT enable_debug_layers() {
    if (!g_renderer.debug_enabled) return S_OK;

    ComPtr<ID3D12Debug1> debug;
    auto hr = D3D12GetDebugInterface(IID_PPV_ARGS(&debug));
    if (FAILED(hr)) {
        log_hresult("D3D12GetDebugInterface", hr);
        return hr;
    }
    debug->EnableDebugLayer();
    debug->SetEnableGPUBasedValidation(TRUE);

    ComPtr<ID3D12DeviceRemovedExtendedDataSettings1> dred;
    if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&dred)))) {
        dred->SetAutoBreadcrumbsEnablement(D3D12_DRED_ENABLEMENT_FORCED_ON);
        dred->SetPageFaultEnablement(D3D12_DRED_ENABLEMENT_FORCED_ON);
        dred->SetBreadcrumbContextEnablement(D3D12_DRED_ENABLEMENT_FORCED_ON);
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
        frame.atlas_texture.Reset();
    }
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
    if (FAILED(hr)) return hr;

    auto handle = g_renderer.rtv_heap->GetCPUDescriptorHandleForHeapStart();
    for (UINT i = 0; i < RendererState::frame_count; ++i) {
        hr = g_renderer.swap_chain->GetBuffer(
            i, IID_PPV_ARGS(&g_renderer.frames[i].render_target));
        if (FAILED(hr)) return hr;
        g_renderer.frames[i].rtv_handle = handle;
        g_renderer.device->CreateRenderTargetView(
            g_renderer.frames[i].render_target.Get(),
            nullptr,
            handle);
        handle.ptr += g_renderer.rtv_descriptor_size;
    }
    return S_OK;
}

inline HRESULT create_atlas_texture(
        TextAtlas const& atlas,
        FrameContext& frame) {
    frame.atlas_texture.Reset();
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
    auto hr = g_renderer.device->CreateCommittedResource(
        &default_heap,
        D3D12_HEAP_FLAG_NONE,
        &tex_desc,
        D3D12_RESOURCE_STATE_COPY_DEST,
        nullptr,
        IID_PPV_ARGS(&frame.atlas_texture));
    if (FAILED(hr)) return hr;

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
    if (!mapped) return E_OUTOFMEMORY;
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
        g_renderer.srv_heap->GetCPUDescriptorHandleForHeapStart());
    return S_OK;
}

inline void begin_frame(FrameContext& frame) {
    wait_for_fence(frame.fence_value);
    frame.atlas_texture.Reset();
    frame.allocator->Reset();
    g_renderer.command_list->Reset(frame.allocator.Get(), nullptr);
    g_renderer.upload.offset = 0;
}

inline void end_frame(FrameContext& frame) {
    g_renderer.command_list->Close();
    ID3D12CommandList* lists[] = {g_renderer.command_list.Get()};
    g_renderer.queue->ExecuteCommandLists(1, lists);
    g_renderer.swap_chain->Present(1, 0);
    frame.fence_value = g_renderer.next_fence_value++;
    g_renderer.queue->Signal(g_renderer.fence.Get(), frame.fence_value);
}

inline void renderer_init(GLFWwindow* window) {
    if (g_renderer.initialized) return;
    g_renderer.window = window;
    g_renderer.debug_enabled = env_enabled("PHENOTYPE_DX12_DEBUG");
    g_renderer.warp_enabled = env_enabled("PHENOTYPE_DX12_WARP");

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
        || FAILED(create_frame_resources())) {
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
    if (len == 0 || !g_renderer.initialized) return;

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

    begin_frame(frame);
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
        static_cast<float>(cr),
        static_cast<float>(cg),
        static_cast<float>(cb),
        static_cast<float>(ca),
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

    UINT color_count = static_cast<UINT>(color_data.size() / 12);
    if (color_count > 0) {
        UINT64 bytes = static_cast<UINT64>(color_data.size() * sizeof(float));
        auto [mapped, offset] = upload_alloc(bytes, 16);
        if (mapped) {
            std::memcpy(mapped, color_data.data(), static_cast<std::size_t>(bytes));
            D3D12_VERTEX_BUFFER_VIEW vbv{};
            vbv.BufferLocation = g_renderer.upload.resource->GetGPUVirtualAddress() + offset;
            vbv.SizeInBytes = static_cast<UINT>(bytes);
            vbv.StrideInBytes = 48;
            g_renderer.command_list->SetPipelineState(g_renderer.color_pipeline.Get());
            g_renderer.command_list->IASetVertexBuffers(0, 1, &vbv);
            g_renderer.command_list->DrawInstanced(6, color_count, 0, 0);
        }
    }

    if (!text_entries.empty() && g_text.initialized) {
        auto atlas = text_build_atlas(text_entries, text_scale);
        if (!atlas.quads.empty() && !atlas.pixels.empty()
            && SUCCEEDED(create_atlas_texture(atlas, frame))) {
            std::vector<float> text_data;
            text_data.reserve(atlas.quads.size() * 8);
            for (auto const& quad : atlas.quads) {
                text_data.insert(text_data.end(), {
                    quad.x, quad.y, quad.w, quad.h,
                    quad.u, quad.v, quad.uw, quad.vh,
                });
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
                    1, g_renderer.srv_heap->GetGPUDescriptorHandleForHeapStart());
                g_renderer.command_list->SetPipelineState(g_renderer.text_pipeline.Get());
                g_renderer.command_list->IASetVertexBuffers(0, 1, &vbv);
                g_renderer.command_list->DrawInstanced(
                    6, static_cast<UINT>(atlas.quads.size()), 0, 0);
            }
        }
    }

    transition_resource(
        frame.render_target.Get(),
        D3D12_RESOURCE_STATE_RENDER_TARGET,
        D3D12_RESOURCE_STATE_PRESENT);
    end_frame(frame);
}

inline void renderer_shutdown() {
    if (g_renderer.fence) {
        for (auto const& frame : g_renderer.frames)
            wait_for_fence(frame.fence_value);
    }
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

inline void windows_open_url(char const* url, unsigned int len) {
    auto wide = utf8_to_wstring(url, len);
    if (wide.empty()) return;
    auto result = reinterpret_cast<std::intptr_t>(
        ShellExecuteW(nullptr, L"open", wide.c_str(), nullptr, nullptr, SW_SHOWNORMAL));
    if (result <= 32) {
        std::fprintf(stderr, "[windows] ShellExecuteW failed (%td)\n", result);
    }
}

#endif

} // namespace phenotype::native::detail

export namespace phenotype::native {

inline platform_api const& windows_platform() {
#ifdef _WIN32
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
        detail::windows_open_url,
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

} // namespace phenotype::native
#endif
