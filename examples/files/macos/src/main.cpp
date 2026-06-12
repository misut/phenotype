#import <AppKit/AppKit.h>
#import <QuartzCore/CAMetalLayer.h>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <webgpu/webgpu_cpp.h>

namespace {

constexpr uint32_t kInitialWidth = 960;
constexpr uint32_t kInitialHeight = 640;

constexpr char kButtonShader[] = R"wgsl(
struct VertexOut {
    @builtin(position) position : vec4f,
    @location(0) local : vec2f,
};

@vertex
fn vertexMain(@builtin(vertex_index) index : u32) -> VertexOut {
    let positions = array<vec2f, 6>(
        vec2f(-0.42, -0.12),
        vec2f(-0.42,  0.12),
        vec2f( 0.42, -0.12),
        vec2f( 0.42, -0.12),
        vec2f(-0.42,  0.12),
        vec2f( 0.42,  0.12),
    );
    let local = array<vec2f, 6>(
        vec2f(0.0, 0.0),
        vec2f(0.0, 1.0),
        vec2f(1.0, 0.0),
        vec2f(1.0, 0.0),
        vec2f(0.0, 1.0),
        vec2f(1.0, 1.0),
    );

    var out : VertexOut;
    out.position = vec4f(positions[index], 0.0, 1.0);
    out.local = local[index];
    return out;
}

fn roundedButtonDistance(local : vec2f) -> f32 {
    let half_size = vec2f(0.5, 0.5);
    let radius = 0.18;
    let q = abs(local - half_size) - (half_size - vec2f(radius));
    return length(max(q, vec2f(0.0))) + min(max(q.x, q.y), 0.0) - radius;
}

@fragment
fn fragmentMain(in : VertexOut) -> @location(0) vec4f {
    let edge = roundedButtonDistance(in.local);
    if (edge > 0.0) {
        discard;
    }

    let highlight = 0.10 * (1.0 - in.local.y);
    return vec4f(0.12 + highlight, 0.32 + highlight, 0.78 + highlight, 1.0);
}
)wgsl";

uint32_t PixelSize(CGFloat points, CGFloat scale) {
    double pixels = static_cast<double>(points * scale);
    if (pixels < 1.0) {
        return 1;
    }
    return static_cast<uint32_t>(pixels);
}

NSRect InitialWindowVisibleFrame() {
    NSArray<NSScreen*>* screens = [NSScreen screens];
    for (NSScreen* screen in screens) {
        NSRect frame = [screen visibleFrame];
        if (frame.origin.x == 0.0 && frame.origin.y == 0.0) {
            return frame;
        }
    }

    NSPoint mouse = [NSEvent mouseLocation];
    for (NSScreen* screen in screens) {
        if (NSPointInRect(mouse, [screen frame])) {
            return [screen visibleFrame];
        }
    }

    NSScreen* screen = [NSScreen mainScreen];
    if (screen) {
        return [screen visibleFrame];
    }
    return NSMakeRect(0.0, 0.0, kInitialWidth, kInitialHeight);
}

class DawnButtonRenderer {
public:
    bool Initialize(CAMetalLayer* layer, uint32_t width, uint32_t height) {
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
        if (surface_texture.status != wgpu::SurfaceGetCurrentTextureStatus::SuccessOptimal &&
            surface_texture.status != wgpu::SurfaceGetCurrentTextureStatus::SuccessSuboptimal) {
            return;
        }

        wgpu::TextureView backbuffer = surface_texture.texture.CreateView();

        wgpu::RenderPassColorAttachment color_attachment;
        color_attachment.view = backbuffer;
        color_attachment.loadOp = wgpu::LoadOp::Clear;
        color_attachment.storeOp = wgpu::StoreOp::Store;
        color_attachment.clearValue = {0.94, 0.95, 0.97, 1.0};

        wgpu::RenderPassDescriptor render_pass_descriptor;
        render_pass_descriptor.colorAttachmentCount = 1;
        render_pass_descriptor.colorAttachments = &color_attachment;

        wgpu::CommandEncoder encoder = _device.CreateCommandEncoder();
        wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&render_pass_descriptor);
        pass.SetPipeline(_pipeline);
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
            &options,
            wgpu::CallbackMode::WaitAnyOnly,
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
        descriptor.SetUncapturedErrorCallback(
            [](const wgpu::Device&, wgpu::ErrorType error_type,
               wgpu::StringView message) {
                std::fprintf(stderr, "Dawn device error (%u): %.*s\n",
                             static_cast<unsigned>(error_type),
                             static_cast<int>(message.length), message.data);
            });

        bool resolved = false;
        bool succeeded = false;
        wgpu::Future future = _adapter.RequestDevice(
            &descriptor,
            wgpu::CallbackMode::WaitAnyOnly,
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

        wgpu::SurfaceConfiguration configuration;
        configuration.device = _device;
        configuration.format = _format;
        configuration.usage = wgpu::TextureUsage::RenderAttachment;
        configuration.width = _width;
        configuration.height = _height;
        configuration.presentMode = wgpu::PresentMode::Fifo;
        configuration.alphaMode = wgpu::CompositeAlphaMode::Auto;
        _surface.Configure(&configuration);
        return true;
    }

    void CreatePipeline() {
        wgpu::ShaderSourceWGSL wgsl;
        wgsl.code = kButtonShader;

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
        pipeline_descriptor.vertex.module = shader;
        pipeline_descriptor.vertex.entryPoint = "vertexMain";
        pipeline_descriptor.primitive.topology = wgpu::PrimitiveTopology::TriangleList;
        pipeline_descriptor.fragment = &fragment;
        pipeline_descriptor.multisample.count = 1;

        _pipeline = _device.CreateRenderPipeline(&pipeline_descriptor);
    }

    wgpu::Instance _instance;
    wgpu::Surface _surface;
    wgpu::Adapter _adapter;
    wgpu::Device _device;
    wgpu::TextureFormat _format = wgpu::TextureFormat::Undefined;
    wgpu::RenderPipeline _pipeline;
    uint32_t _width = 0;
    uint32_t _height = 0;
};

}  // namespace

@interface AppDelegate : NSObject <NSApplicationDelegate> {
    NSWindow* _window;
    NSTimer* _render_timer;
    CAMetalLayer* _metal_layer;
    DawnButtonRenderer* _renderer;
}
@end

@implementation AppDelegate

- (void)applicationDidFinishLaunching:(NSNotification*)notification {
    (void)notification;

    NSRect content_rect = NSMakeRect(0.0, 0.0, kInitialWidth, kInitialHeight);
    NSWindowStyleMask style_mask = NSWindowStyleMaskTitled |
        NSWindowStyleMaskClosable |
        NSWindowStyleMaskMiniaturizable |
        NSWindowStyleMaskResizable;

    _window = [[NSWindow alloc] initWithContentRect:content_rect
                                          styleMask:style_mask
                                            backing:NSBackingStoreBuffered
                                              defer:NO];
    [_window setTitle:@"Files"];

    NSRect visible_frame = InitialWindowVisibleFrame();
    [_window setFrameOrigin:NSMakePoint(
        NSMidX(visible_frame) - (kInitialWidth / 2.0),
        NSMidY(visible_frame) - (kInitialHeight / 2.0))];

    NSView* content_view = [[NSView alloc] initWithFrame:content_rect];
    [content_view setWantsLayer:YES];

    _metal_layer = [CAMetalLayer layer];
    [_metal_layer setContentsScale:[_window backingScaleFactor]];
    [_metal_layer setDrawableSize:CGSizeMake(
        kInitialWidth * [_window backingScaleFactor],
        kInitialHeight * [_window backingScaleFactor])];
    [content_view setLayer:_metal_layer];
    [_window setContentView:content_view];
    [content_view release];

    _renderer = new DawnButtonRenderer();
    if (!_renderer->Initialize(_metal_layer,
                               PixelSize(kInitialWidth, [_window backingScaleFactor]),
                               PixelSize(kInitialHeight, [_window backingScaleFactor]))) {
        delete _renderer;
        _renderer = nullptr;
        [NSApp terminate:nil];
        return;
    }

    [_window makeKeyAndOrderFront:nil];
    [NSApp activate];

    _render_timer = [NSTimer scheduledTimerWithTimeInterval:(1.0 / 60.0)
                                                     target:self
                                                   selector:@selector(renderFrame:)
                                                    userInfo:nil
                                                    repeats:YES];
    _renderer->Render();
}

- (void)renderFrame:(NSTimer*)timer {
    (void)timer;

    CGFloat scale = [_window backingScaleFactor];
    NSSize bounds = [[_window contentView] bounds].size;
    CGSize drawable_size = CGSizeMake(bounds.width * scale, bounds.height * scale);
    [_metal_layer setContentsScale:scale];
    [_metal_layer setDrawableSize:drawable_size];

    _renderer->Resize(PixelSize(bounds.width, scale), PixelSize(bounds.height, scale));
    _renderer->Render();
}

- (BOOL)applicationShouldTerminateAfterLastWindowClosed:
    (NSApplication*)sender {
    (void)sender;
    return YES;
}

- (void)dealloc {
    [_render_timer invalidate];
    delete _renderer;
    [_window release];
    [super dealloc];
}

@end

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    @autoreleasepool {
        NSApplication* application = [NSApplication sharedApplication];
        AppDelegate* delegate = [[AppDelegate alloc] init];

        [application setActivationPolicy:NSApplicationActivationPolicyRegular];
        [application setDelegate:delegate];
        [application run];

        [delegate release];
    }

    return 0;
}
