#import <AppKit/AppKit.h>
#import <QuartzCore/CAMetalLayer.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <string>
#include <utility>
#include <vector>
#include <webgpu/webgpu_cpp.h>

#include <phenotype/macos.hpp>

namespace {

constexpr size_t kMaxSymbolButtonCount = 8;

constexpr char kButtonShader[] = R"wgsl(
struct VertexOut {
    @builtin(position) position : vec4f,
    @location(0) pixel_position : vec2f,
};

struct SymbolButton {
    frame : vec4f,
    icon : vec4f,
    style : vec4f,
};

struct SceneUniforms {
    viewport : vec4f,
    buttons : array<SymbolButton, 8>,
};

@group(0) @binding(0) var<uniform> scene : SceneUniforms;

@vertex
fn vertexMain(@builtin(vertex_index) index : u32) -> VertexOut {
    let clip = array<vec2f, 6>(
        vec2f(-1.0, -1.0),
        vec2f(-1.0,  1.0),
        vec2f( 1.0, -1.0),
        vec2f( 1.0, -1.0),
        vec2f(-1.0,  1.0),
        vec2f( 1.0,  1.0),
    );

    let viewport_size = scene.viewport.xy;
    let pixel_position = vec2f(
        (clip[index].x + 1.0) * 0.5 * viewport_size.x,
        (1.0 - clip[index].y) * 0.5 * viewport_size.y,
    );

    var out : VertexOut;
    out.position = vec4f(clip[index], 0.0, 1.0);
    out.pixel_position = pixel_position;
    return out;
}

fn roundedRectDistance(position : vec2f, half_size : vec2f, radius : f32) -> f32 {
    let q = abs(position) - (half_size - vec2f(radius));
    return length(max(q, vec2f(0.0))) + min(max(q.x, q.y), 0.0) - radius;
}

fn segmentDistance(position : vec2f, a : vec2f, b : vec2f) -> f32 {
    let pa = position - a;
    let ba = b - a;
    let h = clamp(dot(pa, ba) / dot(ba, ba), 0.0, 1.0);
    return length(pa - (ba * h));
}

fn iconStrokeWidth(style : vec4f) -> f32 {
    let fill = clamp(style.x, 0.0, 1.0);
    let weight = clamp(style.y, 100.0, 700.0);
    let grade = clamp(style.z, -50.0, 200.0);
    let optical_size = clamp(style.w, 20.0, 48.0);
    let optical_scale = optical_size / 24.0;
    return clamp((weight / 400.0) * 2.0 + (grade * 0.004) + (fill * 0.35), 0.8, 4.8) *
        optical_scale;
}

fn chevronDistance(position : vec2f, icon : vec4f, style : vec4f) -> f32 {
    let optical_size = clamp(style.w, 20.0, 48.0);
    let icon_scale = optical_size / 24.0;
    let top = vec2f(icon.x, icon.z) * icon_scale;
    let center = vec2f(icon.y, 0.0) * icon_scale;
    let bottom = vec2f(icon.x, icon.w) * icon_scale;
    let stroke_radius = iconStrokeWidth(style) * 0.5;
    return min(segmentDistance(position, top, center), segmentDistance(position, center, bottom)) -
        stroke_radius;
}

fn compositeOver(destination : vec4f, source_color : vec3f, source_alpha : f32) -> vec4f {
    let alpha = clamp(source_alpha, 0.0, 1.0);
    return vec4f(
        destination.rgb * (1.0 - alpha) + source_color * alpha,
        destination.a * (1.0 - alpha) + alpha,
    );
}

fn drawSymbolButton(layer : vec4f, pixel_position : vec2f, button : SymbolButton) -> vec4f {
    let center = button.frame.xy;
    let size = button.frame.zw;
    let radius = 10.0;
    let local_position = pixel_position - center;

    let button_edge = roundedRectDistance(local_position, size * 0.5, radius);
    let button_coverage = 1.0 - smoothstep(-1.0, 1.0, button_edge);
    let border_coverage = 1.0 - smoothstep(-1.0, 1.0, abs(button_edge) - 0.75);
    let button_fill = vec3f(0.985, 0.988, 0.992);
    let button_border = vec3f(0.73, 0.76, 0.82);

    var out_layer = compositeOver(layer, button_fill, button_coverage * 0.72);
    out_layer = compositeOver(out_layer, button_border, border_coverage * button_coverage * 0.55);

    let icon_distance = chevronDistance(local_position, button.icon, button.style);
    let icon_coverage = 1.0 - smoothstep(-1.0, 1.0, icon_distance);
    return compositeOver(out_layer, vec3f(0.13, 0.20, 0.32), icon_coverage * button_coverage);
}

@fragment
fn fragmentMain(in : VertexOut) -> @location(0) vec4f {
    var layer = vec4f(0.0);
    let button_count = min(u32(scene.viewport.z), 8u);
    for (var index = 0u; index < button_count; index = index + 1u) {
        layer = drawSymbolButton(layer, in.pixel_position, scene.buttons[index]);
    }
    return layer;
}
)wgsl";

struct SymbolButtonUniform {
  float frame[4];
  float icon[4];
  float style[4];
};

struct SceneUniforms {
  float viewport[4];
  SymbolButtonUniform buttons[kMaxSymbolButtonCount];
};

struct LayoutRect {
  float x = 0.0f;
  float y = 0.0f;
  float width = 0.0f;
  float height = 0.0f;
};

struct SymbolButtonLayout {
  LayoutRect frame;
  phenotype::ui::Symbol symbol = phenotype::ui::Symbol::chevron_left;
  phenotype::ui::SymbolOptions options;
  bool is_enabled = true;
};

uint32_t PixelSize(CGFloat points, CGFloat scale) noexcept {
  double pixels = static_cast<double>(points * scale);
  if (pixels < 1.0) {
    return 1;
  }
  return static_cast<uint32_t>(pixels);
}

float LogicalPixel(CGFloat points, CGFloat scale) noexcept {
  return static_cast<float>(points * scale);
}

NSRect InitialWindowVisibleFrame(CGFloat width, CGFloat height) {
  NSArray<NSScreen *> *screens = [NSScreen screens];
  for (NSScreen *screen in screens) {
    NSRect frame = [screen visibleFrame];
    if (frame.origin.x == 0.0 && frame.origin.y == 0.0) {
      return frame;
    }
  }

  NSPoint mouse = [NSEvent mouseLocation];
  for (NSScreen *screen in screens) {
    if (NSPointInRect(mouse, [screen frame])) {
      return [screen visibleFrame];
    }
  }

  NSScreen *screen = [NSScreen mainScreen];
  if (screen) {
    return [screen visibleFrame];
  }
  return NSMakeRect(0.0, 0.0, width, height);
}

NSVisualEffectMaterial
ToNativeVisualMaterial(phenotype::macos::window::VisualMaterial material) {
  switch (material) {
  case phenotype::macos::window::VisualMaterial::under_window_background:
    return NSVisualEffectMaterialUnderWindowBackground;
  }
  return NSVisualEffectMaterialUnderWindowBackground;
}

phenotype::ui::Size IntrinsicSize(const phenotype::ui::View &view) {
  namespace ui = phenotype::ui;

  if (view.preferred_size.width > 0.0f || view.preferred_size.height > 0.0f) {
    return view.preferred_size;
  }

  switch (view.kind) {
  case ui::ViewKind::button:
    return {40.0f, 36.0f};
  case ui::ViewKind::icon:
    return {view.symbol_options.optical_size, view.symbol_options.optical_size};
  case ui::ViewKind::spacer:
  case ui::ViewKind::empty:
    return {};
  case ui::ViewKind::stack:
    break;
  }

  ui::Size size;
  bool has_visible_child = false;
  for (const ui::View &child : view.children) {
    ui::Size child_size = IntrinsicSize(child);
    if (view.axis == ui::LayoutAxis::horizontal) {
      if (has_visible_child) {
        size.width += view.child_spacing;
      }
      size.width += child_size.width;
      size.height = std::max(size.height, child_size.height);
    } else if (view.axis == ui::LayoutAxis::vertical) {
      if (has_visible_child) {
        size.height += view.child_spacing;
      }
      size.width = std::max(size.width, child_size.width);
      size.height += child_size.height;
    } else {
      size.width = std::max(size.width, child_size.width);
      size.height = std::max(size.height, child_size.height);
    }
    has_visible_child = true;
  }

  size.width += view.content_padding.left + view.content_padding.right;
  size.height += view.content_padding.top + view.content_padding.bottom;
  return size;
}

const phenotype::ui::View *FindIconContent(const phenotype::ui::View &view) {
  namespace ui = phenotype::ui;

  if (view.kind == ui::ViewKind::icon) {
    return &view;
  }
  for (const ui::View &child : view.children) {
    if (const ui::View *icon = FindIconContent(child)) {
      return icon;
    }
  }
  return nullptr;
}

void LayoutView(const phenotype::ui::View &view, LayoutRect rect,
                std::vector<SymbolButtonLayout> &buttons) {
  namespace ui = phenotype::ui;

  switch (view.kind) {
  case ui::ViewKind::empty:
  case ui::ViewKind::spacer:
  case ui::ViewKind::icon:
    return;
  case ui::ViewKind::button: {
    if (buttons.size() >= kMaxSymbolButtonCount) {
      return;
    }
    const ui::View *icon = FindIconContent(view);
    if (!icon) {
      return;
    }
    buttons.push_back({
        rect,
        icon->symbol,
        icon->symbol_options,
        view.is_enabled,
    });
    return;
  }
  case ui::ViewKind::stack:
    break;
  }

  LayoutRect content_rect{
      rect.x + view.content_padding.left,
      rect.y + view.content_padding.top,
      std::max(0.0f, rect.width - view.content_padding.left -
                         view.content_padding.right),
      std::max(0.0f, rect.height - view.content_padding.top -
                         view.content_padding.bottom),
  };

  if (view.axis == ui::LayoutAxis::overlay) {
    for (const ui::View &child : view.children) {
      LayoutView(child, content_rect, buttons);
    }
    return;
  }

  float cursor_x = content_rect.x;
  float cursor_y = content_rect.y;
  for (const ui::View &child : view.children) {
    ui::Size child_size = IntrinsicSize(child);
    LayoutRect child_rect{cursor_x, cursor_y, child_size.width,
                          child_size.height};
    LayoutView(child, child_rect, buttons);
    if (view.axis == ui::LayoutAxis::horizontal) {
      cursor_x += child_size.width + view.child_spacing;
    } else {
      cursor_y += child_size.height + view.child_spacing;
    }
  }
}

std::vector<SymbolButtonLayout>
LayoutSymbolButtons(const phenotype::ui::View &root, uint32_t width,
                    uint32_t height) {
  std::vector<SymbolButtonLayout> buttons;
  buttons.reserve(kMaxSymbolButtonCount);
  LayoutView(root,
             {
                 0.0f,
                 0.0f,
                 static_cast<float>(width),
                 static_cast<float>(height),
             },
             buttons);
  return buttons;
}

SymbolButtonUniform MakeSymbolButton(const SymbolButtonLayout &button) {
  auto geometry = phenotype::MaterialSymbolChevronGeometryFor(
      phenotype::ui::ToMaterialSymbolIcon(button.symbol));
  float alpha_scale = button.is_enabled ? 1.0f : 0.44f;

  return SymbolButtonUniform{
      {
          button.frame.x + (button.frame.width * 0.5f),
          button.frame.y + (button.frame.height * 0.5f),
          button.frame.width,
          button.frame.height,
      },
      {geometry.outer_x, geometry.center_x, geometry.top_y, geometry.bottom_y},
      {
          button.options.fill ? 1.0f : 0.0f,
          button.options.weight * alpha_scale,
          button.options.grade,
          button.options.optical_size,
      },
  };
}

class DawnButtonRenderer {
public:
  bool Initialize(CAMetalLayer *layer, uint32_t width, uint32_t height,
                  phenotype::ui::View root_view) {
    _root_view = std::move(root_view);

    wgpu::InstanceFeatureName required_features[] = {
        wgpu::InstanceFeatureName::TimedWaitAny,
    };
    wgpu::InstanceDescriptor instance_descriptor;
    instance_descriptor.requiredFeatureCount = 1;
    instance_descriptor.requiredFeatures = required_features;

    _instance = wgpu::CreateInstance(&instance_descriptor);
    if (!_instance) {
      std::fprintf(stderr, "Dawn: failed to create WebGPU instance\n");
      return false;
    }

    wgpu::SurfaceSourceMetalLayer metal_layer_source;
    metal_layer_source.layer = layer;

    wgpu::SurfaceDescriptor surface_descriptor;
    surface_descriptor.nextInChain = &metal_layer_source;
    _surface = _instance.CreateSurface(&surface_descriptor);
    if (!_surface) {
      std::fprintf(stderr, "Dawn: failed to create Metal surface\n");
      return false;
    }

    if (!RequestAdapter() || !RequestDevice()) {
      return false;
    }

    CreateSceneUniformBuffer();
    if (!ConfigureSurface(width, height)) {
      return false;
    }

    CreatePipeline();
    return static_cast<bool>(_pipeline);
  }

  void Resize(uint32_t width, uint32_t height) {
    if (width == 0 || height == 0 || !_device) {
      return;
    }
    if (width == _width && height == _height) {
      return;
    }
    ConfigureSurface(width, height);
  }

  void Render() {
    if (!_device || !_surface || !_pipeline) {
      return;
    }

    wgpu::SurfaceTexture surface_texture;
    _surface.GetCurrentTexture(&surface_texture);
    if (surface_texture.status !=
            wgpu::SurfaceGetCurrentTextureStatus::SuccessOptimal &&
        surface_texture.status !=
            wgpu::SurfaceGetCurrentTextureStatus::SuccessSuboptimal) {
      return;
    }

    wgpu::TextureView backbuffer = surface_texture.texture.CreateView();

    wgpu::RenderPassColorAttachment color_attachment;
    color_attachment.view = backbuffer;
    color_attachment.loadOp = wgpu::LoadOp::Clear;
    color_attachment.storeOp = wgpu::StoreOp::Store;
    color_attachment.clearValue = {0.0, 0.0, 0.0, 0.0};

    wgpu::RenderPassDescriptor render_pass_descriptor;
    render_pass_descriptor.colorAttachmentCount = 1;
    render_pass_descriptor.colorAttachments = &color_attachment;

    wgpu::CommandEncoder encoder = _device.CreateCommandEncoder();
    wgpu::RenderPassEncoder pass =
        encoder.BeginRenderPass(&render_pass_descriptor);
    pass.SetPipeline(_pipeline);
    pass.SetBindGroup(0, _scene_bind_group);
    pass.Draw(6);
    pass.End();

    wgpu::CommandBuffer commands = encoder.Finish();
    _device.GetQueue().Submit(1, &commands);
    _surface.Present();
    _instance.ProcessEvents();
  }

private:
  bool RequestAdapter() {
    wgpu::RequestAdapterOptions options;
    options.compatibleSurface = _surface;

    bool resolved = false;
    bool succeeded = false;
    wgpu::Future future = _instance.RequestAdapter(
        &options, wgpu::CallbackMode::WaitAnyOnly,
        [&](wgpu::RequestAdapterStatus status, wgpu::Adapter adapter,
            wgpu::StringView message) {
          (void)message;
          resolved = true;
          succeeded = status == wgpu::RequestAdapterStatus::Success;
          if (succeeded) {
            _adapter = adapter;
          }
        });

    _instance.WaitAny(future, UINT64_MAX);
    if (!resolved || !succeeded || !_adapter) {
      std::fprintf(stderr, "Dawn: failed to request adapter\n");
      return false;
    }
    return true;
  }

  bool RequestDevice() {
    wgpu::DeviceDescriptor descriptor;
    descriptor.SetUncapturedErrorCallback([](const wgpu::Device &,
                                             wgpu::ErrorType error_type,
                                             wgpu::StringView message) {
      std::fprintf(stderr, "Dawn device error (%u): %.*s\n",
                   static_cast<unsigned>(error_type),
                   static_cast<int>(message.length), message.data);
    });

    bool resolved = false;
    bool succeeded = false;
    wgpu::Future future = _adapter.RequestDevice(
        &descriptor, wgpu::CallbackMode::WaitAnyOnly,
        [&](wgpu::RequestDeviceStatus status, wgpu::Device device,
            wgpu::StringView message) {
          (void)message;
          resolved = true;
          succeeded = status == wgpu::RequestDeviceStatus::Success;
          if (succeeded) {
            _device = device;
          }
        });

    _instance.WaitAny(future, UINT64_MAX);
    if (!resolved || !succeeded || !_device) {
      std::fprintf(stderr, "Dawn: failed to request device\n");
      return false;
    }
    return true;
  }

  bool ConfigureSurface(uint32_t width, uint32_t height) {
    wgpu::SurfaceCapabilities capabilities;
    _surface.GetCapabilities(_adapter, &capabilities);
    if (capabilities.formatCount == 0) {
      std::fprintf(stderr, "Dawn: surface returned no supported formats\n");
      return false;
    }

    _format = capabilities.formats[0];
    _width = width;
    _height = height;
    UpdateSceneUniforms();

    wgpu::SurfaceConfiguration configuration;
    configuration.device = _device;
    configuration.format = _format;
    configuration.usage = wgpu::TextureUsage::RenderAttachment;
    configuration.width = _width;
    configuration.height = _height;
    configuration.presentMode = wgpu::PresentMode::Fifo;
    configuration.alphaMode = wgpu::CompositeAlphaMode::Premultiplied;
    _surface.Configure(&configuration);
    return true;
  }

  void CreatePipeline() {
    wgpu::ShaderSourceWGSL wgsl;
    wgsl.code = kButtonShader;

    wgpu::BindGroupLayoutEntry bind_group_layout_entry;
    bind_group_layout_entry.binding = 0;
    bind_group_layout_entry.visibility =
        wgpu::ShaderStage::Vertex | wgpu::ShaderStage::Fragment;
    bind_group_layout_entry.buffer.type = wgpu::BufferBindingType::Uniform;
    bind_group_layout_entry.buffer.minBindingSize = sizeof(SceneUniforms);

    wgpu::BindGroupLayoutDescriptor bind_group_layout_descriptor;
    bind_group_layout_descriptor.entryCount = 1;
    bind_group_layout_descriptor.entries = &bind_group_layout_entry;
    wgpu::BindGroupLayout bind_group_layout =
        _device.CreateBindGroupLayout(&bind_group_layout_descriptor);

    wgpu::PipelineLayoutDescriptor pipeline_layout_descriptor;
    pipeline_layout_descriptor.bindGroupLayoutCount = 1;
    pipeline_layout_descriptor.bindGroupLayouts = &bind_group_layout;
    wgpu::PipelineLayout pipeline_layout =
        _device.CreatePipelineLayout(&pipeline_layout_descriptor);

    wgpu::ShaderModuleDescriptor shader_descriptor;
    shader_descriptor.nextInChain = &wgsl;
    wgpu::ShaderModule shader = _device.CreateShaderModule(&shader_descriptor);

    wgpu::ColorTargetState color_target;
    color_target.format = _format;
    color_target.writeMask = wgpu::ColorWriteMask::All;

    wgpu::FragmentState fragment;
    fragment.module = shader;
    fragment.entryPoint = "fragmentMain";
    fragment.targetCount = 1;
    fragment.targets = &color_target;

    wgpu::RenderPipelineDescriptor pipeline_descriptor;
    pipeline_descriptor.layout = pipeline_layout;
    pipeline_descriptor.vertex.module = shader;
    pipeline_descriptor.vertex.entryPoint = "vertexMain";
    pipeline_descriptor.primitive.topology =
        wgpu::PrimitiveTopology::TriangleList;
    pipeline_descriptor.fragment = &fragment;
    pipeline_descriptor.multisample.count = 1;

    _pipeline = _device.CreateRenderPipeline(&pipeline_descriptor);

    wgpu::BindGroupEntry bind_group_entry;
    bind_group_entry.binding = 0;
    bind_group_entry.buffer = _scene_uniform_buffer;
    bind_group_entry.offset = 0;
    bind_group_entry.size = sizeof(SceneUniforms);

    wgpu::BindGroupDescriptor bind_group_descriptor;
    bind_group_descriptor.layout = bind_group_layout;
    bind_group_descriptor.entryCount = 1;
    bind_group_descriptor.entries = &bind_group_entry;
    _scene_bind_group = _device.CreateBindGroup(&bind_group_descriptor);
  }

  void CreateSceneUniformBuffer() {
    wgpu::BufferDescriptor buffer_descriptor;
    buffer_descriptor.size = sizeof(SceneUniforms);
    buffer_descriptor.usage =
        wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::Uniform;
    _scene_uniform_buffer = _device.CreateBuffer(&buffer_descriptor);
  }

  void UpdateSceneUniforms() {
    if (!_scene_uniform_buffer) {
      return;
    }

    std::vector<SymbolButtonLayout> buttons =
        LayoutSymbolButtons(_root_view, _width, _height);

    SceneUniforms uniforms = {};
    uniforms.viewport[0] = static_cast<float>(_width);
    uniforms.viewport[1] = static_cast<float>(_height);
    uniforms.viewport[2] = static_cast<float>(buttons.size());
    for (size_t index = 0; index < buttons.size(); ++index) {
      uniforms.buttons[index] = MakeSymbolButton(buttons[index]);
    }
    _device.GetQueue().WriteBuffer(_scene_uniform_buffer, 0, &uniforms,
                                   sizeof(uniforms));
  }

  wgpu::Instance _instance;
  wgpu::Surface _surface;
  wgpu::Adapter _adapter;
  wgpu::Device _device;
  wgpu::Buffer _scene_uniform_buffer;
  wgpu::BindGroup _scene_bind_group;
  wgpu::TextureFormat _format = wgpu::TextureFormat::Undefined;
  wgpu::RenderPipeline _pipeline;
  phenotype::ui::View _root_view;
  uint32_t _width = 0;
  uint32_t _height = 0;
};

} // namespace

@interface AppDelegate : NSObject <NSApplicationDelegate> {
  NSWindow *_window;
  NSTimer *_render_timer;
  NSView *_metal_view;
  CAMetalLayer *_metal_layer;
  std::unique_ptr<DawnButtonRenderer> _renderer;
  phenotype::macos::window::Spec _spec;
}
- (instancetype)initWithSpec:(phenotype::macos::window::Spec)spec;
@end

@implementation AppDelegate

- (instancetype)initWithSpec:(phenotype::macos::window::Spec)spec {
  self = [super init];
  if (self) {
    _spec = std::move(spec);
  }
  return self;
}

- (void)applicationDidFinishLaunching:(NSNotification *)notification {
  (void)notification;

  CGFloat window_width = std::max<CGFloat>(1.0, _spec.options.size.width);
  CGFloat window_height = std::max<CGFloat>(1.0, _spec.options.size.height);
  NSRect content_rect = NSMakeRect(0.0, 0.0, window_width, window_height);
  NSWindowStyleMask style_mask =
      NSWindowStyleMaskTitled | NSWindowStyleMaskClosable |
      NSWindowStyleMaskMiniaturizable | NSWindowStyleMaskResizable;

  _window = [[NSWindow alloc] initWithContentRect:content_rect
                                        styleMask:style_mask
                                          backing:NSBackingStoreBuffered
                                            defer:NO];
  [_window
      setTitle:[NSString stringWithUTF8String:_spec.options.title.c_str()]];

  bool uses_blur = _spec.options.background.kind ==
                   phenotype::macos::window::Background::Kind::blurred;
  if (uses_blur) {
    [_window setOpaque:NO];
    [_window setBackgroundColor:[NSColor clearColor]];
  } else {
    [_window setOpaque:YES];
    [_window setBackgroundColor:[NSColor windowBackgroundColor]];
  }

  NSRect visible_frame = InitialWindowVisibleFrame(window_width, window_height);
  [_window setFrameOrigin:NSMakePoint(
                              NSMidX(visible_frame) - (window_width / 2.0),
                              NSMidY(visible_frame) - (window_height / 2.0))];

  NSView *content_view = [[NSView alloc] initWithFrame:content_rect];
  [content_view setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];
  [content_view setWantsLayer:YES];
  [[content_view layer] setOpaque:NO];
  [[content_view layer] setBackgroundColor:[[NSColor clearColor] CGColor]];

  if (uses_blur) {
    NSVisualEffectView *background_view =
        [[NSVisualEffectView alloc] initWithFrame:content_rect];
    [background_view
        setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];
    [background_view setMaterial:ToNativeVisualMaterial(
                                     _spec.options.background.blur.material)];
    [background_view setBlendingMode:NSVisualEffectBlendingModeBehindWindow];
    [background_view setState:NSVisualEffectStateActive];
    [background_view setAlphaValue:_spec.options.background.blur.opacity];
    [content_view addSubview:background_view];
    [background_view release];
  }

  _metal_view = [[NSView alloc] initWithFrame:content_rect];
  [_metal_view setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];
  [_metal_view setWantsLayer:YES];

  _metal_layer = [CAMetalLayer layer];
  [_metal_layer setOpaque:NO];
  [_metal_layer setBackgroundColor:[[NSColor clearColor] CGColor]];
  [_metal_layer setFrame:NSRectToCGRect([_metal_view bounds])];
  [_metal_layer setContentsScale:[_window backingScaleFactor]];
  [_metal_layer
      setDrawableSize:CGSizeMake(LogicalPixel(window_width,
                                              [_window backingScaleFactor]),
                                 LogicalPixel(window_height,
                                              [_window backingScaleFactor]))];
  [_metal_view setLayer:_metal_layer];
  [content_view addSubview:_metal_view];
  [_metal_view release];

  [_window setContentView:content_view];
  [content_view release];

  phenotype::ui::View root_view =
      _spec.content ? _spec.content() : phenotype::ui::empty();
  _renderer = std::make_unique<DawnButtonRenderer>();
  if (!_renderer->Initialize(
          _metal_layer, PixelSize(window_width, [_window backingScaleFactor]),
          PixelSize(window_height, [_window backingScaleFactor]),
          std::move(root_view))) {
    _renderer.reset();
    [NSApp terminate:nil];
    return;
  }

  [_window makeKeyAndOrderFront:nil];
  [NSApp activate];

  _render_timer =
      [NSTimer scheduledTimerWithTimeInterval:(1.0 / 60.0)
                                       target:self
                                     selector:@selector(renderFrame:)
                                     userInfo:nil
                                      repeats:YES];
  _renderer->Render();
}

- (void)renderFrame:(NSTimer *)timer {
  (void)timer;

  CGFloat scale = [_window backingScaleFactor];
  NSSize bounds = [_metal_view bounds].size;
  CGSize drawable_size =
      CGSizeMake(bounds.width * scale, bounds.height * scale);
  [_metal_layer setFrame:NSRectToCGRect([_metal_view bounds])];
  [_metal_layer setContentsScale:scale];
  [_metal_layer setDrawableSize:drawable_size];

  if (_renderer) {
    _renderer->Resize(PixelSize(bounds.width, scale),
                      PixelSize(bounds.height, scale));
    _renderer->Render();
  }
}

- (BOOL)applicationShouldTerminateAfterLastWindowClosed:
    (NSApplication *)sender {
  (void)sender;
  return YES;
}

- (void)dealloc {
  [_render_timer invalidate];
  _renderer.reset();
  [_window release];
  [super dealloc];
}

@end

extern "C" int phenotype_macos_app_run(int argc, char *argv[],
                                       phenotype::macos::window::Spec *spec) {
  (void)argc;
  (void)argv;

  @autoreleasepool {
    NSApplication *application = [NSApplication sharedApplication];
    AppDelegate *delegate = [[AppDelegate alloc] initWithSpec:std::move(*spec)];

    [application setActivationPolicy:NSApplicationActivationPolicyRegular];
    [application setDelegate:delegate];
    [application run];

    [delegate release];
  }

  return 0;
}
