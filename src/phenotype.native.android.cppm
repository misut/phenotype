// Android (Vulkan) platform backend for phenotype. Stage 2 clears the
// swapchain to the active theme background; Stage 3 brings the color
// primitives (Clear / FillRect / StrokeRect / RoundRect / DrawLine)
// online via a single instanced graphics pipeline that mirrors the
// macOS / Windows color_pipeline design.
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

namespace phenotype::native::detail::shaders {
#include "phenotype.native.android.shaders.inl"
} // namespace phenotype::native::detail::shaders

namespace phenotype::native::detail {

constexpr char const ANDROID_LOG_TAG[] = "phenotype";

// ColorInstance layout mirrored byte-for-byte with macOS
// phenotype.native.macos.cppm's ColorInstanceGPU (stride 48). The GLSL
// `struct ColorInstance` in color.vert / color.frag reads the same
// layout — keep them in sync if anything here changes.
struct ColorInstanceGPU {
    float rect[4];
    float color[4];
    float params[4];
};
static_assert(sizeof(ColorInstanceGPU) == 48,
              "ColorInstance stride must match the GLSL SSBO layout");

// Uniforms block: std140-laid-out vec2 viewport + vec2 pad (= 16B).
struct ColorUniforms {
    float viewport[2];
    float _pad[2];
};
static_assert(sizeof(ColorUniforms) == 16);

inline constexpr std::size_t INITIAL_INSTANCE_CAPACITY = 256;

struct android_renderer {
    VkInstance instance;
    VkPhysicalDevice physical_device;
    VkDevice device;
    std::uint32_t queue_family_index;
    VkQueue queue;
    VkSurfaceKHR surface;

    // Swapchain-scoped — rebuilt on resize / lifecycle.
    VkSwapchainKHR swapchain;
    VkFormat swapchain_format;
    VkExtent2D swapchain_extent;
    std::vector<VkImage> swapchain_images;
    std::vector<VkImageView> swapchain_image_views;
    std::vector<VkFramebuffer> framebuffers;
    VkCommandBuffer command_buffer;

    // Device-scoped — created once per device, destroyed on shutdown.
    VkCommandPool command_pool;
    VkSemaphore image_available;
    VkSemaphore render_finished;
    VkFence in_flight;

    // Stage 3 color pipeline. Device-scoped; reused across swapchain
    // recreates. The framebuffers above bind to render_pass.
    VkRenderPass render_pass;
    VkDescriptorSetLayout color_descriptor_set_layout;
    VkPipelineLayout color_pipeline_layout;
    VkPipeline color_pipeline;
    VkDescriptorPool color_descriptor_pool;
    VkDescriptorSet color_descriptor_set;

    // Host-visible, persistently mapped. Uniform is fixed 16 bytes;
    // the instance buffer grows by doubling on overflow.
    VkBuffer uniform_buffer;
    VkDeviceMemory uniform_memory;
    void* uniform_mapped;

    VkBuffer instance_buffer;
    VkDeviceMemory instance_memory;
    void* instance_mapped;
    std::size_t instance_capacity; // in ColorInstanceGPU count

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

inline std::optional<std::uint32_t> find_memory_type(
        std::uint32_t type_filter, VkMemoryPropertyFlags flags) {
    VkPhysicalDeviceMemoryProperties props{};
    vkGetPhysicalDeviceMemoryProperties(g_renderer.physical_device, &props);
    for (std::uint32_t i = 0; i < props.memoryTypeCount; ++i) {
        if ((type_filter & (1u << i))
            && (props.memoryTypes[i].propertyFlags & flags) == flags) {
            return i;
        }
    }
    return std::nullopt;
}

// Allocates a host-visible, host-coherent, persistently-mapped buffer
// of the requested size + usage. Returns false on failure.
inline bool create_host_buffer(VkDeviceSize size, VkBufferUsageFlags usage,
                               VkBuffer& out_buffer,
                               VkDeviceMemory& out_memory,
                               void*& out_mapped) {
    VkBufferCreateInfo bi{};
    bi.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bi.size = size;
    bi.usage = usage;
    bi.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    if (!vk_ok(vkCreateBuffer(g_renderer.device, &bi, nullptr, &out_buffer),
              "vkCreateBuffer"))
        return false;

    VkMemoryRequirements req{};
    vkGetBufferMemoryRequirements(g_renderer.device, out_buffer, &req);
    auto mt = find_memory_type(req.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    if (!mt) {
        __android_log_print(ANDROID_LOG_ERROR, ANDROID_LOG_TAG,
                            "no host-visible memory type for buffer usage=%u",
                            static_cast<unsigned>(usage));
        return false;
    }

    VkMemoryAllocateInfo ai{};
    ai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    ai.allocationSize = req.size;
    ai.memoryTypeIndex = *mt;
    if (!vk_ok(vkAllocateMemory(g_renderer.device, &ai, nullptr, &out_memory),
              "vkAllocateMemory"))
        return false;
    if (!vk_ok(vkBindBufferMemory(g_renderer.device, out_buffer, out_memory, 0),
              "vkBindBufferMemory"))
        return false;
    if (!vk_ok(vkMapMemory(g_renderer.device, out_memory, 0, VK_WHOLE_SIZE, 0,
                           &out_mapped),
              "vkMapMemory"))
        return false;
    return true;
}

inline void destroy_host_buffer(VkBuffer& buffer, VkDeviceMemory& memory,
                                void*& mapped) {
    if (memory != VK_NULL_HANDLE && mapped != nullptr) {
        vkUnmapMemory(g_renderer.device, memory);
        mapped = nullptr;
    }
    if (buffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(g_renderer.device, buffer, nullptr);
        buffer = VK_NULL_HANDLE;
    }
    if (memory != VK_NULL_HANDLE) {
        vkFreeMemory(g_renderer.device, memory, nullptr);
        memory = VK_NULL_HANDLE;
    }
}

// Rewrites the single descriptor set to point at the current uniform
// + instance buffers. Called on device init and whenever the instance
// buffer is reallocated after a capacity overflow.
inline void write_color_descriptor_set() {
    if (g_renderer.color_descriptor_set == VK_NULL_HANDLE) return;
    VkDescriptorBufferInfo ubo{};
    ubo.buffer = g_renderer.uniform_buffer;
    ubo.offset = 0;
    ubo.range = sizeof(ColorUniforms);

    VkDescriptorBufferInfo ssbo{};
    ssbo.buffer = g_renderer.instance_buffer;
    ssbo.offset = 0;
    ssbo.range = VK_WHOLE_SIZE;

    VkWriteDescriptorSet writes[2]{};
    writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[0].dstSet = g_renderer.color_descriptor_set;
    writes[0].dstBinding = 0;
    writes[0].descriptorCount = 1;
    writes[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    writes[0].pBufferInfo = &ubo;

    writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[1].dstSet = g_renderer.color_descriptor_set;
    writes[1].dstBinding = 1;
    writes[1].descriptorCount = 1;
    writes[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    writes[1].pBufferInfo = &ssbo;

    vkUpdateDescriptorSets(g_renderer.device, 2, writes, 0, nullptr);
}

// Ensures the instance buffer can hold at least `required` instances.
// Grows by doubling. Rewrites the descriptor set when the underlying
// buffer is reallocated. Returns false on allocation failure.
inline bool ensure_instance_capacity(std::size_t required) {
    if (required <= g_renderer.instance_capacity) return true;
    std::size_t cap = g_renderer.instance_capacity == 0
                        ? INITIAL_INSTANCE_CAPACITY
                        : g_renderer.instance_capacity;
    while (cap < required) cap *= 2;

    // GPU may still be consuming the old buffer; the caller guarantees
    // the in-flight fence has been waited on before calling this.
    destroy_host_buffer(g_renderer.instance_buffer,
                        g_renderer.instance_memory,
                        g_renderer.instance_mapped);

    if (!create_host_buffer(cap * sizeof(ColorInstanceGPU),
                            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                            g_renderer.instance_buffer,
                            g_renderer.instance_memory,
                            g_renderer.instance_mapped))
        return false;
    g_renderer.instance_capacity = cap;
    write_color_descriptor_set();
    return true;
}

inline VkShaderModule create_shader_module(uint32_t const* code,
                                           std::size_t size_bytes) {
    VkShaderModuleCreateInfo ci{};
    ci.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    ci.codeSize = size_bytes;
    ci.pCode = code;
    VkShaderModule mod = VK_NULL_HANDLE;
    vk_ok(vkCreateShaderModule(g_renderer.device, &ci, nullptr, &mod),
          "vkCreateShaderModule");
    return mod;
}

inline bool create_render_pass() {
    if (g_renderer.render_pass != VK_NULL_HANDLE) return true;

    VkAttachmentDescription att{};
    att.format = g_renderer.swapchain_format;
    att.samples = VK_SAMPLE_COUNT_1_BIT;
    att.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    att.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    att.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    att.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    att.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    att.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference ref{};
    ref.attachment = 0;
    ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription sub{};
    sub.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    sub.colorAttachmentCount = 1;
    sub.pColorAttachments = &ref;

    VkSubpassDependency dep{};
    dep.srcSubpass = VK_SUBPASS_EXTERNAL;
    dep.dstSubpass = 0;
    dep.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dep.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dep.srcAccessMask = 0;
    dep.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    VkRenderPassCreateInfo ci{};
    ci.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    ci.attachmentCount = 1;
    ci.pAttachments = &att;
    ci.subpassCount = 1;
    ci.pSubpasses = &sub;
    ci.dependencyCount = 1;
    ci.pDependencies = &dep;

    return vk_ok(vkCreateRenderPass(g_renderer.device, &ci, nullptr,
                                    &g_renderer.render_pass),
                "vkCreateRenderPass");
}

inline bool create_color_pipeline() {
    if (g_renderer.color_pipeline != VK_NULL_HANDLE) return true;

    // Descriptor set layout: binding 0 = UBO (vertex), binding 1 = SSBO (vertex).
    VkDescriptorSetLayoutBinding bindings[2]{};
    bindings[0].binding = 0;
    bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    bindings[0].descriptorCount = 1;
    bindings[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    bindings[1].binding = 1;
    bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    bindings[1].descriptorCount = 1;
    bindings[1].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

    VkDescriptorSetLayoutCreateInfo dslci{};
    dslci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    dslci.bindingCount = 2;
    dslci.pBindings = bindings;
    if (!vk_ok(vkCreateDescriptorSetLayout(
                  g_renderer.device, &dslci, nullptr,
                  &g_renderer.color_descriptor_set_layout),
              "vkCreateDescriptorSetLayout"))
        return false;

    VkPipelineLayoutCreateInfo plci{};
    plci.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    plci.setLayoutCount = 1;
    plci.pSetLayouts = &g_renderer.color_descriptor_set_layout;
    if (!vk_ok(vkCreatePipelineLayout(g_renderer.device, &plci, nullptr,
                                      &g_renderer.color_pipeline_layout),
              "vkCreatePipelineLayout"))
        return false;

    auto vs = create_shader_module(shaders::SPIRV_COLOR_VS,
                                   sizeof(shaders::SPIRV_COLOR_VS));
    auto fs = create_shader_module(shaders::SPIRV_COLOR_FS,
                                   sizeof(shaders::SPIRV_COLOR_FS));
    if (vs == VK_NULL_HANDLE || fs == VK_NULL_HANDLE) return false;

    VkPipelineShaderStageCreateInfo stages[2]{};
    stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    stages[0].module = vs;
    stages[0].pName = "main";
    stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    stages[1].module = fs;
    stages[1].pName = "main";

    VkPipelineVertexInputStateCreateInfo vi{};
    vi.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    // No vertex bindings; corners are baked into the shader.

    VkPipelineInputAssemblyStateCreateInfo ia{};
    ia.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    ia.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    VkPipelineViewportStateCreateInfo vp{};
    vp.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    vp.viewportCount = 1;
    vp.scissorCount = 1;

    VkPipelineRasterizationStateCreateInfo rs{};
    rs.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rs.polygonMode = VK_POLYGON_MODE_FILL;
    rs.cullMode = VK_CULL_MODE_NONE;
    rs.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rs.lineWidth = 1.0f;

    VkPipelineMultisampleStateCreateInfo ms{};
    ms.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    // Source-alpha premultiplied-over, matching macOS / Windows setup.
    VkPipelineColorBlendAttachmentState cba{};
    cba.blendEnable = VK_TRUE;
    cba.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    cba.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    cba.colorBlendOp = VK_BLEND_OP_ADD;
    cba.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    cba.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    cba.alphaBlendOp = VK_BLEND_OP_ADD;
    cba.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT
                       | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

    VkPipelineColorBlendStateCreateInfo cb{};
    cb.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    cb.attachmentCount = 1;
    cb.pAttachments = &cba;

    // Dynamic viewport / scissor so the same pipeline survives
    // swapchain resizes without recreation.
    VkDynamicState dyn[2] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
    VkPipelineDynamicStateCreateInfo ds{};
    ds.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    ds.dynamicStateCount = 2;
    ds.pDynamicStates = dyn;

    VkGraphicsPipelineCreateInfo gci{};
    gci.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    gci.stageCount = 2;
    gci.pStages = stages;
    gci.pVertexInputState = &vi;
    gci.pInputAssemblyState = &ia;
    gci.pViewportState = &vp;
    gci.pRasterizationState = &rs;
    gci.pMultisampleState = &ms;
    gci.pColorBlendState = &cb;
    gci.pDynamicState = &ds;
    gci.layout = g_renderer.color_pipeline_layout;
    gci.renderPass = g_renderer.render_pass;
    gci.subpass = 0;

    bool ok = vk_ok(vkCreateGraphicsPipelines(
                        g_renderer.device, VK_NULL_HANDLE, 1, &gci, nullptr,
                        &g_renderer.color_pipeline),
                   "vkCreateGraphicsPipelines");

    vkDestroyShaderModule(g_renderer.device, vs, nullptr);
    vkDestroyShaderModule(g_renderer.device, fs, nullptr);
    return ok;
}

inline bool create_descriptor_pool_and_set() {
    if (g_renderer.color_descriptor_set != VK_NULL_HANDLE) return true;

    VkDescriptorPoolSize sizes[2]{};
    sizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    sizes[0].descriptorCount = 1;
    sizes[1].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    sizes[1].descriptorCount = 1;

    VkDescriptorPoolCreateInfo pci{};
    pci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pci.maxSets = 1;
    pci.poolSizeCount = 2;
    pci.pPoolSizes = sizes;
    if (!vk_ok(vkCreateDescriptorPool(g_renderer.device, &pci, nullptr,
                                      &g_renderer.color_descriptor_pool),
              "vkCreateDescriptorPool"))
        return false;

    VkDescriptorSetAllocateInfo ai{};
    ai.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    ai.descriptorPool = g_renderer.color_descriptor_pool;
    ai.descriptorSetCount = 1;
    ai.pSetLayouts = &g_renderer.color_descriptor_set_layout;
    if (!vk_ok(vkAllocateDescriptorSets(g_renderer.device, &ai,
                                        &g_renderer.color_descriptor_set),
              "vkAllocateDescriptorSets"))
        return false;
    write_color_descriptor_set();
    return true;
}

inline bool create_color_resources() {
    // Idempotent: first swapchain creation does the full build; later
    // swapchain recreates reuse render_pass / pipeline / buffers and
    // only rebuild the image views + framebuffers.
    if (!create_render_pass()) return false;
    if (!create_color_pipeline()) return false;

    if (g_renderer.uniform_buffer == VK_NULL_HANDLE) {
        if (!create_host_buffer(sizeof(ColorUniforms),
                                VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                g_renderer.uniform_buffer,
                                g_renderer.uniform_memory,
                                g_renderer.uniform_mapped))
            return false;
    }
    if (g_renderer.instance_buffer == VK_NULL_HANDLE) {
        if (!create_host_buffer(
                INITIAL_INSTANCE_CAPACITY * sizeof(ColorInstanceGPU),
                VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                g_renderer.instance_buffer,
                g_renderer.instance_memory,
                g_renderer.instance_mapped))
            return false;
        g_renderer.instance_capacity = INITIAL_INSTANCE_CAPACITY;
    }
    if (!create_descriptor_pool_and_set()) return false;
    return true;
}

inline bool create_image_views_and_framebuffers() {
    g_renderer.swapchain_image_views.resize(g_renderer.swapchain_images.size());
    for (std::size_t i = 0; i < g_renderer.swapchain_images.size(); ++i) {
        VkImageViewCreateInfo ci{};
        ci.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        ci.image = g_renderer.swapchain_images[i];
        ci.viewType = VK_IMAGE_VIEW_TYPE_2D;
        ci.format = g_renderer.swapchain_format;
        ci.components = { VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY,
                          VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY };
        ci.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
        if (!vk_ok(vkCreateImageView(g_renderer.device, &ci, nullptr,
                                     &g_renderer.swapchain_image_views[i]),
                  "vkCreateImageView"))
            return false;
    }

    g_renderer.framebuffers.resize(g_renderer.swapchain_images.size());
    for (std::size_t i = 0; i < g_renderer.swapchain_images.size(); ++i) {
        VkFramebufferCreateInfo fi{};
        fi.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        fi.renderPass = g_renderer.render_pass;
        fi.attachmentCount = 1;
        fi.pAttachments = &g_renderer.swapchain_image_views[i];
        fi.width = g_renderer.swapchain_extent.width;
        fi.height = g_renderer.swapchain_extent.height;
        fi.layers = 1;
        if (!vk_ok(vkCreateFramebuffer(g_renderer.device, &fi, nullptr,
                                       &g_renderer.framebuffers[i]),
                  "vkCreateFramebuffer"))
            return false;
    }
    return true;
}

inline void destroy_swapchain_resources() {
    if (g_renderer.device == VK_NULL_HANDLE) return;
    if (g_renderer.in_flight != VK_NULL_HANDLE)
        vkWaitForFences(g_renderer.device, 1, &g_renderer.in_flight, VK_TRUE, UINT64_MAX);

    for (auto fb : g_renderer.framebuffers) {
        if (fb != VK_NULL_HANDLE)
            vkDestroyFramebuffer(g_renderer.device, fb, nullptr);
    }
    g_renderer.framebuffers.clear();

    for (auto iv : g_renderer.swapchain_image_views) {
        if (iv != VK_NULL_HANDLE)
            vkDestroyImageView(g_renderer.device, iv, nullptr);
    }
    g_renderer.swapchain_image_views.clear();

    if (g_renderer.command_pool != VK_NULL_HANDLE
        && g_renderer.command_buffer != VK_NULL_HANDLE) {
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
    // Stage 3 renders via a color attachment; keep TRANSFER_DST for the
    // legacy clear path (commit 3 drops it once renderer_flush migrates).
    ci.imageUsage = VK_IMAGE_USAGE_TRANSFER_DST_BIT
                  | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
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

    if (!create_color_resources()) return false;
    if (!create_image_views_and_framebuffers()) return false;
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
    // Stage 2 path — kept behavior-neutral in this commit. The graphics
    // pipeline / render pass / descriptor set / buffers are created by
    // renderer_init but not yet recorded into the command buffer.
    // Commit 3 swaps this body for a proper render-pass draw.
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

inline void destroy_color_resources() {
    if (g_renderer.color_descriptor_pool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(g_renderer.device,
                                g_renderer.color_descriptor_pool, nullptr);
        g_renderer.color_descriptor_pool = VK_NULL_HANDLE;
        g_renderer.color_descriptor_set = VK_NULL_HANDLE;
    }
    destroy_host_buffer(g_renderer.instance_buffer,
                        g_renderer.instance_memory,
                        g_renderer.instance_mapped);
    g_renderer.instance_capacity = 0;
    destroy_host_buffer(g_renderer.uniform_buffer,
                        g_renderer.uniform_memory,
                        g_renderer.uniform_mapped);
    if (g_renderer.color_pipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(g_renderer.device, g_renderer.color_pipeline, nullptr);
        g_renderer.color_pipeline = VK_NULL_HANDLE;
    }
    if (g_renderer.color_pipeline_layout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(g_renderer.device,
                                g_renderer.color_pipeline_layout, nullptr);
        g_renderer.color_pipeline_layout = VK_NULL_HANDLE;
    }
    if (g_renderer.color_descriptor_set_layout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(g_renderer.device,
                                     g_renderer.color_descriptor_set_layout,
                                     nullptr);
        g_renderer.color_descriptor_set_layout = VK_NULL_HANDLE;
    }
    if (g_renderer.render_pass != VK_NULL_HANDLE) {
        vkDestroyRenderPass(g_renderer.device, g_renderer.render_pass, nullptr);
        g_renderer.render_pass = VK_NULL_HANDLE;
    }
}

inline void renderer_shutdown() {
    if (g_renderer.device != VK_NULL_HANDLE) vkDeviceWaitIdle(g_renderer.device);
    destroy_swapchain_resources();
    destroy_color_resources();
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
