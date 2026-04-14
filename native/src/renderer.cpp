#include "renderer.h"
#include "text.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <optional>
#include <variant>
#include <vector>

#include <webgpu/webgpu_cpp.h>
#include <webgpu/webgpu_glfw.h>
#include <GLFW/glfw3.h>

extern "C" {
    extern unsigned char phenotype_cmd_buf[];
    extern unsigned int phenotype_cmd_len;
}

import phenotype;

namespace native::renderer {

// --- Globals ---

static wgpu::Instance        g_instance;
static wgpu::Device          g_device;
static wgpu::Surface         g_surface;
static wgpu::Queue           g_queue;
static GLFWwindow*           g_window = nullptr;
static wgpu::RenderPipeline  g_color_pipeline;
static wgpu::RenderPipeline  g_text_pipeline;
static wgpu::Buffer          g_uniform_buf;
static wgpu::BindGroup       g_bind_group;
static wgpu::BindGroupLayout g_bind_group_layout;
static wgpu::BindGroupLayout g_text_bind_group_layout;
static wgpu::ShaderModule    g_shader_module;
static wgpu::PipelineLayout  g_pipeline_layout;
static wgpu::PipelineLayout  g_text_pipeline_layout;
static wgpu::Sampler         g_sampler;

// Hit regions from the last frame (for event hit testing)
static std::vector<phenotype::HitRegionCmd> g_hit_regions;

// --- WGSL color shader (from shim/phenotype.js) ---

static constexpr char SHADER_CODE[] = R"(
struct Uniforms { viewport: vec2f, _pad: vec2f };
@group(0) @binding(0) var<uniform> uniforms: Uniforms;

struct ColorInstance {
  @location(0) rect: vec4f,
  @location(1) color: vec4f,
  @location(2) params: vec4f,
};
struct ColorVsOut {
  @builtin(position) pos: vec4f,
  @location(0) color: vec4f,
  @location(1) local_pos: vec2f,
  @location(2) rect_size: vec2f,
  @location(3) params: vec4f,
};
@vertex fn vs_color(
  @builtin(vertex_index) vi: u32,
  @builtin(instance_index) ii: u32,
  inst: ColorInstance,
) -> ColorVsOut {
  var corners = array<vec2f, 6>(
    vec2f(0, 0), vec2f(1, 0), vec2f(0, 1),
    vec2f(1, 0), vec2f(1, 1), vec2f(0, 1),
  );
  let c = corners[vi];
  let px = inst.rect.x + c.x * inst.rect.z;
  let py = inst.rect.y + c.y * inst.rect.w;
  let cx = (px / uniforms.viewport.x) * 2.0 - 1.0;
  let cy = 1.0 - (py / uniforms.viewport.y) * 2.0;
  var out: ColorVsOut;
  out.pos = vec4f(cx, cy, 0, 1);
  out.color = inst.color;
  out.local_pos = c * inst.rect.zw;
  out.rect_size = inst.rect.zw;
  out.params = inst.params;
  return out;
}
@fragment fn fs_color(in: ColorVsOut) -> @location(0) vec4f {
  let draw_type = u32(in.params.z);
  let radius = in.params.x;
  let border_w = in.params.y;
  if (draw_type == 2u && radius > 0.0) {
    let half_size = in.rect_size * 0.5;
    let p = abs(in.local_pos - half_size) - half_size + vec2f(radius);
    let d = length(max(p, vec2f(0.0))) - radius;
    if (d > 0.5) { discard; }
    let alpha = in.color.a * clamp(0.5 - d, 0.0, 1.0);
    return vec4f(in.color.rgb * alpha, alpha);
  }
  if (draw_type == 1u) {
    let lp = in.local_pos;
    let sz = in.rect_size;
    let bw = border_w;
    if (lp.x > bw && lp.x < sz.x - bw && lp.y > bw && lp.y < sz.y - bw) { discard; }
    return in.color;
  }
  if (draw_type == 3u) { return in.color; }
  return in.color;
}

// --- Text pipeline ---
struct TextInstance {
  @location(0) rect: vec4f,
  @location(1) uv_rect: vec4f,
};
struct TextVsOut {
  @builtin(position) pos: vec4f,
  @location(0) uv: vec2f,
};
@vertex fn vs_text(
  @builtin(vertex_index) vi: u32,
  inst: TextInstance,
) -> TextVsOut {
  var corners = array<vec2f, 6>(
    vec2f(0, 0), vec2f(1, 0), vec2f(0, 1),
    vec2f(1, 0), vec2f(1, 1), vec2f(0, 1),
  );
  let c = corners[vi];
  let px = inst.rect.x + c.x * inst.rect.z;
  let py = inst.rect.y + c.y * inst.rect.w;
  let cx = (px / uniforms.viewport.x) * 2.0 - 1.0;
  let cy = 1.0 - (py / uniforms.viewport.y) * 2.0;
  var out: TextVsOut;
  out.pos = vec4f(cx, cy, 0, 1);
  out.uv = inst.uv_rect.xy + c * inst.uv_rect.zw;
  return out;
}
@group(0) @binding(1) var textAtlas: texture_2d<f32>;
@group(0) @binding(2) var textSampler: sampler;
@fragment fn fs_text(in: TextVsOut) -> @location(0) vec4f {
  let sample = textureSample(textAtlas, textSampler, in.uv);
  if (sample.a < 0.01) { discard; }
  return sample;
}
)";

// --- Error callback ---

static void on_device_error(wgpu::Device const&, wgpu::ErrorType type, wgpu::StringView message) {
    std::fprintf(stderr, "[Dawn] error %d: %.*s\n",
        static_cast<int>(type),
        static_cast<int>(message.length), message.data);
}

// --- Pipeline setup ---

static void create_pipelines() {
    // Shader module
    wgpu::ShaderSourceWGSL wgslDesc{};
    wgslDesc.code = SHADER_CODE;
    wgpu::ShaderModuleDescriptor shaderDesc{};
    shaderDesc.nextInChain = &wgslDesc;
    g_shader_module = g_device.CreateShaderModule(&shaderDesc);

    // Uniform buffer (viewport: vec2f + pad)
    wgpu::BufferDescriptor ubDesc{};
    ubDesc.size = 16;
    ubDesc.usage = wgpu::BufferUsage::Uniform | wgpu::BufferUsage::CopyDst;
    g_uniform_buf = g_device.CreateBuffer(&ubDesc);

    // Bind group layout + bind group
    wgpu::BindGroupLayoutEntry bglEntry{};
    bglEntry.binding = 0;
    bglEntry.visibility = wgpu::ShaderStage::Vertex;
    bglEntry.buffer.type = wgpu::BufferBindingType::Uniform;

    wgpu::BindGroupLayoutDescriptor bglDesc{};
    bglDesc.entryCount = 1;
    bglDesc.entries = &bglEntry;
    g_bind_group_layout = g_device.CreateBindGroupLayout(&bglDesc);

    wgpu::BindGroupEntry bgEntry{};
    bgEntry.binding = 0;
    bgEntry.buffer = g_uniform_buf;
    bgEntry.size = 16;

    wgpu::BindGroupDescriptor bgDesc{};
    bgDesc.layout = g_bind_group_layout;
    bgDesc.entryCount = 1;
    bgDesc.entries = &bgEntry;
    g_bind_group = g_device.CreateBindGroup(&bgDesc);

    // Pipeline layout
    wgpu::PipelineLayoutDescriptor plDesc{};
    plDesc.bindGroupLayoutCount = 1;
    plDesc.bindGroupLayouts = &g_bind_group_layout;
    g_pipeline_layout = g_device.CreatePipelineLayout(&plDesc);

    // Vertex layout: 12 floats per instance (rect:4 + color:4 + params:4)
    wgpu::VertexAttribute attrs[3]{};
    attrs[0].format = wgpu::VertexFormat::Float32x4; attrs[0].offset = 0;  attrs[0].shaderLocation = 0;
    attrs[1].format = wgpu::VertexFormat::Float32x4; attrs[1].offset = 16; attrs[1].shaderLocation = 1;
    attrs[2].format = wgpu::VertexFormat::Float32x4; attrs[2].offset = 32; attrs[2].shaderLocation = 2;

    wgpu::VertexBufferLayout vbLayout{};
    vbLayout.arrayStride = 48;
    vbLayout.stepMode = wgpu::VertexStepMode::Instance;
    vbLayout.attributeCount = 3;
    vbLayout.attributes = attrs;

    // Blend state for SDF antialiasing
    wgpu::BlendComponent blendColor{};
    blendColor.operation = wgpu::BlendOperation::Add;
    blendColor.srcFactor = wgpu::BlendFactor::SrcAlpha;
    blendColor.dstFactor = wgpu::BlendFactor::OneMinusSrcAlpha;
    wgpu::BlendComponent blendAlpha{};
    blendAlpha.operation = wgpu::BlendOperation::Add;
    blendAlpha.srcFactor = wgpu::BlendFactor::One;
    blendAlpha.dstFactor = wgpu::BlendFactor::OneMinusSrcAlpha;
    wgpu::BlendState blend{};
    blend.color = blendColor;
    blend.alpha = blendAlpha;

    wgpu::ColorTargetState colorTarget{};
    colorTarget.format = wgpu::TextureFormat::BGRA8Unorm;
    colorTarget.blend = &blend;

    wgpu::FragmentState fragState{};
    fragState.module = g_shader_module;
    fragState.entryPoint = "fs_color";
    fragState.targetCount = 1;
    fragState.targets = &colorTarget;

    wgpu::RenderPipelineDescriptor rpDesc{};
    rpDesc.layout = g_pipeline_layout;
    rpDesc.vertex.module = g_shader_module;
    rpDesc.vertex.entryPoint = "vs_color";
    rpDesc.vertex.bufferCount = 1;
    rpDesc.vertex.buffers = &vbLayout;
    rpDesc.fragment = &fragState;
    rpDesc.primitive.topology = wgpu::PrimitiveTopology::TriangleList;

    g_color_pipeline = g_device.CreateRenderPipeline(&rpDesc);

    // --- Text pipeline ---

    // Sampler
    wgpu::SamplerDescriptor samplerDesc{};
    samplerDesc.minFilter = wgpu::FilterMode::Linear;
    samplerDesc.magFilter = wgpu::FilterMode::Linear;
    g_sampler = g_device.CreateSampler(&samplerDesc);

    // Text bind group layout: uniform + texture + sampler
    wgpu::BindGroupLayoutEntry textBglEntries[3]{};
    textBglEntries[0].binding = 0;
    textBglEntries[0].visibility = wgpu::ShaderStage::Vertex;
    textBglEntries[0].buffer.type = wgpu::BufferBindingType::Uniform;
    textBglEntries[1].binding = 1;
    textBglEntries[1].visibility = wgpu::ShaderStage::Fragment;
    textBglEntries[1].texture.sampleType = wgpu::TextureSampleType::Float;
    textBglEntries[1].texture.viewDimension = wgpu::TextureViewDimension::e2D;
    textBglEntries[2].binding = 2;
    textBglEntries[2].visibility = wgpu::ShaderStage::Fragment;
    textBglEntries[2].sampler.type = wgpu::SamplerBindingType::Filtering;

    wgpu::BindGroupLayoutDescriptor textBglDesc{};
    textBglDesc.entryCount = 3;
    textBglDesc.entries = textBglEntries;
    g_text_bind_group_layout = g_device.CreateBindGroupLayout(&textBglDesc);

    // Text pipeline layout
    wgpu::PipelineLayoutDescriptor textPlDesc{};
    textPlDesc.bindGroupLayoutCount = 1;
    textPlDesc.bindGroupLayouts = &g_text_bind_group_layout;
    g_text_pipeline_layout = g_device.CreatePipelineLayout(&textPlDesc);

    // Text vertex layout: 8 floats per instance (rect:4 + uv_rect:4)
    wgpu::VertexAttribute textAttrs[2]{};
    textAttrs[0].format = wgpu::VertexFormat::Float32x4; textAttrs[0].offset = 0;  textAttrs[0].shaderLocation = 0;
    textAttrs[1].format = wgpu::VertexFormat::Float32x4; textAttrs[1].offset = 16; textAttrs[1].shaderLocation = 1;

    wgpu::VertexBufferLayout textVbLayout{};
    textVbLayout.arrayStride = 32; // 8 * 4 bytes
    textVbLayout.stepMode = wgpu::VertexStepMode::Instance;
    textVbLayout.attributeCount = 2;
    textVbLayout.attributes = textAttrs;

    // Text color target with alpha blending
    wgpu::ColorTargetState textColorTarget{};
    textColorTarget.format = wgpu::TextureFormat::BGRA8Unorm;
    textColorTarget.blend = &blend; // reuse blend state from color pipeline

    wgpu::FragmentState textFragState{};
    textFragState.module = g_shader_module;
    textFragState.entryPoint = "fs_text";
    textFragState.targetCount = 1;
    textFragState.targets = &textColorTarget;

    wgpu::RenderPipelineDescriptor textRpDesc{};
    textRpDesc.layout = g_text_pipeline_layout;
    textRpDesc.vertex.module = g_shader_module;
    textRpDesc.vertex.entryPoint = "vs_text";
    textRpDesc.vertex.bufferCount = 1;
    textRpDesc.vertex.buffers = &textVbLayout;
    textRpDesc.fragment = &textFragState;
    textRpDesc.primitive.topology = wgpu::PrimitiveTopology::TriangleList;

    g_text_pipeline = g_device.CreateRenderPipeline(&textRpDesc);
}

// --- Init ---

void init(GLFWwindow* window) {
    g_window = window;

    wgpu::InstanceFeatureName requiredFeatures[] = {
        wgpu::InstanceFeatureName::TimedWaitAny,
    };
    wgpu::InstanceDescriptor instanceDesc{};
    instanceDesc.requiredFeatureCount = 1;
    instanceDesc.requiredFeatures = requiredFeatures;
    g_instance = wgpu::CreateInstance(&instanceDesc);
    if (!g_instance) return;

    g_surface = wgpu::glfw::CreateSurfaceForWindow(g_instance, window);
    if (!g_surface) return;

    wgpu::RequestAdapterOptions adapterOpts{};
    adapterOpts.compatibleSurface = g_surface;
    wgpu::Adapter adapter;
    auto af = g_instance.RequestAdapter(&adapterOpts, wgpu::CallbackMode::WaitAnyOnly,
        [&](wgpu::RequestAdapterStatus s, wgpu::Adapter r, wgpu::StringView) {
            if (s == wgpu::RequestAdapterStatus::Success) adapter = std::move(r);
        });
    g_instance.WaitAny(af, UINT64_MAX);
    if (!adapter) return;

    wgpu::DeviceDescriptor deviceDesc{};
    deviceDesc.SetUncapturedErrorCallback(on_device_error);
    auto df = adapter.RequestDevice(&deviceDesc, wgpu::CallbackMode::WaitAnyOnly,
        [&](wgpu::RequestDeviceStatus s, wgpu::Device r, wgpu::StringView) {
            if (s == wgpu::RequestDeviceStatus::Success) g_device = std::move(r);
        });
    g_instance.WaitAny(df, UINT64_MAX);
    if (!g_device) return;

    g_queue = g_device.GetQueue();

    int w, h;
    glfwGetFramebufferSize(window, &w, &h);
    wgpu::SurfaceConfiguration config{};
    config.device = g_device;
    config.format = wgpu::TextureFormat::BGRA8Unorm;
    config.width = static_cast<uint32_t>(w);
    config.height = static_cast<uint32_t>(h);
    config.presentMode = wgpu::PresentMode::Fifo;
    g_surface.Configure(&config);

    create_pipelines();
    std::printf("[phenotype-native] Dawn initialized (%dx%d)\n", w, h);
}

// --- Flush ---

void flush() {
    if (phenotype_cmd_len == 0) return;
    if (!g_device || !g_surface || !g_color_pipeline) return;

    auto cmds = phenotype::parse_commands(phenotype_cmd_buf, phenotype_cmd_len);

    // Build clear color + color instances + text entries + hit regions
    double cr = 0.98, cg = 0.98, cb = 0.98, ca = 1.0;
    std::vector<float> colorData;
    std::vector<native::text::TextEntry> textEntries;
    g_hit_regions.clear();

    for (auto& cmd : cmds) {
        if (auto* c = std::get_if<phenotype::ClearCmd>(&cmd)) {
            cr = c->color.r / 255.0; cg = c->color.g / 255.0;
            cb = c->color.b / 255.0; ca = c->color.a / 255.0;
        } else if (auto* t = std::get_if<phenotype::DrawTextCmd>(&cmd)) {
            textEntries.push_back({t->x, t->y, t->font_size, t->mono,
                t->color.r/255.f, t->color.g/255.f, t->color.b/255.f, t->color.a/255.f,
                t->text});
        } else if (auto* hr = std::get_if<phenotype::HitRegionCmd>(&cmd)) {
            g_hit_regions.push_back(*hr);
        } else if (auto* r = std::get_if<phenotype::FillRectCmd>(&cmd)) {
            colorData.insert(colorData.end(), {
                r->x, r->y, r->w, r->h,
                r->color.r/255.f, r->color.g/255.f, r->color.b/255.f, r->color.a/255.f,
                0, 0, 0, 0 });
        } else if (auto* s = std::get_if<phenotype::StrokeRectCmd>(&cmd)) {
            colorData.insert(colorData.end(), {
                s->x, s->y, s->w, s->h,
                s->color.r/255.f, s->color.g/255.f, s->color.b/255.f, s->color.a/255.f,
                0, s->line_width, 1, 0 });
        } else if (auto* rr = std::get_if<phenotype::RoundRectCmd>(&cmd)) {
            colorData.insert(colorData.end(), {
                rr->x, rr->y, rr->w, rr->h,
                rr->color.r/255.f, rr->color.g/255.f, rr->color.b/255.f, rr->color.a/255.f,
                rr->radius, 0, 2, 0 });
        } else if (auto* l = std::get_if<phenotype::DrawLineCmd>(&cmd)) {
            float dx = l->x2 - l->x1, dy = l->y2 - l->y1;
            float len = std::sqrt(dx*dx + dy*dy);
            float w = (dy == 0) ? len : l->thickness;
            float h = (dx == 0) ? len : l->thickness;
            float x = (dx == 0) ? l->x1 - l->thickness/2 : (l->x1 < l->x2 ? l->x1 : l->x2);
            float y = (dy == 0) ? l->y1 - l->thickness/2 : (l->y1 < l->y2 ? l->y1 : l->y2);
            colorData.insert(colorData.end(), {
                x, y, w, h,
                l->color.r/255.f, l->color.g/255.f, l->color.b/255.f, l->color.a/255.f,
                0, 0, 3, 0 });
        }
    }

    // Reconfigure surface on resize
    int fbw, fbh;
    glfwGetFramebufferSize(g_window, &fbw, &fbh);
    if (fbw == 0 || fbh == 0) return;

    wgpu::SurfaceConfiguration config{};
    config.device = g_device;
    config.format = wgpu::TextureFormat::BGRA8Unorm;
    config.width = static_cast<uint32_t>(fbw);
    config.height = static_cast<uint32_t>(fbh);
    config.presentMode = wgpu::PresentMode::Fifo;
    g_surface.Configure(&config);

    // Update viewport uniform
    int winw, winh;
    glfwGetWindowSize(g_window, &winw, &winh);
    float uniforms[4] = {static_cast<float>(winw), static_cast<float>(winh), 0, 0};
    g_queue.WriteBuffer(g_uniform_buf, 0, uniforms, 16);

    // Get surface texture
    wgpu::SurfaceTexture surfaceTexture;
    g_surface.GetCurrentTexture(&surfaceTexture);
    if (surfaceTexture.status != wgpu::SurfaceGetCurrentTextureStatus::SuccessOptimal &&
        surfaceTexture.status != wgpu::SurfaceGetCurrentTextureStatus::SuccessSuboptimal)
        return;

    wgpu::TextureView view = surfaceTexture.texture.CreateView();

    // Render pass
    wgpu::RenderPassColorAttachment colorAttachment{};
    colorAttachment.view = view;
    colorAttachment.loadOp = wgpu::LoadOp::Clear;
    colorAttachment.storeOp = wgpu::StoreOp::Store;
    colorAttachment.clearValue = {cr, cg, cb, ca};

    wgpu::RenderPassDescriptor passDesc{};
    passDesc.colorAttachmentCount = 1;
    passDesc.colorAttachments = &colorAttachment;

    auto encoder = g_device.CreateCommandEncoder();
    auto pass = encoder.BeginRenderPass(&passDesc);
    pass.SetBindGroup(0, g_bind_group);

    // Draw color instances
    uint32_t instanceCount = static_cast<uint32_t>(colorData.size() / 12);
    if (instanceCount > 0) {
        wgpu::BufferDescriptor ibDesc{};
        ibDesc.size = colorData.size() * sizeof(float);
        ibDesc.usage = wgpu::BufferUsage::Vertex | wgpu::BufferUsage::CopyDst;
        ibDesc.mappedAtCreation = true;
        auto instanceBuf = g_device.CreateBuffer(&ibDesc);
        std::memcpy(instanceBuf.GetMappedRange(), colorData.data(), ibDesc.size);
        instanceBuf.Unmap();

        pass.SetPipeline(g_color_pipeline);
        pass.SetVertexBuffer(0, instanceBuf);
        pass.Draw(6, instanceCount);
    }

    // Draw text instances
    if (!textEntries.empty() && g_text_pipeline) {
        auto atlas = native::text::build_atlas(textEntries);
        if (!atlas.quads.empty() && !atlas.pixels.empty()) {
            // Create atlas texture
            wgpu::TextureDescriptor texDesc{};
            texDesc.size = {static_cast<uint32_t>(atlas.width),
                            static_cast<uint32_t>(atlas.height), 1};
            texDesc.format = wgpu::TextureFormat::RGBA8Unorm;
            texDesc.usage = wgpu::TextureUsage::TextureBinding | wgpu::TextureUsage::CopyDst;
            auto atlasTex = g_device.CreateTexture(&texDesc);

            // Upload atlas pixels
            wgpu::TexelCopyBufferLayout dataLayout{};
            dataLayout.bytesPerRow = static_cast<uint32_t>(atlas.width * 4);
            dataLayout.rowsPerImage = static_cast<uint32_t>(atlas.height);
            wgpu::TexelCopyTextureInfo destInfo{};
            destInfo.texture = atlasTex;
            wgpu::Extent3D extent = {static_cast<uint32_t>(atlas.width),
                                      static_cast<uint32_t>(atlas.height), 1};
            g_queue.WriteTexture(&destInfo, atlas.pixels.data(),
                atlas.pixels.size(), &dataLayout, &extent);

            // Create text bind group
            wgpu::BindGroupEntry textBgEntries[3]{};
            textBgEntries[0].binding = 0;
            textBgEntries[0].buffer = g_uniform_buf;
            textBgEntries[0].size = 16;
            textBgEntries[1].binding = 1;
            textBgEntries[1].textureView = atlasTex.CreateView();
            textBgEntries[2].binding = 2;
            textBgEntries[2].sampler = g_sampler;

            wgpu::BindGroupDescriptor textBgDesc{};
            textBgDesc.layout = g_text_bind_group_layout;
            textBgDesc.entryCount = 3;
            textBgDesc.entries = textBgEntries;
            auto textBindGroup = g_device.CreateBindGroup(&textBgDesc);

            // Build text instance buffer (8 floats per quad)
            std::vector<float> textData;
            textData.reserve(atlas.quads.size() * 8);
            for (auto& q : atlas.quads) {
                textData.insert(textData.end(), {
                    q.x, q.y, q.w, q.h,
                    q.u, q.v, q.uw, q.vh
                });
            }

            wgpu::BufferDescriptor tbDesc{};
            tbDesc.size = textData.size() * sizeof(float);
            tbDesc.usage = wgpu::BufferUsage::Vertex | wgpu::BufferUsage::CopyDst;
            tbDesc.mappedAtCreation = true;
            auto textBuf = g_device.CreateBuffer(&tbDesc);
            std::memcpy(textBuf.GetMappedRange(), textData.data(), tbDesc.size);
            textBuf.Unmap();

            pass.SetBindGroup(0, textBindGroup);
            pass.SetPipeline(g_text_pipeline);
            pass.SetVertexBuffer(0, textBuf);
            pass.Draw(6, static_cast<uint32_t>(atlas.quads.size()));
        }
    }

    pass.End();
    auto commands = encoder.Finish();
    g_queue.Submit(1, &commands);
    g_surface.Present();
}

std::optional<unsigned int> hit_test(float x, float y, float scroll_y) {
    float wy = y + scroll_y; // world-space Y
    for (int i = static_cast<int>(g_hit_regions.size()) - 1; i >= 0; --i) {
        auto& hr = g_hit_regions[i];
        if (x >= hr.x && x <= hr.x + hr.w && wy >= hr.y && wy <= hr.y + hr.h)
            return hr.callback_id;
    }
    return std::nullopt;
}

void shutdown() {
    g_color_pipeline = nullptr;
    g_shader_module = nullptr;
    g_pipeline_layout = nullptr;
    g_uniform_buf = nullptr;
    g_bind_group = nullptr;
    g_bind_group_layout = nullptr;
    g_queue = nullptr;
    g_surface = nullptr;
    g_device = nullptr;
    g_instance = nullptr;
}

} // namespace native::renderer
