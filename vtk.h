#pragma once

#include <windows.h>

#define VK_USE_PLATFORM_WIN32_KHR
#include <vulkan/vulkan.h>

#include "ctk/ctk.h"
#include "ctk/memory.h"
#include "ctk/containers.h"
#include "ctk/file.h"
#include "vtk/debug.h"
#include "vtk/device_features.h"

using namespace ctk;

namespace vtk {

////////////////////////////////////////////////////////////
/// Macros
////////////////////////////////////////////////////////////
#define LOAD_INSTANCE_EXTENSION_FUNCTION(INSTANCE, FUNC_NAME)\
    auto FUNC_NAME = (PFN_ ## FUNC_NAME)vkGetInstanceProcAddr(INSTANCE, #FUNC_NAME);\
    if (FUNC_NAME == NULL)\
        CTK_FATAL("failed to load instance extension function \"%s\"", #FUNC_NAME)

#define COLOR_COMPONENT_RGBA \
    VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT

////////////////////////////////////////////////////////////
/// Data
////////////////////////////////////////////////////////////
typedef VKAPI_ATTR VkBool32 (VKAPI_CALL *DebugCallback)(VkDebugUtilsMessageSeverityFlagBitsEXT msg_severity_flag_bit,
                                                        VkDebugUtilsMessageTypeFlagsEXT msg_type_flags,
                                                        const VkDebugUtilsMessengerCallbackDataEXT *callback_data,
                                                        void *user_data);

struct InstanceInfo {
    bool enable_validation;
    DebugCallback debug_callback;
};

struct Instance {
    VkInstance handle;
    VkDebugUtilsMessengerEXT debug_messenger;
};

struct QueueFamilyIndexes {
    u32 graphics;
    u32 present;
};

struct PhysicalDevice {
    VkPhysicalDevice handle;
    QueueFamilyIndexes queue_family_idxs;

    VkPhysicalDeviceType type;
    u32 min_uniform_buffer_offset_alignment;
    u32 max_push_constant_size;

    VkPhysicalDeviceFeatures features;
    VkPhysicalDeviceMemoryProperties mem_properties;
    VkFormat depth_image_format;
};

struct Swapchain {
    VkSwapchainKHR handle;
    FixedArray<VkImageView, 4> image_views;
    u32 image_count;
    VkFormat image_format;
    VkExtent2D extent;
};

struct BufferInfo {
    VkDeviceSize size;
    VkSharingMode sharing_mode;
    VkBufferUsageFlags usage_flags;
    VkMemoryPropertyFlags mem_property_flags;
};

struct Buffer {
    VkBuffer handle;
    VkDeviceMemory mem;
    VkDeviceSize size;
};

struct ImageInfo {
    VkImageCreateInfo image;
    VkImageViewCreateInfo view;
    VkMemoryPropertyFlagBits mem_property_flags;
};

struct Image {
    VkImage handle;
    VkImageView view;
    VkDeviceMemory mem;
    VkExtent3D extent;
};

struct SubpassInfo {
    Array<u32> *preserve_attachment_indexes;
    Array<VkAttachmentReference> *input_attachment_refs;
    Array<VkAttachmentReference> *color_attachment_refs;
    VkAttachmentReference depth_attachment_ref;
};

struct AttachmentInfo {
    VkAttachmentDescription description;
    VkClearValue clear_value;
};

struct RenderPassInfo {
    struct {
        Array<VkAttachmentDescription> *descriptions;
        Array<VkClearValue> *clear_values;
    } attachment;

    struct {
        Array<SubpassInfo> *infos;
        Array<VkSubpassDependency> *dependencies;
    } subpass;
};

struct RenderPass {
    VkRenderPass handle;
    Array<VkClearValue> *attachment_clear_values;
};

struct FramebufferInfo {
    Array<VkImageView> *attachments;
    VkExtent2D extent;
    u32 layers;
};

struct CommandBufferAllocateInfo {
    VkCommandPool pool;
    VkCommandBufferLevel level;
};

struct DescriptorPoolInfo {
    struct {
        u32 uniform_buffer;
        u32 uniform_buffer_dynamic;
        u32 combined_image_sampler;
        u32 input_attachment;
    } descriptor_count;

    u32 max_descriptor_sets;
};

struct DescriptorInfo {
    u32 count;
    VkDescriptorType type;
    VkShaderStageFlags stage;
};

struct BufferWriteInfo {
    Buffer *buffer;
    VkDeviceSize offset;
    void *data;
    VkDeviceSize size;
};

struct BufferCopyInfo {
    Buffer *src_buffer;
    VkDeviceSize src_offset;
    Buffer *dst_buffer;
    VkDeviceSize dst_offset;
    VkDeviceSize size;
};

struct ImageMemoryInfo {
    VkImageLayout layout;
    VkPipelineStageFlags stage;
    VkAccessFlags access;
    u32 queue_family_index;
};

struct ImageMemoryBarrier {
    ImageMemoryInfo src;
    ImageMemoryInfo dst;
    VkImageSubresourceRange subresource_range;
};

static constexpr VkPipelineColorBlendAttachmentState DEFAULT_COLOR_BLEND_ATTACHMENT = {
    .blendEnable = VK_FALSE,
    .srcColorBlendFactor = VK_BLEND_FACTOR_ZERO,
    .dstColorBlendFactor = VK_BLEND_FACTOR_ZERO,
    .colorBlendOp = VK_BLEND_OP_ADD,
    .srcAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
    .dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
    .alphaBlendOp = VK_BLEND_OP_ADD,
    .colorWriteMask = COLOR_COMPONENT_RGBA,
};

////////////////////////////////////////////////////////////
/// Utils
////////////////////////////////////////////////////////////
template<typename Object, typename Loader, typename ...Args>
static Array<Object> *load_vk_objects(Memory *mem, Loader loader, Args... args) {
    u32 count = 0;
    loader(args..., &count, NULL);
    CTK_ASSERT(count > 0);
    auto vk_objects = create_array_full<Object>(mem, count, 0);
    loader(args..., &vk_objects->count, vk_objects->data);
    return vk_objects;
}

static VkFormat find_depth_image_format(PhysicalDevice *physical_device) {
    static constexpr VkFormat DEPTH_IMAGE_FORMATS[] = {
        VK_FORMAT_D32_SFLOAT_S8_UINT,
        VK_FORMAT_D32_SFLOAT,
        VK_FORMAT_D24_UNORM_S8_UINT,
        VK_FORMAT_D16_UNORM_S8_UINT,
        VK_FORMAT_D16_UNORM
    };

    static constexpr VkFormatFeatureFlags DEPTH_IMG_FMT_FEATS = VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT;

    // Find format that supports depth-stencil attachment feature for physical device.
    for (u32 i = 0; i < CTK_ARRAY_SIZE(DEPTH_IMAGE_FORMATS); i++) {
        VkFormat depth_img_fmt = DEPTH_IMAGE_FORMATS[i];
        VkFormatProperties depth_img_fmt_props = {};
        vkGetPhysicalDeviceFormatProperties(physical_device->handle, depth_img_fmt, &depth_img_fmt_props);

        if ((depth_img_fmt_props.optimalTilingFeatures & DEPTH_IMG_FMT_FEATS) == DEPTH_IMG_FMT_FEATS)
            return depth_img_fmt;
    }

    CTK_FATAL("failed to find physical device depth format that supports the depth-stencil attachment feature")
}

static u32 find_memory_type_index(VkMemoryRequirements mem_reqs, PhysicalDevice *physical_device,
                                  VkMemoryPropertyFlags mem_prop_flags)
{
    VkPhysicalDeviceMemoryProperties mem_props = physical_device->mem_properties;

    // Find memory type index from device based on memory property flags.
    for (u32 mem_type_idx = 0; mem_type_idx < mem_props.memoryTypeCount; ++mem_type_idx) {
        // Ensure index refers to memory type from memory requirements.
        if (!(mem_reqs.memoryTypeBits & (1 << mem_type_idx)))
            continue;

        // Check if memory at index has correct properties.
        if ((mem_props.memoryTypes[mem_type_idx].propertyFlags & mem_prop_flags) == mem_prop_flags)
            return mem_type_idx;
    }

    CTK_FATAL("failed to find memory type that satisfies property requirements");
}

////////////////////////////////////////////////////////////
/// Initialization
////////////////////////////////////////////////////////////
static Instance *create_instance(Instance *instance, InstanceInfo info) {
    VkDebugUtilsMessengerCreateInfoEXT debug_msgr_info = {};
    if (info.enable_validation) {
        debug_msgr_info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        debug_msgr_info.pNext = NULL;
        debug_msgr_info.flags = 0;
        debug_msgr_info.messageSeverity = // VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
                                          // VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT |
                                          VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                                          VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        debug_msgr_info.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                                      VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                                      VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        debug_msgr_info.pfnUserCallback = info.debug_callback ? info.debug_callback : debug_callback;
        debug_msgr_info.pUserData = NULL;
    }

    VkApplicationInfo app_info = {};
    app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app_info.pNext = NULL;
    app_info.pApplicationName = "renderer";
    app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    app_info.pEngineName = "renderer";
    app_info.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    app_info.apiVersion = VK_API_VERSION_1_0;

    FixedArray<cstr, 16> extensions = {};
    push(&extensions, VK_KHR_WIN32_SURFACE_EXTENSION_NAME);
    push(&extensions, VK_KHR_SURFACE_EXTENSION_NAME);
    if (info.enable_validation)
        push(&extensions, VK_EXT_DEBUG_UTILS_EXTENSION_NAME); // Validation

    FixedArray<cstr, 16> layers = {};
    if (info.enable_validation)
        push(&layers, "VK_LAYER_KHRONOS_validation"); // Validation

    VkInstanceCreateInfo create_info = {};
    create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    create_info.pNext = info.enable_validation ? &debug_msgr_info : NULL;
    create_info.flags = 0;
    create_info.pApplicationInfo = &app_info;
    create_info.enabledLayerCount = layers.count;
    create_info.ppEnabledLayerNames = layers.data;
    create_info.enabledExtensionCount = extensions.count;
    create_info.ppEnabledExtensionNames = extensions.data;
    validate(vkCreateInstance(&create_info, NULL, &instance->handle), "failed to create Vulkan instance");

    if (info.enable_validation) {
        LOAD_INSTANCE_EXTENSION_FUNCTION(instance->handle, vkCreateDebugUtilsMessengerEXT);
        validate(vkCreateDebugUtilsMessengerEXT(instance->handle, &debug_msgr_info, NULL, &instance->debug_messenger),
                 "failed to create debug messenger");
    }

    return instance;
}

static VkSurfaceKHR create_win32_surface(Instance *instance, HWND win32_window, HINSTANCE win32_instance) {
    VkWin32SurfaceCreateInfoKHR info = {};
    info.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
    info.hwnd = win32_window;
    info.hinstance = win32_instance;

    VkSurfaceKHR surface = VK_NULL_HANDLE;
    validate(vkCreateWin32SurfaceKHR(instance->handle, &info, nullptr, &surface), "failed to get win32 surface");

    return surface;
}

static QueueFamilyIndexes find_queue_family_idxs(Memory temp_mem, VkPhysicalDevice physical_device,
                                                 VkSurfaceKHR surface)
{
    QueueFamilyIndexes queue_family_idxs = { .graphics = U32_MAX, .present = U32_MAX };
    auto queue_family_props_array =
        load_vk_objects<VkQueueFamilyProperties>(&temp_mem, vkGetPhysicalDeviceQueueFamilyProperties,
                                                 physical_device);

    for (u32 queue_family_idx = 0; queue_family_idx < queue_family_props_array->count; ++queue_family_idx) {
        VkQueueFamilyProperties *queue_family_props = queue_family_props_array->data + queue_family_idx;

        if (queue_family_props->queueFlags & VK_QUEUE_GRAPHICS_BIT)
            queue_family_idxs.graphics = queue_family_idx;

        VkBool32 present_supported = VK_FALSE;
        vkGetPhysicalDeviceSurfaceSupportKHR(physical_device, queue_family_idx, surface, &present_supported);

        if (present_supported == VK_TRUE)
            queue_family_idxs.present = queue_family_idx;
    }

    return queue_family_idxs;
}

static PhysicalDevice *find_suitable_physical_device(Memory temp_mem, Array<PhysicalDevice *> *physical_devices,
                                                     PhysicalDeviceFeature *requested_features,
                                                     u32 requested_feature_count)
{
    Array<PhysicalDeviceFeature> *unsupported_features =
        requested_features
        ? create_array<PhysicalDeviceFeature>(&temp_mem, requested_feature_count)
        : NULL;

    PhysicalDevice *suitable_device = NULL;
    for (u32 i = 0; suitable_device == NULL && i < physical_devices->count; ++i) {
        PhysicalDevice *physical_device = physical_devices->data[i];

        // Check for queue families that support vk and present.
        bool has_required_queue_families = physical_device->queue_family_idxs.graphics != U32_MAX &&
                                           physical_device->queue_family_idxs.present  != U32_MAX;

        bool requested_features_supported = true;
        if (requested_features) {
            clear(unsupported_features);

            // Check that all requested features are supported.
            for (u32 feat_index = 0; feat_index < requested_feature_count; ++feat_index) {
                PhysicalDeviceFeature requested_feature = requested_features[feat_index];

                if (!physical_device_feature_supported(requested_feature, &physical_device->features))
                    push(unsupported_features, requested_feature);
            }

            if (unsupported_features->count > 0) {
                requested_features_supported = false;
                CTK_TODO("report unsupported features");
            }
        }

        // Check if device passes all tests and load more physical_device about device if so.
        if (has_required_queue_families && requested_features_supported)
            suitable_device = physical_device;
    }

    return suitable_device;
}

static PhysicalDevice *create_physical_device(Memory temp_mem, PhysicalDevice *physical_device, Instance *instance,
                                              VkSurfaceKHR surface, PhysicalDeviceFeature *requested_features,
                                              u32 requested_feature_count)
{
    // Load info about all physical devices.
    auto vk_physical_devices = load_vk_objects<VkPhysicalDevice>(&temp_mem, vkEnumeratePhysicalDevices,
                                                                 instance->handle);

    auto physical_devices = create_array<PhysicalDevice>(&temp_mem, vk_physical_devices->count);

    for (u32 i = 0; i < vk_physical_devices->count; ++i) {
        VkPhysicalDevice vk_physical_device = vk_physical_devices->data[i];
        PhysicalDevice *curr_physical_device = push(physical_devices);
        curr_physical_device->handle = vk_physical_device;
        curr_physical_device->queue_family_idxs = find_queue_family_idxs(temp_mem, vk_physical_device, surface);

        // Load properties for future reference.
        VkPhysicalDeviceProperties properties = {};
        vkGetPhysicalDeviceProperties(vk_physical_device, &properties);
        curr_physical_device->type = properties.deviceType;
        curr_physical_device->min_uniform_buffer_offset_alignment = properties.limits.minUniformBufferOffsetAlignment;
        curr_physical_device->max_push_constant_size = properties.limits.maxPushConstantsSize;

        vkGetPhysicalDeviceFeatures(vk_physical_device, &curr_physical_device->features);
        vkGetPhysicalDeviceMemoryProperties(vk_physical_device, &curr_physical_device->mem_properties);
        curr_physical_device->depth_image_format = find_depth_image_format(curr_physical_device);
    }

    // Sort out discrete and integrated gpus.
    auto discrete_devices = create_array<PhysicalDevice *>(&temp_mem, physical_devices->count);
    auto integrated_devices = create_array<PhysicalDevice *>(&temp_mem, physical_devices->count);

    for (u32 i = 0; i < physical_devices->count; ++i) {
        PhysicalDevice *curr_physical_device = physical_devices->data + i;

        if (curr_physical_device->type == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
            push(discrete_devices, curr_physical_device);
        else if (curr_physical_device->type == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU)
            push(integrated_devices, curr_physical_device);
    }

    // Find suitable discrete device, or fallback to an integrated device.
    PhysicalDevice *suitable_device = find_suitable_physical_device(temp_mem, discrete_devices, requested_features,
                                                                    requested_feature_count);

    if (suitable_device == NULL) {
        suitable_device = find_suitable_physical_device(temp_mem, integrated_devices, requested_features,
                                                        requested_feature_count);

        if (suitable_device == NULL)
            CTK_FATAL("failed to find any suitable device");
    }

    *physical_device = *suitable_device;

    return physical_device;
}

static VkDeviceQueueCreateInfo default_queue_info(u32 queue_fam_idx) {
    static f32 const QUEUE_PRIORITIES[] = { 1.0f };

    VkDeviceQueueCreateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    info.flags = 0;
    info.queueFamilyIndex = queue_fam_idx;
    info.queueCount = CTK_ARRAY_SIZE(QUEUE_PRIORITIES);
    info.pQueuePriorities = QUEUE_PRIORITIES;

    return info;
}

static VkDevice create_device(PhysicalDevice *physical_device, PhysicalDeviceFeature *requested_features,
                              u32 requested_feature_count)
{
    FixedArray<VkDeviceQueueCreateInfo, 2> queue_infos = {};
    push(&queue_infos, default_queue_info(physical_device->queue_family_idxs.graphics));

    // Don't create separate queues if present and vk belong to same queue family.
    if (physical_device->queue_family_idxs.present != physical_device->queue_family_idxs.graphics)
        push(&queue_infos, default_queue_info(physical_device->queue_family_idxs.present));

    cstr extensions[] = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
    VkBool32 enabled_features[(s32)PhysicalDeviceFeature::COUNT] = {};

    for (u32 i = 0; i < requested_feature_count; ++i)
        enabled_features[(s32)requested_features[i]] = VK_TRUE;

    VkDeviceCreateInfo create_info = {};
    create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    create_info.flags = 0;
    create_info.queueCreateInfoCount = queue_infos.count;
    create_info.pQueueCreateInfos = queue_infos.data;
    create_info.enabledLayerCount = 0;
    create_info.ppEnabledLayerNames = NULL;
    create_info.enabledExtensionCount = CTK_ARRAY_SIZE(extensions);
    create_info.ppEnabledExtensionNames = extensions;
    create_info.pEnabledFeatures = (VkPhysicalDeviceFeatures *)enabled_features;

    VkDevice device = VK_NULL_HANDLE;
    validate(vkCreateDevice(physical_device->handle, &create_info, NULL, &device), "failed to create logical device");

    return device;
}

static VkQueue get_queue(VkDevice device, u32 queue_family_index, u32 queue_index)  {
    VkQueue queue = VK_NULL_HANDLE;
    vkGetDeviceQueue(device, queue_family_index, queue_index, &queue);
    return queue;
}

static VkSurfaceCapabilitiesKHR get_surface_capabilities(PhysicalDevice *physical_device, VkSurfaceKHR surface) {
    VkSurfaceCapabilitiesKHR capabilities = {};
    validate(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical_device->handle, surface, &capabilities),
             "failed to get physical device surface capabilities");

    return capabilities;
}

static VkExtent2D get_surface_extent(PhysicalDevice *physical_device, VkSurfaceKHR surface) {
    return get_surface_capabilities(physical_device, surface).currentExtent;
}

static Swapchain *create_swapchain(Memory temp_mem, Swapchain *swapchain, VkDevice device,
                                   PhysicalDevice *physical_device, VkSurfaceKHR surface)
{
    VkSurfaceCapabilitiesKHR surface_capabilities = get_surface_capabilities(physical_device, surface);

    ////////////////////////////////////////////////////////////
    /// Configuration
    ////////////////////////////////////////////////////////////

    // Configure swapchain based on surface properties.
    auto surface_formats =
        load_vk_objects<VkSurfaceFormatKHR>(
            &temp_mem,
            vkGetPhysicalDeviceSurfaceFormatsKHR,
            physical_device->handle,
            surface);

    auto surface_present_modes =
        load_vk_objects<VkPresentModeKHR>(
            &temp_mem,
            vkGetPhysicalDeviceSurfacePresentModesKHR,
            physical_device->handle,
            surface);

    // Default to first surface format.
    VkSurfaceFormatKHR selected_format = surface_formats->data[0];
    for (u32 i = 0; i < surface_formats->count; ++i) {
        VkSurfaceFormatKHR surface_format = surface_formats->data[i];

        // Prefer 4-component 8-bit BGRA unnormalized format and sRGB color space.
        if (surface_format.format == VK_FORMAT_B8G8R8A8_UNORM &&
            surface_format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {

            selected_format = surface_format;
            break;
        }
    }

    // Default to FIFO (only present mode with guarenteed availability).
    VkPresentModeKHR selected_present_mode = VK_PRESENT_MODE_FIFO_KHR;
    for (u32 i = 0; i < surface_present_modes->count; ++i) {
        VkPresentModeKHR surface_present_mode = surface_present_modes->data[i];

        // Mailbox is the preferred present mode if available.
        if (surface_present_mode == VK_PRESENT_MODE_MAILBOX_KHR) {
            selected_present_mode = surface_present_mode;
            break;
        }
    }

    // Set image count to min image count + 1 or max image count (whichever is smaller).
    u32 selected_image_count = surface_capabilities.minImageCount + 1;
    if (surface_capabilities.maxImageCount > 0 && selected_image_count > surface_capabilities.maxImageCount)
        selected_image_count = surface_capabilities.maxImageCount;

    // Verify current extent has been set for surface.
    if (surface_capabilities.currentExtent.width == UINT32_MAX)
        CTK_FATAL("current extent not set for surface")

    ////////////////////////////////////////////////////////////
    /// Creation
    ////////////////////////////////////////////////////////////
    u32 graphics_queue_family_idx = physical_device->queue_family_idxs.graphics;
    u32 present_queue_family_idx = physical_device->queue_family_idxs.present;
    u32 queue_family_idxs[] = {
        graphics_queue_family_idx,
        present_queue_family_idx,
    };

    VkSwapchainCreateInfoKHR info = {};
    info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    info.flags = 0;
    info.surface = surface;
    info.minImageCount = selected_image_count;
    info.imageFormat = selected_format.format;
    info.imageColorSpace = selected_format.colorSpace;
    info.imageExtent = surface_capabilities.currentExtent;
    info.imageArrayLayers = 1; // Always 1 for standard images.
    info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                      VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    info.preTransform = surface_capabilities.currentTransform;
    info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    info.presentMode = selected_present_mode;
    info.clipped = VK_TRUE;
    info.oldSwapchain = VK_NULL_HANDLE;
    if (graphics_queue_family_idx != present_queue_family_idx) {
        info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        info.queueFamilyIndexCount = CTK_ARRAY_SIZE(queue_family_idxs);
        info.pQueueFamilyIndices = queue_family_idxs;
    }
    else {
        info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        info.queueFamilyIndexCount = 0;
        info.pQueueFamilyIndices = NULL;
    }
    validate(vkCreateSwapchainKHR(device, &info, NULL, &swapchain->handle), "failed to create swapchain");

    // Store surface state used to create swapchain for future reference.
    swapchain->image_format = selected_format.format;
    swapchain->extent = surface_capabilities.currentExtent;

    ////////////////////////////////////////////////////////////
    /// Image View Creation
    ////////////////////////////////////////////////////////////
    auto swap_imgs = load_vk_objects<VkImage>(&temp_mem, vkGetSwapchainImagesKHR, device, swapchain->handle);

    CTK_ASSERT(swap_imgs->count <= get_size(&swapchain->image_views));
    swapchain->image_views.count = swap_imgs->count;
    swapchain->image_count = swap_imgs->count;

    for (u32 i = 0; i < swap_imgs->count; ++i) {
        VkImageViewCreateInfo view_info = {};
        view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        view_info.flags = 0;
        view_info.image = swap_imgs->data[i];
        view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
        view_info.format = swapchain->image_format;
        view_info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        view_info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        view_info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        view_info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        view_info.subresourceRange.baseMipLevel = 0;
        view_info.subresourceRange.levelCount = 1;
        view_info.subresourceRange.baseArrayLayer = 0;
        view_info.subresourceRange.layerCount = 1;
        validate(vkCreateImageView(device, &view_info, NULL, swapchain->image_views.data + i),
                 "failed to create image view");
    }

    return swapchain;
}

static VkCommandPool create_cmd_pool(VkDevice device, u32 queue_fam_idx) {
    VkCommandPoolCreateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    info.queueFamilyIndex = queue_fam_idx;

    VkCommandPool cmd_pool = VK_NULL_HANDLE;
    validate(vkCreateCommandPool(device, &info, NULL, &cmd_pool), "failed to create command pool");

    return cmd_pool;
}

////////////////////////////////////////////////////////////
/// Memory
////////////////////////////////////////////////////////////
static VkDeviceMemory allocate_device_memory(VkDevice device, VkDeviceSize size, u32 type_index) {
    VkMemoryAllocateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    info.allocationSize = size;
    info.memoryTypeIndex = type_index;
    VkDeviceMemory mem = VK_NULL_HANDLE;
    validate(vkAllocateMemory(device, &info, NULL, &mem), "failed to allocate memory");
    return mem;
}

static Buffer *create_buffer(Buffer *buffer, VkDevice device, PhysicalDevice *physical_device, BufferInfo info) {
    VkBufferCreateInfo create_info = {};
    create_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    create_info.size = info.size;
    create_info.usage = info.usage_flags;
    create_info.sharingMode = info.sharing_mode;
    create_info.queueFamilyIndexCount = 0;
    create_info.pQueueFamilyIndices = NULL; // Ignored if sharingMode is not VK_SHARING_MODE_CONCURRENT.
    validate(vkCreateBuffer(device, &create_info, NULL, &buffer->handle), "failed to create buffer");

    // Allocate / Bind Memory
    VkMemoryRequirements mem_reqs = {};
    vkGetBufferMemoryRequirements(device, buffer->handle, &mem_reqs);

    buffer->size = mem_reqs.size;
    u32 type_index = find_memory_type_index(mem_reqs, physical_device, info.mem_property_flags);

    buffer->mem = allocate_device_memory(device, mem_reqs.size, type_index);
    validate(vkBindBufferMemory(device, buffer->handle, buffer->mem, 0), "failed to bind buffer memory");

    return buffer;
}

// static Region *allocate_region(Region *region, Buffer *buffer, u32 size, VkDeviceSize align) {
//     VkDeviceSize align_offset = buffer->end % align;
//     region->buffer = buffer;
//     region->offset = align_offset ? buffer->end - align_offset + align : buffer->end;

//     if (region->offset + size > buffer->size) {
//         CTK_FATAL("buffer (size=%u end=%u) cannot allocate region of size %u and alignment %u (only %u bytes left)",
//                   buffer->size, buffer->end, size, align, buffer->size - buffer->end);
//     }

//     region->size = size;
//     buffer->end = region->offset + region->size;

//     return region;
// }

// static Region *
// allocate_uniform_buffer_region(Region *region, Buffer *buffer, u32 size, PhysicalDevice *physical_device) {
//     return allocate_region(region, buffer, size, physical_device->min_uniform_buffer_offset_alignment);
// }

// static void write_to_host_region(VkDevice device, Region *region, u32 offset, void *data, u32 size) {
//     CTK_ASSERT(size <= region->size);
//     void *mapped_mem = NULL;
//     vkMapMemory(device, region->buffer->mem, region->offset + offset, size, 0, &mapped_mem);
//     memcpy(mapped_mem, data, size);
//     vkUnmapMemory(device, region->buffer->mem);
// }

// static void
// write_to_device_region(VkDevice device, VkCommandBuffer cmd_buf,
//                        Region *staging_region, u32 staging_offset,
//                        Region *region, u32 offset,
//                        void *data, u32 size)
// {
//     write_to_host_region(device, staging_region, staging_offset, data, size);

//     VkBufferCopy copy = {};
//     copy.srcOffset = staging_region->offset + staging_offset;
//     copy.dstOffset = region->offset + offset;
//     copy.size = size;

//     vkCmdCopyBuffer(cmd_buf, staging_region->buffer->handle, region->buffer->handle, 1, &copy);
// }

static void write_to_buffer(VkDevice device, BufferWriteInfo info) {
    void *mapped_mem = NULL;
    vkMapMemory(device, info.buffer->mem, info.offset, info.size, 0, &mapped_mem);
    memcpy(mapped_mem, info.data, info.size);
    vkUnmapMemory(device, info.buffer->mem);
}

static void copy_to_buffer(VkDevice device, VkCommandBuffer cmd_buf, BufferCopyInfo copy_info) {
    VkBufferCopy copy = {};
    copy.srcOffset = copy_info.src_offset;
    copy.dstOffset = copy_info.dst_offset;
    copy.size = copy_info.size;

    vkCmdCopyBuffer(cmd_buf, copy_info.src_buffer->handle, copy_info.dst_buffer->handle, 1, &copy);
}

static Image *create_image(Image *image, VkDevice device, PhysicalDevice *physical_device, ImageInfo info) {
    validate(vkCreateImage(device, &info.image, NULL, &image->handle), "failed to create image");

    image->extent = info.image.extent;

    // Allocate / Bind Memory
    VkMemoryRequirements mem_reqs = {};
    vkGetImageMemoryRequirements(device, image->handle, &mem_reqs);

    u32 type_index = find_memory_type_index(mem_reqs, physical_device, info.mem_property_flags);

    image->mem = allocate_device_memory(device, mem_reqs.size, type_index);
    validate(vkBindImageMemory(device, image->handle, image->mem, 0), "failed to bind image memory");

    info.view.image = image->handle;
    validate(vkCreateImageView(device, &info.view, NULL, &image->view), "failed to create image view");

    return image;
}

static void image_memory_barrier(VkCommandBuffer cmd_buf, Image *image, ImageMemoryBarrier image_memory_barrier) {
    VkImageMemoryBarrier vk_image_memory_barrier = {};
    vk_image_memory_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    vk_image_memory_barrier.srcAccessMask = image_memory_barrier.src.access;
    vk_image_memory_barrier.dstAccessMask = image_memory_barrier.dst.access;
    vk_image_memory_barrier.oldLayout = image_memory_barrier.src.layout;
    vk_image_memory_barrier.newLayout = image_memory_barrier.dst.layout;
    vk_image_memory_barrier.srcQueueFamilyIndex = image_memory_barrier.src.queue_family_index;
    vk_image_memory_barrier.dstQueueFamilyIndex = image_memory_barrier.src.queue_family_index;
    vk_image_memory_barrier.image = image->handle;
    vk_image_memory_barrier.subresourceRange = image_memory_barrier.subresource_range;

    vkCmdPipelineBarrier(cmd_buf,
                         image_memory_barrier.src.stage,
                         image_memory_barrier.dst.stage,
                         0,                             // Dependency Flags
                         0, NULL,                       // Memory Barriers
                         0, NULL,                       // Buffer Memory Barriers
                         1, &vk_image_memory_barrier);  // Image Memory Barriers
}

static void copy_to_image(VkCommandBuffer cmd_buf, Buffer *buffer, u32 offset, Image *image) {
    image_memory_barrier(cmd_buf, image, {
        .src = {
            .layout = VK_IMAGE_LAYOUT_UNDEFINED,
            .stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
            .access = 0,
            .queue_family_index = VK_QUEUE_FAMILY_IGNORED,
        },
        .dst = {
            .layout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            .stage = VK_PIPELINE_STAGE_TRANSFER_BIT,
            .access = VK_ACCESS_TRANSFER_WRITE_BIT,
            .queue_family_index = VK_QUEUE_FAMILY_IGNORED,
        },
        .subresource_range = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1,
        },
    });

    VkBufferImageCopy copy = {};
    copy.bufferOffset = offset;
    copy.bufferRowLength = 0;
    copy.bufferImageHeight = 0;
    copy.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    copy.imageSubresource.mipLevel = 0;
    copy.imageSubresource.baseArrayLayer = 0;
    copy.imageSubresource.layerCount = 1;
    copy.imageOffset.x = 0;
    copy.imageOffset.y = 0;
    copy.imageOffset.z = 0;
    copy.imageExtent = image->extent;
    vkCmdCopyBufferToImage(cmd_buf, buffer->handle, image->handle, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);

    image_memory_barrier(cmd_buf, image, {
        .src = {
            .layout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            .stage = VK_PIPELINE_STAGE_TRANSFER_BIT,
            .access = VK_ACCESS_TRANSFER_WRITE_BIT,
            .queue_family_index = VK_QUEUE_FAMILY_IGNORED,
        },
        .dst = {
            .layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            .stage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            .access = VK_ACCESS_SHADER_READ_BIT,
            .queue_family_index = VK_QUEUE_FAMILY_IGNORED,
        },
        .subresource_range = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1,
        },
    });
}

static VkSampler create_sampler(VkDevice device, VkSamplerCreateInfo info) {
    VkSampler sampler = VK_NULL_HANDLE;
    validate(vkCreateSampler(device, &info, NULL, &sampler), "failed to create sampler");
    return sampler;
}

////////////////////////////////////////////////////////////
/// Resource Creation
////////////////////////////////////////////////////////////
static u32 push_attachment(RenderPassInfo *info, AttachmentInfo attachment_info) {
    if (info->attachment.descriptions->count == info->attachment.descriptions->size)
        CTK_FATAL("cannot push any more attachments to RenderPassInfo");

    u32 attachment_index = info->attachment.descriptions->count;

    push(info->attachment.descriptions, attachment_info.description);
    push(info->attachment.clear_values, attachment_info.clear_value);

    return attachment_index;
}

static RenderPass *create_render_pass(Memory temp_mem, Memory *perm_mem, RenderPass *render_pass,
                                      VkDevice device, RenderPassInfo *info)
{

    render_pass->attachment_clear_values = create_array<VkClearValue>(perm_mem, info->attachment.clear_values->count);

    // Clear Values
    concat(render_pass->attachment_clear_values, info->attachment.clear_values);

    // Subpass Descriptions
    auto subpass_descriptions = create_array<VkSubpassDescription>(&temp_mem, info->subpass.infos->count);
    for (u32 i = 0; i < info->subpass.infos->count; ++i) {
        SubpassInfo *subpass_info = info->subpass.infos->data + i;
        VkSubpassDescription *description = push(subpass_descriptions);
        description->pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;

        // Input Attachments
        if (subpass_info->input_attachment_refs) {
            description->inputAttachmentCount = subpass_info->input_attachment_refs->count;
            description->pInputAttachments = subpass_info->input_attachment_refs->data;
        }

        // Color/Resolve/Depth Attachments
        if (subpass_info->color_attachment_refs) {
            description->colorAttachmentCount = subpass_info->color_attachment_refs->count;
            description->pColorAttachments = subpass_info->color_attachment_refs->data;
        }

        description->pResolveAttachments = NULL;

        // if (subpass_info->depth_attachment_ref)
            description->pDepthStencilAttachment = NULL;//&subpass_info->depth_attachment_ref;

        // Preserve Attachments
        if (subpass_info->preserve_attachment_indexes) {
            description->preserveAttachmentCount = subpass_info->preserve_attachment_indexes->count;
            description->pPreserveAttachments = subpass_info->preserve_attachment_indexes->data;
        }
    }

    // Render Pass
    VkRenderPassCreateInfo create_info = {};
    create_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    create_info.attachmentCount = info->attachment.descriptions->count;
    create_info.pAttachments = info->attachment.descriptions->data;
    create_info.subpassCount = subpass_descriptions->count;
    create_info.pSubpasses = subpass_descriptions->data;
    create_info.dependencyCount = info->subpass.dependencies->count;
    create_info.pDependencies = info->subpass.dependencies->data;
    validate(vkCreateRenderPass(device, &create_info, NULL, &render_pass->handle), "failed to create render pass");

    return render_pass;
}

static VkDescriptorPool create_descriptor_pool(VkDevice device, DescriptorPoolInfo info) {
    FixedArray<VkDescriptorPoolSize, 4> pool_sizes = {};

    if (info.descriptor_count.uniform_buffer) {
        push(&pool_sizes, {
            .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .descriptorCount = info.descriptor_count.uniform_buffer,
        });
    }

    if (info.descriptor_count.uniform_buffer_dynamic) {
        push(&pool_sizes, {
            .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
            .descriptorCount = info.descriptor_count.uniform_buffer_dynamic,
        });
    }

    if (info.descriptor_count.combined_image_sampler) {
        push(&pool_sizes, {
            .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .descriptorCount = info.descriptor_count.combined_image_sampler,
        });
    }

    if (info.descriptor_count.input_attachment) {
        push(&pool_sizes, {
            .type = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,
            .descriptorCount = info.descriptor_count.input_attachment,
        });
    }

    VkDescriptorPoolCreateInfo pool_info = {};
    pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.flags = 0;
    pool_info.maxSets = info.max_descriptor_sets;
    pool_info.poolSizeCount = pool_sizes.count;
    pool_info.pPoolSizes = pool_sizes.data;
    VkDescriptorPool pool = VK_NULL_HANDLE;
    validate(vkCreateDescriptorPool(device, &pool_info, NULL, &pool), "failed to create descriptor pool");

    return pool;
}

static VkDescriptorSetLayout create_descriptor_set_layout(VkDevice device, VkDescriptorSetLayoutBinding *bindings,
                                                          u32 binding_count)
{
    VkDescriptorSetLayoutCreateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    info.bindingCount = binding_count;
    info.pBindings = bindings;
    VkDescriptorSetLayout layout = VK_NULL_HANDLE;
    validate(vkCreateDescriptorSetLayout(device, &info, NULL, &layout), "error creating descriptor set layout");

    return layout;
}

static VkDescriptorSetLayout create_descriptor_set_layout(Memory temp_mem, VkDevice device,
                                                          DescriptorInfo *descriptor_infos, u32 count)
{
    auto bindings = create_array<VkDescriptorSetLayoutBinding>(&temp_mem, count);

    for (u32 i = 0; i < count; ++i) {
        DescriptorInfo *info = descriptor_infos + i;

        push(bindings, {
            .binding = i,
            .descriptorType = info->type,
            .descriptorCount = info->count,
            .stageFlags = info->stage,
            .pImmutableSamplers = NULL,
        });
    }

    VkDescriptorSetLayout layout = create_descriptor_set_layout(device, bindings->data, bindings->count);

    return layout;
}

static void allocate_descriptor_sets(Memory temp_mem, VkDevice device, VkDescriptorPool pool,
                                     VkDescriptorSetLayout layout, u32 count, VkDescriptorSet *descriptor_sets)
{
    auto layouts = create_array<VkDescriptorSetLayout>(&temp_mem, count);

    CTK_REPEAT(count)
        push(layouts, layout);

    VkDescriptorSetAllocateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    info.descriptorPool = pool;
    info.descriptorSetCount = count;
    info.pSetLayouts = layouts->data;
    validate(vkAllocateDescriptorSets(device, &info, descriptor_sets), "failed to allocate descriptor sets");
}

static VkDescriptorSet allocate_descriptor_set(Memory temp_mem, VkDevice device, VkDescriptorPool pool,
                                               VkDescriptorSetLayout layout)
{
    VkDescriptorSet descriptor_set = VK_NULL_HANDLE;
    allocate_descriptor_sets(temp_mem, device, pool, layout, 1, &descriptor_set);
    return descriptor_set;
}

struct DescriptorBinding {
    VkDescriptorType type;

    union {
        VkDescriptorBufferInfo buffer;
        VkDescriptorImageInfo image;
    };
};

static void update_descriptor_set(Memory temp_mem, VkDevice device, VkDescriptorSet descriptor_set,
                                  DescriptorBinding *bindings, u32 binding_count)
{
    // auto buf_infos = create_array<VkDescriptorBufferInfo>(temp_mem, binding_count);
    // auto img_infos = create_array<VkDescriptorImageInfo>(temp_mem, binding_count);
    auto writes = create_array<VkWriteDescriptorSet>(&temp_mem, binding_count);

    for (u32 i = 0; i < binding_count; ++i) {
        DescriptorBinding *binding = bindings + i;

        VkWriteDescriptorSet *write = push(writes);
        write->sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write->dstSet = descriptor_set;
        write->dstBinding = i;
        write->dstArrayElement = 0;
        write->descriptorCount = 1;
        write->descriptorType = binding->type;

        if (binding->type == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER ||
            binding->type == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC ||
            binding->type == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER)
        {
            write->pBufferInfo = &bindings[i].buffer;
            // VkDescriptorBufferInfo *info = push(buf_infos);
            // info->buffer = binding->uniform_buffer->buffer;
            // info->offset = binding->uniform_buffer->offset;
            // info->range = binding->uniform_buffer->size;

            // write->pBufferInfo = info;
        }
        else if (binding->type == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER) {
            write->pImageInfo = &bindings[i].image;
            // VkDescriptorImageInfo *info = push(img_infos);
            // info->sampler = binding->combined_image_sampler.sampler;
            // info->imageView = binding->combined_image_sampler.image->view;
            // info->imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

            // write->pImageInfo = info;
        }
        else {
            CTK_FATAL("unhandled descriptor type when updating descriptor set");
        }
    }

    vkUpdateDescriptorSets(device, writes->count, writes->data, 0, NULL);
}

static VkFramebuffer create_framebuffer(VkDevice device, VkRenderPass rp, FramebufferInfo *info) {
    VkFramebufferCreateInfo create_info = {};
    create_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    create_info.renderPass = rp;
    create_info.attachmentCount = info->attachments->count;
    create_info.pAttachments = info->attachments->data;
    create_info.width = info->extent.width;
    create_info.height = info->extent.height;
    create_info.layers = info->layers;
    VkFramebuffer fb = VK_NULL_HANDLE;
    validate(vkCreateFramebuffer(device, &create_info, NULL, &fb), "failed to create framebuffer");
    return fb;
}

static VkSemaphore create_semaphore(VkDevice device) {
    VkSemaphoreCreateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    info.pNext = NULL;
    info.flags = 0;
    VkSemaphore semaphore = VK_NULL_HANDLE;
    validate(vkCreateSemaphore(device, &info, NULL, &semaphore), "failed to create semaphore");

    return semaphore;
}

static VkFence create_fence(VkDevice device) {
    VkFenceCreateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    info.pNext = NULL;
    info.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    VkFence fence = VK_NULL_HANDLE;
    validate(vkCreateFence(device, &info, NULL, &fence), "failed to create fence");

    return fence;
}

static void allocate_cmd_bufs(VkDevice device, VkCommandPool pool, VkCommandBufferLevel level,
                              VkCommandBuffer *cmd_bufs, u32 count)
{
    VkCommandBufferAllocateInfo alloc_info = {};
    alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
    alloc_info.commandPool = pool;
    alloc_info.level = level;
    alloc_info.commandBufferCount = count;
    validate(vkAllocateCommandBuffers(device, &alloc_info, cmd_bufs), "failed to allocate command buffer");
}

////////////////////////////////////////////////////////////
/// Command Buffer
////////////////////////////////////////////////////////////
static void begin_temp_cmd_buf(VkCommandBuffer cmd_buf) {
    VkCommandBufferBeginInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    info.pInheritanceInfo = NULL;
    vkBeginCommandBuffer(cmd_buf, &info);
}

static void submit_temp_cmd_buf(VkCommandBuffer cmd_buf, VkQueue queue) {
    vkEndCommandBuffer(cmd_buf);
    VkSubmitInfo submit_info = {};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &cmd_buf;
    validate(vkQueueSubmit(queue, 1, &submit_info, VK_NULL_HANDLE), "failed to submit temp command buffer");
    vkQueueWaitIdle(queue);
}

////////////////////////////////////////////////////////////
/// Rendering
////////////////////////////////////////////////////////////
static u32 next_swap_img_idx(VkDevice device, VkSwapchainKHR swapchain, VkSemaphore semaphore, VkFence fence) {
    u32 img_idx = U32_MAX;

    validate(vkAcquireNextImageKHR(device, swapchain, U64_MAX, semaphore, fence, &img_idx),
             "failed to aquire next swapchain image");

    return img_idx;
}

}
