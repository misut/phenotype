// Android (Vulkan) platform backend for phenotype. Stage 2 scope:
// clear the swapchain image to the active phenotype theme background on
// every flush; no draw-command parsing yet. Text / input / debug come
// from `phenotype.native.stub` so every `platform_api` slot is populated
// and downstream consumers don't need to null-check.
//
// GameActivity / android_main live in `examples/android/` (Gradle-built).
// This module only needs headers that ship with the NDK: <vulkan/vulkan.h>,
// <android/native_window.h>, <android/log.h>.

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
import phenotype.native.stub;

namespace phenotype::native::detail {

inline constexpr char const* ANDROID_LOG_TAG = "phenotype";

#define PH_ANDROID_LOG_I(fmt, ...) \
    ::__android_log_print(ANDROID_LOG_INFO,  ANDROID_LOG_TAG, fmt, ##__VA_ARGS__)
#define PH_ANDROID_LOG_W(fmt, ...) \
    ::__android_log_print(ANDROID_LOG_WARN,  ANDROID_LOG_TAG, fmt, ##__VA_ARGS__)
#define PH_ANDROID_LOG_E(fmt, ...) \
    ::__android_log_print(ANDROID_LOG_ERROR, ANDROID_LOG_TAG, fmt, ##__VA_ARGS__)

struct android_renderer {
    VkInstance instance = VK_NULL_HANDLE;
    VkPhysicalDevice physical_device = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE;
    uint32_t queue_family_index = 0;
    VkQueue queue = VK_NULL_HANDLE;
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    VkSwapchainKHR swapchain = VK_NULL_HANDLE;
    VkFormat swapchain_format = VK_FORMAT_UNDEFINED;
    VkExtent2D swapchain_extent{};
    std::vector<VkImage> swapchain_images;
    VkCommandPool command_pool = VK_NULL_HANDLE;
    VkCommandBuffer command_buffer = VK_NULL_HANDLE;
    VkSemaphore image_available = VK_NULL_HANDLE;
    VkSemaphore render_finished = VK_NULL_HANDLE;
    VkFence in_flight = VK_NULL_HANDLE;
    ANativeWindow* window = nullptr;
    bool initialized = false;
};

inline android_renderer g_renderer;

inline bool vk_check(VkResult result, char const* label) {
    if (result == VK_SUCCESS)
        return true;
    PH_ANDROID_LOG_E("%s failed (VkResult=%d)", label, static_cast<int>(result));
    return false;
}

inline void destroy_swapchain_resources() {
    if (g_renderer.device == VK_NULL_HANDLE)
        return;
    if (g_renderer.in_flight != VK_NULL_HANDLE) {
        vkWaitForFences(g_renderer.device, 1, &g_renderer.in_flight, VK_TRUE, UINT64_MAX);
    }
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
    // Query capabilities and pick a format.
    VkSurfaceCapabilitiesKHR caps{};
    if (!vk_check(
            vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
                g_renderer.physical_device, g_renderer.surface, &caps),
            "vkGetPhysicalDeviceSurfaceCapabilitiesKHR"))
        return false;

    uint32_t format_count = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(
        g_renderer.physical_device, g_renderer.surface, &format_count, nullptr);
    if (format_count == 0) {
        PH_ANDROID_LOG_E("no surface formats available");
        return false;
    }
    std::vector<VkSurfaceFormatKHR> formats(format_count);
    vkGetPhysicalDeviceSurfaceFormatsKHR(
        g_renderer.physical_device, g_renderer.surface, &format_count, formats.data());

    auto picked_format = formats[0];
    for (auto const& fmt : formats) {
        if (fmt.format == VK_FORMAT_R8G8B8A8_UNORM
            && fmt.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            picked_format = fmt;
            break;
        }
    }

    VkExtent2D extent = caps.currentExtent;
    if (extent.width == UINT32_MAX) {
        extent.width = static_cast<uint32_t>(ANativeWindow_getWidth(g_renderer.window));
        extent.height = static_cast<uint32_t>(ANativeWindow_getHeight(g_renderer.window));
    }

    uint32_t image_count = caps.minImageCount + 1;
    if (caps.maxImageCount > 0 && image_count > caps.maxImageCount)
        image_count = caps.maxImageCount;

    VkSwapchainCreateInfoKHR create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    create_info.surface = g_renderer.surface;
    create_info.minImageCount = image_count;
    create_info.imageFormat = picked_format.format;
    create_info.imageColorSpace = picked_format.colorSpace;
    create_info.imageExtent = extent;
    create_info.imageArrayLayers = 1;
    create_info.imageUsage = VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    create_info.preTransform = caps.currentTransform;
    create_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    create_info.presentMode = VK_PRESENT_MODE_FIFO_KHR;
    create_info.clipped = VK_TRUE;
    create_info.oldSwapchain = VK_NULL_HANDLE;

    if (!vk_check(
            vkCreateSwapchainKHR(g_renderer.device, &create_info, nullptr,
                                 &g_renderer.swapchain),
            "vkCreateSwapchainKHR"))
        return false;

    g_renderer.swapchain_format = picked_format.format;
    g_renderer.swapchain_extent = extent;

    uint32_t actual_count = 0;
    vkGetSwapchainImagesKHR(g_renderer.device, g_renderer.swapchain, &actual_count, nullptr);
    g_renderer.swapchain_images.resize(actual_count);
    vkGetSwapchainImagesKHR(g_renderer.device, g_renderer.swapchain, &actual_count,
                            g_renderer.swapchain_images.data());

    VkCommandBufferAllocateInfo cb_alloc{};
    cb_alloc.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cb_alloc.commandPool = g_renderer.command_pool;
    cb_alloc.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cb_alloc.commandBufferCount = 1;
    if (!vk_check(
            vkAllocateCommandBuffers(g_renderer.device, &cb_alloc, &g_renderer.command_buffer),
            "vkAllocateCommandBuffers"))
        return false;

    return true;
}

inline void renderer_init(native_surface_handle handle) {
    auto* window = static_cast<ANativeWindow*>(handle);
    if (g_renderer.initialized && g_renderer.window == window)
        return;
    if (g_renderer.initialized)
        destroy_swapchain_resources();

    g_renderer.window = window;
    if (!window) {
        PH_ANDROID_LOG_W("renderer_init called with null window");
        return;
    }

    if (g_renderer.instance == VK_NULL_HANDLE) {
        VkApplicationInfo app_info{};
        app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        app_info.pApplicationName = "phenotype";
        app_info.apiVersion = VK_API_VERSION_1_1;

        char const* instance_extensions[] = {
            VK_KHR_SURFACE_EXTENSION_NAME,
            VK_KHR_ANDROID_SURFACE_EXTENSION_NAME,
        };

        VkInstanceCreateInfo inst_info{};
        inst_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        inst_info.pApplicationInfo = &app_info;
        inst_info.enabledExtensionCount =
            static_cast<uint32_t>(std::size(instance_extensions));
        inst_info.ppEnabledExtensionNames = instance_extensions;

        if (!vk_check(vkCreateInstance(&inst_info, nullptr, &g_renderer.instance),
                      "vkCreateInstance"))
            return;
    }

    if (g_renderer.surface != VK_NULL_HANDLE) {
        vkDestroySurfaceKHR(g_renderer.instance, g_renderer.surface, nullptr);
        g_renderer.surface = VK_NULL_HANDLE;
    }

    VkAndroidSurfaceCreateInfoKHR surf_info{};
    surf_info.sType = VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR;
    surf_info.window = window;
    if (!vk_check(vkCreateAndroidSurfaceKHR(g_renderer.instance, &surf_info, nullptr,
                                            &g_renderer.surface),
                  "vkCreateAndroidSurfaceKHR"))
        return;

    if (g_renderer.device == VK_NULL_HANDLE) {
        uint32_t device_count = 0;
        vkEnumeratePhysicalDevices(g_renderer.instance, &device_count, nullptr);
        if (device_count == 0) {
            PH_ANDROID_LOG_E("no Vulkan physical devices");
            return;
        }
        std::vector<VkPhysicalDevice> devices(device_count);
        vkEnumeratePhysicalDevices(g_renderer.instance, &device_count, devices.data());
        g_renderer.physical_device = devices[0];

        uint32_t family_count = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(
            g_renderer.physical_device, &family_count, nullptr);
        std::vector<VkQueueFamilyProperties> families(family_count);
        vkGetPhysicalDeviceQueueFamilyProperties(
            g_renderer.physical_device, &family_count, families.data());

        bool found_family = false;
        for (uint32_t i = 0; i < family_count; ++i) {
            if (families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
                VkBool32 present_support = VK_FALSE;
                vkGetPhysicalDeviceSurfaceSupportKHR(
                    g_renderer.physical_device, i, g_renderer.surface, &present_support);
                if (present_support) {
                    g_renderer.queue_family_index = i;
                    found_family = true;
                    break;
                }
            }
        }
        if (!found_family) {
            PH_ANDROID_LOG_E("no graphics+present queue family");
            return;
        }

        float priority = 1.0f;
        VkDeviceQueueCreateInfo queue_info{};
        queue_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queue_info.queueFamilyIndex = g_renderer.queue_family_index;
        queue_info.queueCount = 1;
        queue_info.pQueuePriorities = &priority;

        char const* device_extensions[] = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
        VkDeviceCreateInfo dev_info{};
        dev_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        dev_info.queueCreateInfoCount = 1;
        dev_info.pQueueCreateInfos = &queue_info;
        dev_info.enabledExtensionCount =
            static_cast<uint32_t>(std::size(device_extensions));
        dev_info.ppEnabledExtensionNames = device_extensions;

        if (!vk_check(vkCreateDevice(g_renderer.physical_device, &dev_info, nullptr,
                                     &g_renderer.device),
                      "vkCreateDevice"))
            return;

        vkGetDeviceQueue(g_renderer.device, g_renderer.queue_family_index, 0,
                         &g_renderer.queue);

        VkCommandPoolCreateInfo pool_info{};
        pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        pool_info.queueFamilyIndex = g_renderer.queue_family_index;
        if (!vk_check(vkCreateCommandPool(g_renderer.device, &pool_info, nullptr,
                                          &g_renderer.command_pool),
                      "vkCreateCommandPool"))
            return;

        VkSemaphoreCreateInfo sem_info{};
        sem_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        vk_check(vkCreateSemaphore(g_renderer.device, &sem_info, nullptr,
                                   &g_renderer.image_available),
                 "vkCreateSemaphore(image_available)");
        vk_check(vkCreateSemaphore(g_renderer.device, &sem_info, nullptr,
                                   &g_renderer.render_finished),
                 "vkCreateSemaphore(render_finished)");

        VkFenceCreateInfo fence_info{};
        fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;
        vk_check(vkCreateFence(g_renderer.device, &fence_info, nullptr,
                               &g_renderer.in_flight),
                 "vkCreateFence");
    }

    if (!create_swapchain())
        return;

    g_renderer.initialized = true;
    PH_ANDROID_LOG_I("phenotype Vulkan renderer initialised (%ux%u, format=%d)",
                     g_renderer.swapchain_extent.width,
                     g_renderer.swapchain_extent.height,
                     static_cast<int>(g_renderer.swapchain_format));
}

inline void renderer_flush(unsigned char const*, unsigned int) {
    if (!g_renderer.initialized)
        return;

    vkWaitForFences(g_renderer.device, 1, &g_renderer.in_flight, VK_TRUE, UINT64_MAX);

    uint32_t image_index = 0;
    auto acquire = vkAcquireNextImageKHR(
        g_renderer.device, g_renderer.swapchain, UINT64_MAX,
        g_renderer.image_available, VK_NULL_HANDLE, &image_index);
    if (acquire == VK_ERROR_OUT_OF_DATE_KHR || acquire == VK_SUBOPTIMAL_KHR) {
        destroy_swapchain_resources();
        if (!create_swapchain()) {
            PH_ANDROID_LOG_W("swapchain recreate failed during acquire");
            return;
        }
        return; // skip this frame, repaint on next
    }
    if (!vk_check(acquire, "vkAcquireNextImageKHR"))
        return;

    vkResetFences(g_renderer.device, 1, &g_renderer.in_flight);
    vkResetCommandBuffer(g_renderer.command_buffer, 0);

    VkCommandBufferBeginInfo begin_info{};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vk_check(vkBeginCommandBuffer(g_renderer.command_buffer, &begin_info),
             "vkBeginCommandBuffer");

    auto transition = [&](VkImageLayout old_layout, VkImageLayout new_layout,
                          VkPipelineStageFlags src_stage, VkPipelineStageFlags dst_stage,
                          VkAccessFlags src_access, VkAccessFlags dst_access) {
        VkImageMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout = old_layout;
        barrier.newLayout = new_layout;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = g_renderer.swapchain_images[image_index];
        barrier.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
        barrier.srcAccessMask = src_access;
        barrier.dstAccessMask = dst_access;
        vkCmdPipelineBarrier(
            g_renderer.command_buffer, src_stage, dst_stage, 0,
            0, nullptr, 0, nullptr, 1, &barrier);
    };

    transition(VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
               VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
               0, VK_ACCESS_TRANSFER_WRITE_BIT);

    auto const& theme = ::phenotype::current_theme();
    VkClearColorValue clear{};
    clear.float32[0] = static_cast<float>(theme.background.r) / 255.0f;
    clear.float32[1] = static_cast<float>(theme.background.g) / 255.0f;
    clear.float32[2] = static_cast<float>(theme.background.b) / 255.0f;
    clear.float32[3] = static_cast<float>(theme.background.a) / 255.0f;

    VkImageSubresourceRange range{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
    vkCmdClearColorImage(
        g_renderer.command_buffer,
        g_renderer.swapchain_images[image_index],
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        &clear, 1, &range);

    transition(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
               VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
               VK_ACCESS_TRANSFER_WRITE_BIT, 0);

    vk_check(vkEndCommandBuffer(g_renderer.command_buffer), "vkEndCommandBuffer");

    VkPipelineStageFlags wait_stages = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    VkSubmitInfo submit{};
    submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit.waitSemaphoreCount = 1;
    submit.pWaitSemaphores = &g_renderer.image_available;
    submit.pWaitDstStageMask = &wait_stages;
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &g_renderer.command_buffer;
    submit.signalSemaphoreCount = 1;
    submit.pSignalSemaphores = &g_renderer.render_finished;
    vk_check(vkQueueSubmit(g_renderer.queue, 1, &submit, g_renderer.in_flight),
             "vkQueueSubmit");

    VkPresentInfoKHR present{};
    present.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    present.waitSemaphoreCount = 1;
    present.pWaitSemaphores = &g_renderer.render_finished;
    present.swapchainCount = 1;
    present.pSwapchains = &g_renderer.swapchain;
    present.pImageIndices = &image_index;
    auto present_result = vkQueuePresentKHR(g_renderer.queue, &present);
    if (present_result == VK_ERROR_OUT_OF_DATE_KHR
        || present_result == VK_SUBOPTIMAL_KHR) {
        destroy_swapchain_resources();
        create_swapchain();
    } else {
        vk_check(present_result, "vkQueuePresentKHR");
    }
}

inline void renderer_shutdown() {
    if (g_renderer.device != VK_NULL_HANDLE)
        vkDeviceWaitIdle(g_renderer.device);

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
    // Stage 2: log + drop. Stage 6+ wires JNI Intent.ACTION_VIEW through
    // the GameActivity JVM env.
    PH_ANDROID_LOG_I("open_url ignored: %.*s", static_cast<int>(len), url);
}

} // namespace phenotype::native::detail

export namespace phenotype::native {

inline platform_api const& android_platform() {
    static platform_api api = [] {
        auto base = make_stub_platform(
            "android",
            "[phenotype-native] using Android Vulkan backend (stage 2: clear-color only)");
        base.renderer = {
            detail::renderer_init,
            detail::renderer_flush,
            detail::renderer_shutdown,
            detail::renderer_hit_test,
        };
        base.open_url = detail::android_open_url;
        return base;
    }();
    return api;
}

} // namespace phenotype::native
#endif
