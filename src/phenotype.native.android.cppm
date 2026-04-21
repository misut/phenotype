// Android (Vulkan) platform backend for phenotype. Stage 2 scope: clear
// the swapchain image to the active phenotype theme background on every
// flush. Text / input / debug come from `phenotype.native.stub`.
//
// Vulkan state lives in `detail::g_renderer`; helpers in the detail
// namespace keep the module interface unit short so the Clang module
// codegen path stays simple.

module;

#if defined(__ANDROID__)
#include <cstdint>
#include <cstring>

#define VK_USE_PLATFORM_ANDROID_KHR 1
#include <vulkan/vulkan.h>
#include <android/native_window.h>
#include <android/log.h>
#endif

export module phenotype.native.android;

#if defined(__ANDROID__)
import std;
import phenotype;
import phenotype.native.platform;
import phenotype.native.shell;
import phenotype.native.stub;

namespace phenotype::native::detail {

constexpr char const ANDROID_LOG_TAG[] = "phenotype";

struct android_renderer {
    VkInstance instance;
    VkPhysicalDevice physical_device;
    VkDevice device;
    std::uint32_t queue_family_index;
    VkQueue queue;
    VkSurfaceKHR surface;
    VkSwapchainKHR swapchain;
    VkFormat swapchain_format;
    VkExtent2D swapchain_extent;
    std::vector<VkImage> swapchain_images;
    VkCommandPool command_pool;
    VkCommandBuffer command_buffer;
    VkSemaphore image_available;
    VkSemaphore render_finished;
    VkFence in_flight;
    ANativeWindow* window;
    bool initialized;
};

inline android_renderer g_renderer{};

inline bool vk_ok(VkResult r, char const* label) {
    if (r == VK_SUCCESS) return true;
    __android_log_print(ANDROID_LOG_ERROR, ANDROID_LOG_TAG,
                        "%s failed (VkResult=%d)", label, static_cast<int>(r));
    return false;
}

inline void destroy_swapchain_resources() {
    if (g_renderer.device == VK_NULL_HANDLE) return;
    if (g_renderer.in_flight != VK_NULL_HANDLE)
        vkWaitForFences(g_renderer.device, 1, &g_renderer.in_flight, VK_TRUE, UINT64_MAX);
    if (g_renderer.command_pool != VK_NULL_HANDLE && g_renderer.command_buffer != VK_NULL_HANDLE) {
        vkFreeCommandBuffers(g_renderer.device, g_renderer.command_pool,
                             1, &g_renderer.command_buffer);
        g_renderer.command_buffer = VK_NULL_HANDLE;
    }
    if (g_renderer.swapchain != VK_NULL_HANDLE) {
        vkDestroySwapchainKHR(g_renderer.device, g_renderer.swapchain, nullptr);
        g_renderer.swapchain = VK_NULL_HANDLE;
    }
    g_renderer.swapchain_images.clear();
}

inline bool create_swapchain() {
    VkSurfaceCapabilitiesKHR caps{};
    if (!vk_ok(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
                  g_renderer.physical_device, g_renderer.surface, &caps),
              "vkGetPhysicalDeviceSurfaceCapabilitiesKHR"))
        return false;

    std::uint32_t format_count = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(
        g_renderer.physical_device, g_renderer.surface, &format_count, nullptr);
    if (format_count == 0) return false;
    std::vector<VkSurfaceFormatKHR> formats(format_count);
    vkGetPhysicalDeviceSurfaceFormatsKHR(
        g_renderer.physical_device, g_renderer.surface, &format_count, formats.data());

    VkSurfaceFormatKHR picked = formats[0];
    for (auto const& f : formats) {
        if (f.format == VK_FORMAT_R8G8B8A8_UNORM
            && f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            picked = f;
            break;
        }
    }

    VkExtent2D extent = caps.currentExtent;
    if (extent.width == UINT32_MAX) {
        extent.width = static_cast<std::uint32_t>(
            ANativeWindow_getWidth(g_renderer.window));
        extent.height = static_cast<std::uint32_t>(
            ANativeWindow_getHeight(g_renderer.window));
    }

    std::uint32_t image_count = caps.minImageCount + 1;
    if (caps.maxImageCount > 0 && image_count > caps.maxImageCount)
        image_count = caps.maxImageCount;

    VkSwapchainCreateInfoKHR ci{};
    ci.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    ci.surface = g_renderer.surface;
    ci.minImageCount = image_count;
    ci.imageFormat = picked.format;
    ci.imageColorSpace = picked.colorSpace;
    ci.imageExtent = extent;
    ci.imageArrayLayers = 1;
    ci.imageUsage = VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    ci.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    ci.preTransform = caps.currentTransform;
    ci.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    ci.presentMode = VK_PRESENT_MODE_FIFO_KHR;
    ci.clipped = VK_TRUE;

    if (!vk_ok(vkCreateSwapchainKHR(g_renderer.device, &ci, nullptr,
                                    &g_renderer.swapchain),
              "vkCreateSwapchainKHR"))
        return false;

    g_renderer.swapchain_format = picked.format;
    g_renderer.swapchain_extent = extent;

    std::uint32_t n = 0;
    vkGetSwapchainImagesKHR(g_renderer.device, g_renderer.swapchain, &n, nullptr);
    g_renderer.swapchain_images.resize(n);
    vkGetSwapchainImagesKHR(g_renderer.device, g_renderer.swapchain, &n,
                            g_renderer.swapchain_images.data());

    VkCommandBufferAllocateInfo alloc{};
    alloc.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    alloc.commandPool = g_renderer.command_pool;
    alloc.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    alloc.commandBufferCount = 1;
    if (!vk_ok(vkAllocateCommandBuffers(g_renderer.device, &alloc,
                                        &g_renderer.command_buffer),
              "vkAllocateCommandBuffers"))
        return false;
    return true;
}

inline void renderer_init(native_surface_handle handle) {
    auto* window = static_cast<ANativeWindow*>(handle);
    if (g_renderer.initialized && g_renderer.window == window) return;
    if (g_renderer.initialized) destroy_swapchain_resources();
    g_renderer.window = window;
    if (!window) return;

    if (g_renderer.instance == VK_NULL_HANDLE) {
        VkApplicationInfo app{};
        app.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        app.pApplicationName = "phenotype";
        app.apiVersion = VK_API_VERSION_1_1;
        char const* exts[] = {
            VK_KHR_SURFACE_EXTENSION_NAME,
            VK_KHR_ANDROID_SURFACE_EXTENSION_NAME,
        };
        VkInstanceCreateInfo ici{};
        ici.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        ici.pApplicationInfo = &app;
        ici.enabledExtensionCount = 2;
        ici.ppEnabledExtensionNames = exts;
        if (!vk_ok(vkCreateInstance(&ici, nullptr, &g_renderer.instance),
                  "vkCreateInstance"))
            return;
    }

    if (g_renderer.surface != VK_NULL_HANDLE) {
        vkDestroySurfaceKHR(g_renderer.instance, g_renderer.surface, nullptr);
        g_renderer.surface = VK_NULL_HANDLE;
    }

    VkAndroidSurfaceCreateInfoKHR sci{};
    sci.sType = VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR;
    sci.window = window;
    if (!vk_ok(vkCreateAndroidSurfaceKHR(g_renderer.instance, &sci, nullptr,
                                         &g_renderer.surface),
              "vkCreateAndroidSurfaceKHR"))
        return;

    if (g_renderer.device == VK_NULL_HANDLE) {
        std::uint32_t n = 0;
        vkEnumeratePhysicalDevices(g_renderer.instance, &n, nullptr);
        if (n == 0) return;
        std::vector<VkPhysicalDevice> devs(n);
        vkEnumeratePhysicalDevices(g_renderer.instance, &n, devs.data());
        g_renderer.physical_device = devs[0];

        std::uint32_t q = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(g_renderer.physical_device, &q, nullptr);
        std::vector<VkQueueFamilyProperties> fams(q);
        vkGetPhysicalDeviceQueueFamilyProperties(
            g_renderer.physical_device, &q, fams.data());
        bool found = false;
        for (std::uint32_t i = 0; i < q; ++i) {
            if (fams[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
                VkBool32 present = VK_FALSE;
                vkGetPhysicalDeviceSurfaceSupportKHR(
                    g_renderer.physical_device, i, g_renderer.surface, &present);
                if (present) {
                    g_renderer.queue_family_index = i;
                    found = true;
                    break;
                }
            }
        }
        if (!found) return;

        float priority = 1.0f;
        VkDeviceQueueCreateInfo qci{};
        qci.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        qci.queueFamilyIndex = g_renderer.queue_family_index;
        qci.queueCount = 1;
        qci.pQueuePriorities = &priority;

        char const* dev_exts[] = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
        VkDeviceCreateInfo dci{};
        dci.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        dci.queueCreateInfoCount = 1;
        dci.pQueueCreateInfos = &qci;
        dci.enabledExtensionCount = 1;
        dci.ppEnabledExtensionNames = dev_exts;
        if (!vk_ok(vkCreateDevice(g_renderer.physical_device, &dci, nullptr,
                                  &g_renderer.device),
                  "vkCreateDevice"))
            return;
        vkGetDeviceQueue(g_renderer.device, g_renderer.queue_family_index, 0,
                         &g_renderer.queue);

        VkCommandPoolCreateInfo pci{};
        pci.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        pci.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        pci.queueFamilyIndex = g_renderer.queue_family_index;
        vk_ok(vkCreateCommandPool(g_renderer.device, &pci, nullptr,
                                  &g_renderer.command_pool),
              "vkCreateCommandPool");

        VkSemaphoreCreateInfo semi{};
        semi.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        vk_ok(vkCreateSemaphore(g_renderer.device, &semi, nullptr,
                                &g_renderer.image_available),
              "vkCreateSemaphore");
        vk_ok(vkCreateSemaphore(g_renderer.device, &semi, nullptr,
                                &g_renderer.render_finished),
              "vkCreateSemaphore");

        VkFenceCreateInfo fi{};
        fi.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fi.flags = VK_FENCE_CREATE_SIGNALED_BIT;
        vk_ok(vkCreateFence(g_renderer.device, &fi, nullptr, &g_renderer.in_flight),
              "vkCreateFence");
    }

    if (!create_swapchain()) return;
    g_renderer.initialized = true;
    __android_log_print(ANDROID_LOG_INFO, ANDROID_LOG_TAG,
                        "Vulkan renderer ready (%ux%u)",
                        g_renderer.swapchain_extent.width,
                        g_renderer.swapchain_extent.height);
}

inline void renderer_flush(unsigned char const*, unsigned int) {
    if (!g_renderer.initialized) return;

    vkWaitForFences(g_renderer.device, 1, &g_renderer.in_flight, VK_TRUE, UINT64_MAX);

    std::uint32_t idx = 0;
    auto r = vkAcquireNextImageKHR(
        g_renderer.device, g_renderer.swapchain, UINT64_MAX,
        g_renderer.image_available, VK_NULL_HANDLE, &idx);
    if (r == VK_ERROR_OUT_OF_DATE_KHR || r == VK_SUBOPTIMAL_KHR) {
        destroy_swapchain_resources();
        create_swapchain();
        return;
    }
    if (!vk_ok(r, "vkAcquireNextImageKHR")) return;

    vkResetFences(g_renderer.device, 1, &g_renderer.in_flight);
    vkResetCommandBuffer(g_renderer.command_buffer, 0);

    VkCommandBufferBeginInfo bi{};
    bi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(g_renderer.command_buffer, &bi);

    VkImageMemoryBarrier b1{};
    b1.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    b1.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    b1.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    b1.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    b1.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    b1.image = g_renderer.swapchain_images[idx];
    b1.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
    b1.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    vkCmdPipelineBarrier(g_renderer.command_buffer,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
        0, 0, nullptr, 0, nullptr, 1, &b1);

    auto const& theme = ::phenotype::current_theme();
    VkClearColorValue clear{};
    clear.float32[0] = static_cast<float>(theme.background.r) / 255.0f;
    clear.float32[1] = static_cast<float>(theme.background.g) / 255.0f;
    clear.float32[2] = static_cast<float>(theme.background.b) / 255.0f;
    clear.float32[3] = static_cast<float>(theme.background.a) / 255.0f;
    VkImageSubresourceRange range{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
    vkCmdClearColorImage(g_renderer.command_buffer,
        g_renderer.swapchain_images[idx],
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clear, 1, &range);

    VkImageMemoryBarrier b2 = b1;
    b2.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    b2.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    b2.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    b2.dstAccessMask = 0;
    vkCmdPipelineBarrier(g_renderer.command_buffer,
        VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
        0, 0, nullptr, 0, nullptr, 1, &b2);

    vkEndCommandBuffer(g_renderer.command_buffer);

    VkPipelineStageFlags wait = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    VkSubmitInfo si{};
    si.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    si.waitSemaphoreCount = 1;
    si.pWaitSemaphores = &g_renderer.image_available;
    si.pWaitDstStageMask = &wait;
    si.commandBufferCount = 1;
    si.pCommandBuffers = &g_renderer.command_buffer;
    si.signalSemaphoreCount = 1;
    si.pSignalSemaphores = &g_renderer.render_finished;
    vkQueueSubmit(g_renderer.queue, 1, &si, g_renderer.in_flight);

    VkPresentInfoKHR pi{};
    pi.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    pi.waitSemaphoreCount = 1;
    pi.pWaitSemaphores = &g_renderer.render_finished;
    pi.swapchainCount = 1;
    pi.pSwapchains = &g_renderer.swapchain;
    pi.pImageIndices = &idx;
    auto pr = vkQueuePresentKHR(g_renderer.queue, &pi);
    if (pr == VK_ERROR_OUT_OF_DATE_KHR || pr == VK_SUBOPTIMAL_KHR) {
        destroy_swapchain_resources();
        create_swapchain();
    }
}

inline void renderer_shutdown() {
    if (g_renderer.device != VK_NULL_HANDLE) vkDeviceWaitIdle(g_renderer.device);
    destroy_swapchain_resources();
    if (g_renderer.in_flight != VK_NULL_HANDLE) {
        vkDestroyFence(g_renderer.device, g_renderer.in_flight, nullptr);
        g_renderer.in_flight = VK_NULL_HANDLE;
    }
    if (g_renderer.render_finished != VK_NULL_HANDLE) {
        vkDestroySemaphore(g_renderer.device, g_renderer.render_finished, nullptr);
        g_renderer.render_finished = VK_NULL_HANDLE;
    }
    if (g_renderer.image_available != VK_NULL_HANDLE) {
        vkDestroySemaphore(g_renderer.device, g_renderer.image_available, nullptr);
        g_renderer.image_available = VK_NULL_HANDLE;
    }
    if (g_renderer.command_pool != VK_NULL_HANDLE) {
        vkDestroyCommandPool(g_renderer.device, g_renderer.command_pool, nullptr);
        g_renderer.command_pool = VK_NULL_HANDLE;
    }
    if (g_renderer.device != VK_NULL_HANDLE) {
        vkDestroyDevice(g_renderer.device, nullptr);
        g_renderer.device = VK_NULL_HANDLE;
    }
    if (g_renderer.surface != VK_NULL_HANDLE) {
        vkDestroySurfaceKHR(g_renderer.instance, g_renderer.surface, nullptr);
        g_renderer.surface = VK_NULL_HANDLE;
    }
    if (g_renderer.instance != VK_NULL_HANDLE) {
        vkDestroyInstance(g_renderer.instance, nullptr);
        g_renderer.instance = VK_NULL_HANDLE;
    }
    g_renderer.window = nullptr;
    g_renderer.initialized = false;
}

inline std::optional<unsigned int> renderer_hit_test(float, float, float) {
    return std::nullopt;
}

inline void android_open_url(char const* url, unsigned int len) {
    __android_log_print(ANDROID_LOG_INFO, ANDROID_LOG_TAG,
                        "open_url ignored: %.*s", static_cast<int>(len), url);
}

inline platform_api build_android_platform() {
    auto api = make_stub_platform(
        "android",
        "[phenotype-native] Android Vulkan backend (stage 2: clear-color only)");
    api.renderer = {
        renderer_init,
        renderer_flush,
        renderer_shutdown,
        renderer_hit_test,
    };
    api.open_url = android_open_url;
    return api;
}

} // namespace phenotype::native::detail

export namespace phenotype::native {

inline platform_api const& android_platform() {
    static platform_api api = detail::build_android_platform();
    return api;
}

} // namespace phenotype::native

// C ABI for the examples/android GameActivity glue. Keeping this entry
// surface as plain C avoids forcing the Gradle-built caller to import
// the phenotype modules (which would require sharing exon's PCM output
// across the split build). The static_host + bound platform match what
// `phenotype::native::detail::run_host` would set up for a templated
// app; Stage 3+ will swap in the full run_host once we have something
// worth drawing beyond the clear color.

namespace phenotype::native::detail {

inline native_host g_android_host{};

inline void bind_platform_once() {
    if (!g_android_host.platform)
        g_android_host.platform = &android_platform();
}

} // namespace phenotype::native::detail

extern "C" {

__attribute__((visibility("default")))
void phenotype_android_attach_surface(void* native_window) {
    namespace d = phenotype::native::detail;
    d::bind_platform_once();
    d::g_android_host.window = native_window;
    if (d::g_android_host.platform->renderer.init)
        d::g_android_host.platform->renderer.init(d::g_android_host.window);
    if (d::g_android_host.platform->input.attach)
        d::g_android_host.platform->input.attach(d::g_android_host.window, nullptr);
}

__attribute__((visibility("default")))
void phenotype_android_detach_surface(void) {
    namespace d = phenotype::native::detail;
    if (!d::g_android_host.platform) return;
    if (d::g_android_host.platform->input.detach)
        d::g_android_host.platform->input.detach();
    if (d::g_android_host.platform->renderer.shutdown)
        d::g_android_host.platform->renderer.shutdown();
    d::g_android_host.window = nullptr;
}

__attribute__((visibility("default")))
void phenotype_android_draw_frame(void) {
    namespace d = phenotype::native::detail;
    if (!d::g_android_host.platform || !d::g_android_host.window) return;
    if (d::g_android_host.platform->renderer.flush)
        d::g_android_host.platform->renderer.flush(d::g_android_host.buffer, 0);
}

__attribute__((visibility("default")))
char const* phenotype_android_startup_message(void) {
    namespace d = phenotype::native::detail;
    d::bind_platform_once();
    return d::g_android_host.platform->startup_message;
}

} // extern "C"
#endif
