#include "renderer.h"

#include <cstdio>
#include <cstring>
#include <variant>
#include <vector>

#include <webgpu/webgpu_cpp.h>
#include <webgpu/webgpu_glfw.h>
#include <GLFW/glfw3.h>

// Access the cmd buffer globals from phenotype.paint via C linkage.
extern "C" {
    extern unsigned char phenotype_cmd_buf[];
    extern unsigned int phenotype_cmd_len;
}

// phenotype::parse_commands and command types
import phenotype;

namespace native::renderer {

static wgpu::Instance g_instance;
static wgpu::Device   g_device;
static wgpu::Surface  g_surface;
static wgpu::Queue    g_queue;
static GLFWwindow*    g_window = nullptr;

static void on_device_error(wgpu::Device const&, wgpu::ErrorType type, wgpu::StringView message) {
    std::fprintf(stderr, "[Dawn] error %d: %.*s\n",
        static_cast<int>(type),
        static_cast<int>(message.length), message.data);
}

void init(GLFWwindow* window) {
    g_window = window;

    // Create WebGPU instance with timed-wait support for synchronous
    // adapter/device request callbacks.
    wgpu::InstanceFeatureName requiredFeatures[] = {
        wgpu::InstanceFeatureName::TimedWaitAny,
    };
    wgpu::InstanceDescriptor instanceDesc{};
    instanceDesc.requiredFeatureCount = 1;
    instanceDesc.requiredFeatures = requiredFeatures;
    g_instance = wgpu::CreateInstance(&instanceDesc);
    if (!g_instance) {
        std::fprintf(stderr, "[phenotype-native] failed to create WebGPU instance\n");
        return;
    }

    // Create surface from GLFW window (Dawn handles Metal/D3D12/Vulkan)
    g_surface = wgpu::glfw::CreateSurfaceForWindow(g_instance, window);
    if (!g_surface) {
        std::fprintf(stderr, "[phenotype-native] failed to create surface\n");
        return;
    }

    // Request adapter
    wgpu::RequestAdapterOptions adapterOpts{};
    adapterOpts.compatibleSurface = g_surface;

    wgpu::Adapter adapter;
    auto adapterFuture = g_instance.RequestAdapter(
        &adapterOpts,
        wgpu::CallbackMode::WaitAnyOnly,
        [&](wgpu::RequestAdapterStatus status, wgpu::Adapter result, wgpu::StringView) {
            if (status == wgpu::RequestAdapterStatus::Success)
                adapter = std::move(result);
        });
    g_instance.WaitAny(adapterFuture, UINT64_MAX);

    if (!adapter) {
        std::fprintf(stderr, "[phenotype-native] no GPU adapter available\n");
        return;
    }

    // Request device
    wgpu::DeviceDescriptor deviceDesc{};
    deviceDesc.SetUncapturedErrorCallback(on_device_error);

    auto deviceFuture = adapter.RequestDevice(
        &deviceDesc,
        wgpu::CallbackMode::WaitAnyOnly,
        [&](wgpu::RequestDeviceStatus status, wgpu::Device result, wgpu::StringView) {
            if (status == wgpu::RequestDeviceStatus::Success)
                g_device = std::move(result);
        });
    g_instance.WaitAny(deviceFuture, UINT64_MAX);

    if (!g_device) {
        std::fprintf(stderr, "[phenotype-native] no GPU device available\n");
        return;
    }

    g_queue = g_device.GetQueue();

    // Configure surface
    int w, h;
    glfwGetFramebufferSize(window, &w, &h);

    wgpu::SurfaceConfiguration config{};
    config.device = g_device;
    config.format = wgpu::TextureFormat::BGRA8Unorm;
    config.width  = static_cast<uint32_t>(w);
    config.height = static_cast<uint32_t>(h);
    config.presentMode = wgpu::PresentMode::Fifo;
    g_surface.Configure(&config);

    std::printf("[phenotype-native] Dawn initialized (%dx%d)\n", w, h);
}

void flush() {
    if (phenotype_cmd_len == 0) return;
    if (!g_device || !g_surface) return;

    // Parse cmd buffer to find the clear color
    auto cmds = phenotype::parse_commands(phenotype_cmd_buf, phenotype_cmd_len);

    double cr = 0.98, cg = 0.98, cb = 0.98, ca = 1.0;
    for (auto& cmd : cmds) {
        if (auto* c = std::get_if<phenotype::ClearCmd>(&cmd)) {
            cr = c->color.r / 255.0;
            cg = c->color.g / 255.0;
            cb = c->color.b / 255.0;
            ca = c->color.a / 255.0;
            break;
        }
    }

    // Reconfigure surface on resize
    int w, h;
    glfwGetFramebufferSize(g_window, &w, &h);
    if (w == 0 || h == 0) return; // minimized

    wgpu::SurfaceConfiguration config{};
    config.device = g_device;
    config.format = wgpu::TextureFormat::BGRA8Unorm;
    config.width  = static_cast<uint32_t>(w);
    config.height = static_cast<uint32_t>(h);
    config.presentMode = wgpu::PresentMode::Fifo;
    g_surface.Configure(&config);

    // Get current texture
    wgpu::SurfaceTexture surfaceTexture;
    g_surface.GetCurrentTexture(&surfaceTexture);
    if (surfaceTexture.status != wgpu::SurfaceGetCurrentTextureStatus::SuccessOptimal &&
        surfaceTexture.status != wgpu::SurfaceGetCurrentTextureStatus::SuccessSuboptimal)
        return;

    wgpu::TextureView view = surfaceTexture.texture.CreateView();

    // Render pass — clear only for Phase A
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
    pass.End();

    auto commands = encoder.Finish();
    g_queue.Submit(1, &commands);
    g_surface.Present();
}

void shutdown() {
    g_queue = nullptr;
    g_surface = nullptr;
    g_device = nullptr;
    g_instance = nullptr;
}

} // namespace native::renderer
