#pragma once

#define VK_USE_PLATFORM_WIN32_KHR
#include <vulkan/vulkan.h>

#include "ctk/ctk.h"
#include "ctk/memory.h"
#include "ctk/containers.h"
#include "ctk/file.h"
#include "noise_test/vtk.h"
#include "stk/stk.h"

using namespace ctk;
using namespace vtk;
using namespace stk;

////////////////////////////////////////////////////////////
/// Data
////////////////////////////////////////////////////////////
struct GraphicsMemory {
    Buffer *buffer;
    VkDeviceSize offset;
    VkDeviceSize size;

    enum struct Type {
        HOST,
        DEVICE,
    } type;
};

struct GraphicsStack {
    GraphicsMemory *mem;
    VkDeviceSize count;
};

template<typename Type>
struct GraphicsArray {
    GraphicsMemory *mem;
    VkDeviceSize count;
    VkDeviceSize size;
};

struct Shader {
    VkShaderModule handle;
    VkShaderStageFlagBits stage;
};

struct ShaderGroup {
    Shader *vert;
    Shader *frag;
};

static constexpr u32 MAX_PIPELINE_SHADER_STAGES = 8;

struct PipelineInfo {
    FixedArray<Shader *,                            MAX_PIPELINE_SHADER_STAGES> shaders;
    FixedArray<VkPipelineColorBlendAttachmentState, 4>  color_blend_attachments;
    FixedArray<VkDescriptorSetLayout,               16> descriptor_set_layouts;
    FixedArray<VkPushConstantRange,                 4>  push_constant_ranges;
    FixedArray<VkVertexInputBindingDescription,     4>  vertex_bindings;
    FixedArray<VkVertexInputAttributeDescription,   4>  vertex_attributes;
    FixedArray<VkViewport,                          4>  viewports;
    FixedArray<VkRect2D,                            4>  scissors;
    FixedArray<VkDynamicState,                      4>  dynamic_states;

    VkPipelineInputAssemblyStateCreateInfo input_assembly;
    VkPipelineDepthStencilStateCreateInfo  depth_stencil;
    VkPipelineRasterizationStateCreateInfo rasterization;
    VkPipelineMultisampleStateCreateInfo   multisample;
    VkPipelineColorBlendStateCreateInfo    color_blend;
};

struct DescriptorSet {
    static constexpr u32 MAX_HANDLES = 4;

    VkDescriptorSetLayout layout;
    FixedArray<VkDescriptorSet, MAX_HANDLES> handles;
};

struct Pipeline {
    VkPipeline handle;
    VkPipelineLayout layout;
};

struct Frame {
    VkSemaphore img_aquired;
    VkSemaphore render_finished;
    VkFence in_flight;
};

struct Transform {
    Vec3<f32> position;
    Vec3<f32> rotation;
    Vec3<f32> scale;
};

struct Vertex {
    Vec3<f32> position;
    Vec2<f32> uv;
};

struct MeshInfo {
    u32 max_vertex_count;
    u32 max_index_count;
};

struct Mesh {
    Array<Vertex> *vertexes;
    Array<u32> *indexes;
    u32 vertex_offset;
    u32 index_offset;
};

struct View {
    Transform transform;
    PerspectiveInfo perspective_info;
    f32 max_x_angle;
};

struct Graphics {
    struct {
        Memory *perm;
        Memory *temp;
    } mem;

    // Vulkan
    Instance *instance;
    VkSurfaceKHR surface;
    PhysicalDevice *physical_device;
    VkDevice device;

    struct {
        VkQueue graphics;
        VkQueue present;
    } queue;

    Swapchain *swapchain;
    VkCommandPool main_cmd_pool;
    VkCommandBuffer temp_cmd_buf;

    struct {
        GraphicsStack *host;
        GraphicsStack *device;
        GraphicsArray<u8> *staging;
    } gfx_mem;

    RenderPass *render_pass;
    Array<VkFramebuffer> *framebuffers;
    Array<VkCommandBuffer> *primary_render_cmd_bufs;

    struct {
        u32 swap_img_idx;
        Array<Frame> *frames;
        Frame *frame;
        u32 frame_idx;
    } sync;

    // Render State
    struct {
        GraphicsArray<Vertex> *vertexes;
        GraphicsArray<u32> *indexes;
    } mesh_data;

    struct {
        ShaderGroup test;
        ShaderGroup texture;
    } shader;

    struct {
        Image *display;
    } image;

    struct {
        VkSampler nearest;
    } sampler;

    VkDescriptorPool descriptor_pool;

    struct {
        DescriptorSet *texture;
    } descriptor_set;

    struct {
        Pipeline *test;
        Pipeline *texture;
    } pipeline;
};

#include "noise_test/graphics_defaults.h"

////////////////////////////////////////////////////////////
/// Utils
////////////////////////////////////////////////////////////
static GraphicsStack *create_graphics_stack(Graphics *gfx, BufferInfo buffer_info) {
    auto mem = allocate<GraphicsMemory>(gfx->mem.perm, 1);
    mem->buffer = create_buffer(allocate<Buffer>(gfx->mem.perm, 1), gfx->device, gfx->physical_device, buffer_info);
    mem->offset = 0;
    mem->size = buffer_info.size;
    if ((buffer_info.mem_property_flags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) == VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
        mem->type = GraphicsMemory::Type::DEVICE;
    else
        mem->type = GraphicsMemory::Type::HOST;

    auto stack = allocate<GraphicsStack>(gfx->mem.perm, 1);
    stack->mem = mem;
    stack->count = 0;

    return stack;
}

static GraphicsMemory *allocate(Graphics *gfx, GraphicsStack *stack, VkDeviceSize size, VkDeviceSize align) {
    VkDeviceSize align_overflow = stack->count % align;
    VkDeviceSize align_offset = stack->count + (align_overflow ? align - align_overflow : 0);

    if (align_offset + size > stack->mem->size) {
        CTK_FATAL("allocating %u bytes aligned by %u to offset %u on stack (size=%u) would overflow by %u bytes",
                  size, align, align_offset, stack->mem->size, align_offset + size - stack->mem->size);
    }

    stack->count = align_offset + size;

    auto mem = allocate<GraphicsMemory>(gfx->mem.perm, 1);
    mem->buffer = stack->mem->buffer;
    mem->offset = align_offset;
    mem->size = size;
    mem->type = stack->mem->type;
    return mem;
}

template<typename Type>
static GraphicsArray<Type> *create_graphics_array(Graphics *gfx, GraphicsStack *stack, VkDeviceSize size,
                                                  VkDeviceSize align)
{
    auto array = allocate<GraphicsArray<Type>>(gfx->mem.perm, 1);
    array->mem = allocate(gfx, stack, size * sizeof(Type), align);
    array->count = 0;
    array->size = size;
    return array;
}

template<typename Type>
static VkDeviceSize count_offset(GraphicsArray<Type> *array) {
    return array->mem->offset + (array->count * sizeof(Type));
}

template<typename Type>
static VkDeviceSize push(Graphics *gfx, GraphicsArray<Type> *array, void *data, VkDeviceSize count) {
    if (array->count + count > array->size)
        CTK_FATAL("pushing %u elements to array would overflow by %u", count, array->count + count - array->size);

    VkDeviceSize data_byte_count = count * sizeof(Type);

    if (array->mem->type == GraphicsMemory::Type::HOST) {
        write_to_buffer(gfx->device, {
            .buffer = array->mem->buffer,
            .offset = count_offset(array),
            .data = data,
            .size = data_byte_count,
        });
    }
    else {
        VkDeviceSize staging_offset = push(gfx, gfx->gfx_mem.staging, data, data_byte_count);
        copy_to_buffer(gfx->device, gfx->temp_cmd_buf, {
            .src_buffer = gfx->gfx_mem.staging->mem->buffer,
            .src_offset = staging_offset,
            .dst_buffer = array->mem->buffer,
            .dst_offset = count_offset(array),
            .size = data_byte_count,
        });
    }

    // Starting index of data is returned.
    VkDeviceSize data_start = array->count;
    array->count += count;
    return data_start;
}

template<typename Type>
static void clear(GraphicsArray<Type> *array) {
    array->count = 0;
}

static void create_graphics_memory(Graphics *gfx) {
    gfx->gfx_mem.host = create_graphics_stack(gfx, {
        .size = megabyte(256),
        .sharing_mode = VK_SHARING_MODE_EXCLUSIVE,
        .usage_flags = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT |
                       // VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
                       // VK_BUFFER_USAGE_INDEX_BUFFER_BIT |
                       VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        .mem_property_flags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                              VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
    });

    gfx->gfx_mem.device = create_graphics_stack(gfx, {
        .size = megabyte(512),
        .sharing_mode = VK_SHARING_MODE_EXCLUSIVE,
        .usage_flags = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT |
                       VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
                       VK_BUFFER_USAGE_INDEX_BUFFER_BIT |
                       VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        .mem_property_flags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
    });

    gfx->gfx_mem.staging = create_graphics_array<u8>(gfx, gfx->gfx_mem.host, megabyte(128), 1);
}

static void create_render_passes(Graphics *gfx) {
    Memory temp_mem = *gfx->mem.temp;

    RenderPassInfo info = {
        .attachment = {
            .descriptions = create_array<VkAttachmentDescription>(&temp_mem, 1),
            .clear_values = create_array<VkClearValue>(&temp_mem, 1),
        },
        .subpass = {
            .infos = create_array<SubpassInfo>(&temp_mem, 1),
            .dependencies = create_array<VkSubpassDependency>(&temp_mem, 1),
        },
    };

    // Swapchain Image Attachment
    u32 swapchain_attachment_index = push_attachment(&info, {
        .description = {
            .flags = 0,
            .format = gfx->swapchain->image_format,
            .samples = VK_SAMPLE_COUNT_1_BIT,

            .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
            .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
            .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
            .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,

            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
        },
        .clear_value = { 0.1f, 0.1f, 0.1f, 1 },
    });

    // Subpasses
    SubpassInfo *subpass_info = push(info.subpass.infos);
    subpass_info->color_attachment_refs = create_array<VkAttachmentReference>(gfx->mem.temp, 1);
    push(subpass_info->color_attachment_refs, {
        .attachment = swapchain_attachment_index,
        .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
    });

    // push(info.subpass.dependencies, {
    //     .srcSubpass = 0,
    //     .dstSubpass = VK_SUBPASS_EXTERNAL,
    //     .srcStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
    //     .dstStageMask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
    //     .srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
    //                      VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
    //     .dstAccessMask = VK_ACCESS_MEMORY_READ_BIT,
    //     .dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT,
    // });

    gfx->render_pass = create_render_pass(&temp_mem, gfx->mem.perm, allocate<RenderPass>(gfx->mem.perm, 1),
                                          gfx->device, &info);
}

static void create_framebuffers(Graphics *gfx) {
    gfx->framebuffers = create_array<VkFramebuffer>(gfx->mem.perm, gfx->swapchain->image_count);

    // Create framebuffer for each swapchain image.
    for (u32 i = 0; i < gfx->swapchain->image_views.count; ++i) {
        push_frame(gfx->mem.temp);

        FramebufferInfo info = {};

        info.attachments = create_array<VkImageView>(gfx->mem.temp, 1),
        push(info.attachments, gfx->swapchain->image_views[i]);

        info.extent = get_surface_extent(gfx->physical_device, gfx->surface);
        info.layers = 1;

        push(gfx->framebuffers, create_framebuffer(gfx->device, gfx->render_pass->handle, &info));

        pop_frame(gfx->mem.temp);
    }
}

static void create_primary_render_cmd_bufs(Graphics *gfx) {
    gfx->primary_render_cmd_bufs = create_array_full<VkCommandBuffer>(gfx->mem.perm, gfx->swapchain->image_count);
    allocate_cmd_bufs(gfx->device, gfx->main_cmd_pool, VK_COMMAND_BUFFER_LEVEL_PRIMARY,
                      gfx->primary_render_cmd_bufs->data, gfx->primary_render_cmd_bufs->count);
}

static void init_sync(Graphics *gfx, u32 frame_count) {
    gfx->sync.frame_idx = U32_MAX;
    gfx->sync.frames = create_array<Frame>(gfx->mem.perm, frame_count);

    for (u32 i = 0; i < frame_count; ++i) {
        push(gfx->sync.frames, {
            .img_aquired = create_semaphore(gfx->device),
            .render_finished = create_semaphore(gfx->device),
            .in_flight = create_fence(gfx->device),
        });
    }
}

static void create_vulkan_state(Graphics *gfx, Window *window) {
    // Instance
    gfx->instance = create_instance(allocate<Instance>(gfx->mem.perm, 1), { .enable_validation = true });
    gfx->surface = create_win32_surface(gfx->instance, window->handle, window->instance);

    // Devices
    auto requested_feature = PhysicalDeviceFeature::geometryShader;
    gfx->physical_device = create_physical_device(*gfx->mem.temp, allocate<PhysicalDevice>(gfx->mem.perm, 1),
                                                  gfx->instance, gfx->surface, &requested_feature, 1);
    gfx->device = create_device(gfx->physical_device, &requested_feature, 1);

    // Queues
    gfx->queue.graphics = get_queue(gfx->device, gfx->physical_device->queue_family_idxs.graphics, 0);
    gfx->queue.present = get_queue(gfx->device, gfx->physical_device->queue_family_idxs.present, 0);

    // Swapchain
    gfx->swapchain = create_swapchain(*gfx->mem.temp, allocate<Swapchain>(gfx->mem.perm, 1), gfx->device,
                                      gfx->physical_device, gfx->surface);

    // Command State
    gfx->main_cmd_pool = create_cmd_pool(gfx->device, gfx->physical_device->queue_family_idxs.graphics);
    allocate_cmd_bufs(gfx->device, gfx->main_cmd_pool, VK_COMMAND_BUFFER_LEVEL_PRIMARY, &gfx->temp_cmd_buf, 1);

    create_graphics_memory(gfx);
    create_render_passes(gfx);
    create_framebuffers(gfx);
    create_primary_render_cmd_bufs(gfx);
    init_sync(gfx, 2);
}

static void create_mesh_data(Graphics *gfx) {
    gfx->mesh_data.vertexes = create_graphics_array<Vertex>(gfx, gfx->gfx_mem.device, 1024, 16);
    gfx->mesh_data.indexes = create_graphics_array<u32>(gfx, gfx->gfx_mem.device, 4096, 16);
}

static Shader *create_shader(Graphics *gfx, cstr spirv_path, VkShaderStageFlagBits stage) {
    push_frame(gfx->mem.temp);

    Array<u8> *bytecode = read_file<u8>(gfx->mem.temp, spirv_path);

    if (bytecode == NULL)
        CTK_FATAL("failed to load bytecode from \"%s\"", spirv_path);

    auto shader = allocate<Shader>(gfx->mem.perm, 1);
    shader->stage = stage;

    VkShaderModuleCreateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    info.flags = 0;
    info.codeSize = byte_size(bytecode);
    info.pCode = (const u32 *)bytecode->data;
    validate(vkCreateShaderModule(gfx->device, &info, NULL, &shader->handle),
             "failed to create shader from SPIR-V bytecode in \"%p\"", spirv_path);

    pop_frame(gfx->mem.temp);
    return shader;
}

static void create_shaders(Graphics *gfx) {
    gfx->shader.test.vert = create_shader(gfx, "shaders/test.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
    gfx->shader.test.frag = create_shader(gfx, "shaders/test.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);

    gfx->shader.texture.vert = create_shader(gfx, "shaders/texture.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
    gfx->shader.texture.frag = create_shader(gfx, "shaders/texture.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
}

static void transition_image_layout(Graphics *gfx, Image *image, VkImageLayout src, VkImageLayout dst) {
    begin_temp_cmd_buf(gfx->temp_cmd_buf);
        image_memory_barrier(gfx->temp_cmd_buf, image, {
            .src = {
                .layout = src,
                .stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                .access = 0,
                .queue_family_index = VK_QUEUE_FAMILY_IGNORED,
            },
            .dst = {
                .layout = dst,
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
    submit_temp_cmd_buf(gfx->temp_cmd_buf, gfx->queue.graphics);
}

static void create_images(Graphics *gfx) {
    gfx->image.display = create_image(allocate<Image>(gfx->mem.perm, 1), gfx->device, gfx->physical_device, {
        .image = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
            .flags = 0,
            .imageType = VK_IMAGE_TYPE_2D,
            .format = VK_FORMAT_R8G8B8A8_UNORM,
            .extent = {
                .width = gfx->swapchain->extent.width,
                .height = gfx->swapchain->extent.height,
                .depth = 1,
            },
            .mipLevels = 1,
            .arrayLayers = 1,
            .samples = VK_SAMPLE_COUNT_1_BIT,
            .tiling = VK_IMAGE_TILING_OPTIMAL,
            .usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
            .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
            .queueFamilyIndexCount = 0, // Ignored if sharingMode is not VK_SHARING_MODE_CONCURRENT.
            .pQueueFamilyIndices = NULL, // Ignored if sharingMode is not VK_SHARING_MODE_CONCURRENT.
            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        },
        .view = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .flags = 0,
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .format = VK_FORMAT_R8G8B8A8_UNORM,
            .components = {
                .r = VK_COMPONENT_SWIZZLE_IDENTITY,
                .g = VK_COMPONENT_SWIZZLE_IDENTITY,
                .b = VK_COMPONENT_SWIZZLE_IDENTITY,
                .a = VK_COMPONENT_SWIZZLE_IDENTITY,
            },
            .subresourceRange = {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1,
            },
        },
        .mem_property_flags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
    });

    transition_image_layout(gfx, gfx->image.display, VK_IMAGE_LAYOUT_UNDEFINED,
                            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
}

static void create_samplers(Graphics *gfx) {
    gfx->sampler.nearest = create_sampler(gfx->device, {
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .magFilter = VK_FILTER_NEAREST,
        .minFilter = VK_FILTER_NEAREST,
        .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
        .addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .mipLodBias = 0.0f,
        .anisotropyEnable = VK_FALSE,
        .maxAnisotropy = 16,
        .compareEnable = VK_FALSE,
        .compareOp = VK_COMPARE_OP_NEVER,
        .minLod = 0.0f,
        .maxLod = 0.0f,
        .borderColor = VK_BORDER_COLOR_INT_OPAQUE_WHITE,
        .unnormalizedCoordinates = VK_FALSE,
    });
}

static DescriptorSet *create_descriptor_set(Graphics *gfx, u32 handle_count, DescriptorInfo *descriptor_infos,
                                            u32 descriptor_count)
{
    CTK_ASSERT(handle_count > 0);
    CTK_ASSERT(handle_count <= DescriptorSet::MAX_HANDLES);

    auto ds = allocate<DescriptorSet>(gfx->mem.perm, 1);
    ds->layout = create_descriptor_set_layout(gfx->mem.temp, gfx->device, descriptor_infos, descriptor_count);
    ds->handles.count = handle_count;
    allocate_descriptor_sets(gfx->mem.temp, gfx->device, gfx->descriptor_pool, ds->layout,
                             ds->handles.count, ds->handles.data);

    return ds;
}

static void create_descriptor_sets(Graphics *gfx) {
    gfx->descriptor_pool = create_descriptor_pool(gfx->device, {
        .descriptor_count = {
            .uniform_buffer = 8,
            // .uniform_buffer_dynamic = 4,
            .combined_image_sampler = 8,
            // .input_attachment = 4,
        },
        .max_descriptor_sets = 64,
    });

    // Texture
    {
        DescriptorInfo descriptor_info = {
            .count = 1,
            .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
        };

        gfx->descriptor_set.texture = create_descriptor_set(gfx, 1, &descriptor_info, 1);

        DescriptorBinding texture_binding = {
            .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .image = {
                .sampler = gfx->sampler.nearest,
                .imageView = gfx->image.display->view,
                .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            },
        };

        update_descriptor_set(gfx->mem.temp, gfx->device, gfx->descriptor_set.texture->handles[0], &texture_binding, 1);
    }
}

static void create_descriptor_set_data(Graphics *gfx) {
    // // View Matrix
    // auto view_matrix = create_array_full<GraphicsMemory *>(gfx->mem.perm, gfx->descriptor_set.view->handles->count);

    // for (u32 i = 0; i < view_matrix->count; ++i) {
    //     view_matrix->data[i] = allocate(gfx, gfx->gfx_mem.device, 64u,
    //                                     gfx->physical_device->min_uniform_buffer_offset_alignment);
    // }

    // gfx->descriptor_set_data.view_matrix = view_matrix;

    // // View Bindings
    // for (u32 i = 0; i < gfx->descriptor_set.view->handles->count; ++i) {
    //     DescriptorBinding view_matrix_binding = {
    //         .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
    //         .buffer = create_buffer_info(gfx->descriptor_set_data.view_matrix->data[i]),
    //     };

    //     update_descriptor_set(gfx->mem.temp, gfx->device, gfx->descriptor_set.view->handles->data[i],
    //                           &view_matrix_binding, 1);
    // }
}

static VkDescriptorBufferInfo create_buffer_info(GraphicsMemory *mem) {
    return { mem->buffer->handle, mem->offset, mem->size };
}

static Pipeline *create_pipeline(Graphics *gfx, PipelineInfo *info) {
    // Shader Stages
    FixedArray<VkPipelineShaderStageCreateInfo, MAX_PIPELINE_SHADER_STAGES> shader_stages = {};

    for (u32 i = 0; i < info->shaders.count; ++i) {
        Shader *shader = info->shaders.data[i];
        VkPipelineShaderStageCreateInfo *shader_stage_info = push(&shader_stages);
        shader_stage_info->sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        shader_stage_info->flags = 0;
        shader_stage_info->stage = shader->stage;
        shader_stage_info->module = shader->handle;
        shader_stage_info->pName = "main";
        shader_stage_info->pSpecializationInfo = NULL;
    }

    VkPipelineLayoutCreateInfo layout_create_info = {};
    layout_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layout_create_info.setLayoutCount = info->descriptor_set_layouts.count;
    layout_create_info.pSetLayouts = info->descriptor_set_layouts.data;
    layout_create_info.pushConstantRangeCount = info->push_constant_ranges.count;
    layout_create_info.pPushConstantRanges = info->push_constant_ranges.data;
    VkPipelineLayout pipeline_layout = VK_NULL_HANDLE;
    validate(vkCreatePipelineLayout(gfx->device, &layout_create_info, NULL, &pipeline_layout),
             "failed to create graphics pipeline layout");

    VkPipelineVertexInputStateCreateInfo vertex_input = {};
    vertex_input.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertex_input.vertexBindingDescriptionCount = info->vertex_bindings.count;
    vertex_input.pVertexBindingDescriptions = info->vertex_bindings.data;
    vertex_input.vertexAttributeDescriptionCount = info->vertex_attributes.count;
    vertex_input.pVertexAttributeDescriptions = info->vertex_attributes.data;

    // // Viewport State
    // VkPipelineViewportStateCreateInfo viewport_state = {};
    // viewport_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    // bool dynamic_viewport = false;
    // bool dynamic_scissor = false;
    // for (u32 i = 0; i < info->dynamic_states.count; ++i) {
    //     if (info->dynamic_states[i] == VK_DYNAMIC_STATE_VIEWPORT)
    //         dynamic_viewport = true;

    //     if (info->dynamic_states[i] == VK_DYNAMIC_STATE_SCISSOR)
    //         dynamic_scissor = true;
    // }

    // if (dynamic_viewport) {
    //     viewport_state.viewportCount = 1;
    //     viewport_state.pViewports    = NULL;
    // }
    // else {
    //     viewport_state.viewportCount = info->viewports.count;
    //     viewport_state.pViewports    = info->viewports.data;
    // }

    // if (dynamic_scissor) {
    //     viewport_state.scissorCount = 1;
    //     viewport_state.pScissors    = NULL;
    // }
    // else {
    //     viewport_state.scissorCount = info->scissors.count;
    //     viewport_state.pScissors    = info->scissors.data;
    // }

    VkPipelineViewportStateCreateInfo viewport = {};
    viewport.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport.viewportCount = info->viewports.count;
    viewport.pViewports = info->viewports.data;
    viewport.scissorCount = info->scissors.count;
    viewport.pScissors = info->scissors.data;

    VkPipelineDynamicStateCreateInfo dynamic_state = {};
    dynamic_state.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamic_state.dynamicStateCount = info->dynamic_states.count;
    dynamic_state.pDynamicStates = info->dynamic_states.data;

    // Reference attachment array in color_blend struct.
    info->color_blend.attachmentCount = info->color_blend_attachments.count;
    info->color_blend.pAttachments = info->color_blend_attachments.data;

    // Create and initialize pipeline.
    auto pipeline = allocate<Pipeline>(gfx->mem.perm, 1);
    pipeline->layout = pipeline_layout;

    VkGraphicsPipelineCreateInfo create_info = {};
    create_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    create_info.stageCount = shader_stages.count;
    create_info.pStages = shader_stages.data;
    create_info.pVertexInputState = &vertex_input;
    create_info.pInputAssemblyState = &info->input_assembly;
    create_info.pTessellationState = NULL;
    create_info.pViewportState = &viewport;
    create_info.pRasterizationState = &info->rasterization;
    create_info.pMultisampleState = &info->multisample;
    create_info.pDepthStencilState = &info->depth_stencil;
    create_info.pColorBlendState = &info->color_blend;
    create_info.pDynamicState = &dynamic_state;
    create_info.layout = pipeline_layout;
    create_info.renderPass = gfx->render_pass->handle;
    create_info.subpass = 0;
    create_info.basePipelineHandle = VK_NULL_HANDLE;
    create_info.basePipelineIndex = -1;

    validate(vkCreateGraphicsPipelines(gfx->device, VK_NULL_HANDLE, 1, &create_info, NULL, &pipeline->handle),
             "failed to create graphics pipeline");

    return pipeline;
}

static void create_pipelines(Graphics *gfx) {
    // Test
    {
        PipelineInfo info = DEFAULT_PIPELINE_INFO;
        push(&info.shaders, gfx->shader.test.vert);
        push(&info.shaders, gfx->shader.test.frag);
        push(&info.color_blend_attachments, DEFAULT_COLOR_BLEND_ATTACHMENT);
        push(&info.push_constant_ranges, {
            .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
            .offset = 0,
            .size = 64
        });
        push(&info.vertex_bindings, {
            .binding = 0,
            .stride = 20,
            .inputRate = VK_VERTEX_INPUT_RATE_VERTEX
        });
        push(&info.vertex_attributes, {
            .location = 0,
            .binding = 0,
            .format = VK_FORMAT_R32G32B32_SFLOAT,
            .offset = 0,
        });
        VkExtent2D surface_extent = get_surface_extent(gfx->physical_device, gfx->surface);

        push(&info.viewports, {
            .x = 0,
            .y = 0,
            .width = (f32)surface_extent.width,
            .height = (f32)surface_extent.height,
            .minDepth = 0,
            .maxDepth = 1
        });
        push(&info.scissors, {
            .offset = { 0, 0 },
            .extent = surface_extent
        });

        // Enable depth testing.
        info.depth_stencil.depthTestEnable = VK_TRUE;
        info.depth_stencil.depthWriteEnable = VK_TRUE;
        info.depth_stencil.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;

        gfx->pipeline.test = create_pipeline(gfx, &info);
    }

    // Texture
    {
        PipelineInfo info = DEFAULT_PIPELINE_INFO;
        push(&info.shaders, gfx->shader.texture.vert);
        push(&info.shaders, gfx->shader.texture.frag);
        push(&info.color_blend_attachments, DEFAULT_COLOR_BLEND_ATTACHMENT);
        push(&info.descriptor_set_layouts, gfx->descriptor_set.texture->layout);
        push(&info.push_constant_ranges, {
            .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
            .offset = 0,
            .size = 64
        });
        push(&info.vertex_bindings, {
            .binding = 0,
            .stride = 20,
            .inputRate = VK_VERTEX_INPUT_RATE_VERTEX
        });
        push(&info.vertex_attributes, {
            .location = 0,
            .binding = 0,
            .format = VK_FORMAT_R32G32B32_SFLOAT,
            .offset = 0,
        });
        push(&info.vertex_attributes, {
            .location = 1,
            .binding = 0,
            .format = VK_FORMAT_R32G32_SFLOAT,
            .offset = 12,
        });
        VkExtent2D surface_extent = get_surface_extent(gfx->physical_device, gfx->surface);

        push(&info.viewports, {
            .x = 0,
            .y = 0,
            .width = (f32)surface_extent.width,
            .height = (f32)surface_extent.height,
            .minDepth = 0,
            .maxDepth = 1
        });
        push(&info.scissors, {
            .offset = { 0, 0 },
            .extent = surface_extent
        });

        // Enable depth testing.
        info.depth_stencil.depthTestEnable = VK_TRUE;
        info.depth_stencil.depthWriteEnable = VK_TRUE;
        info.depth_stencil.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;

        gfx->pipeline.texture = create_pipeline(gfx, &info);
    }
}

static void create_render_state(Graphics *gfx) {
    create_mesh_data(gfx);
    create_shaders(gfx);
    create_images(gfx);
    create_samplers(gfx);
    create_descriptor_sets(gfx);
    create_pipelines(gfx);
}

////////////////////////////////////////////////////////////
/// Interface
////////////////////////////////////////////////////////////
static Graphics *create_graphics(Memory *mem, Platform *platform) {
    auto gfx = allocate<Graphics>(mem, 1);
    gfx->mem.perm = mem;
    gfx->mem.temp = create_stack(gfx->mem.perm, megabyte(6));

    create_vulkan_state(gfx, platform);
    create_render_state(gfx);

    return gfx;
}

static Mesh *create_mesh(Graphics *gfx, MeshInfo info) {
    auto mesh = allocate<Mesh>(gfx->mem.perm, 1);
    mesh->vertexes = create_array<Vertex>(gfx->mem.perm, info.max_vertex_count);
    mesh->indexes = create_array<u32>(gfx->mem.perm, info.max_index_count);
    return mesh;
}

static void push_mesh_data(Graphics *gfx, Mesh *mesh) {
    mesh->vertex_offset = gfx->mesh_data.vertexes->count;
    mesh->index_offset = gfx->mesh_data.indexes->count;

    clear(gfx->gfx_mem.staging);
    begin_temp_cmd_buf(gfx->temp_cmd_buf);
        push(gfx, gfx->mesh_data.vertexes, mesh->vertexes->data, mesh->vertexes->count);
        push(gfx, gfx->mesh_data.indexes, mesh->indexes->data, mesh->indexes->count);
    submit_temp_cmd_buf(gfx->temp_cmd_buf, gfx->queue.graphics);
}

static void next_frame(Graphics *gfx) {
    // Update current frame and wait until it is no longer in-flight.
    if (++gfx->sync.frame_idx >= gfx->sync.frames->size)
        gfx->sync.frame_idx = 0;

    gfx->sync.frame = gfx->sync.frames->data + gfx->sync.frame_idx;
    validate(vkWaitForFences(gfx->device, 1, &gfx->sync.frame->in_flight, VK_TRUE, U64_MAX), "vkWaitForFences failed");
    validate(vkResetFences(gfx->device, 1, &gfx->sync.frame->in_flight), "vkResetFences failed");

    // Once current frame is not in-flight, it is safe to use it's img_aquired semaphore and aquire next swap image.
    validate(
        vkAcquireNextImageKHR(gfx->device, gfx->swapchain->handle, U64_MAX, gfx->sync.frame->img_aquired,
                              VK_NULL_HANDLE, &gfx->sync.swap_img_idx),
        "failed to aquire next swapchain image");
}

static void begin_render_cmds(Graphics *gfx, VkCommandBuffer cmd_buf) {
    VkCommandBufferBeginInfo cmd_buf_begin_info = {};
    cmd_buf_begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    cmd_buf_begin_info.flags = 0;
    cmd_buf_begin_info.pInheritanceInfo = NULL;
    validate(vkBeginCommandBuffer(cmd_buf, &cmd_buf_begin_info), "failed to begin recording command buffer");

    VkRenderPassBeginInfo rp_begin_info = {};
    rp_begin_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    rp_begin_info.renderPass = gfx->render_pass->handle;
    rp_begin_info.framebuffer = gfx->framebuffers->data[gfx->sync.swap_img_idx];
    rp_begin_info.clearValueCount = gfx->render_pass->attachment_clear_values->count;
    rp_begin_info.pClearValues = gfx->render_pass->attachment_clear_values->data;
    rp_begin_info.renderArea = {
        .offset = { 0, 0 },
        .extent = gfx->swapchain->extent,
    };
    vkCmdBeginRenderPass(cmd_buf, &rp_begin_info, VK_SUBPASS_CONTENTS_INLINE);
}

static void bind_mesh_data(Graphics *gfx, VkCommandBuffer cmd_buf) {
    vkCmdBindVertexBuffers(cmd_buf,
                           0, // First Binding
                           1, // Binding Count
                           &gfx->mesh_data.vertexes->mem->buffer->handle,
                           &gfx->mesh_data.vertexes->mem->offset);

    vkCmdBindIndexBuffer(cmd_buf,
                         gfx->mesh_data.indexes->mem->buffer->handle,
                         gfx->mesh_data.indexes->mem->offset,
                         VK_INDEX_TYPE_UINT32);
}

static void draw_mesh(Graphics *gfx, VkCommandBuffer cmd_buf, Mesh *mesh) {
    vkCmdDrawIndexed(cmd_buf, mesh->indexes->count, 1, mesh->index_offset, mesh->vertex_offset, 0);
}

static void end_render_cmds(Graphics *gfx, VkCommandBuffer cmd_buf) {
    vkCmdEndRenderPass(cmd_buf);
    vkEndCommandBuffer(cmd_buf);
}

static void submit_render_cmds(Graphics *gfx) {
    // Rendering
    VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

    VkSubmitInfo submit_info = {};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.pNext = NULL;
    submit_info.waitSemaphoreCount = 1;
    submit_info.pWaitSemaphores = &gfx->sync.frame->img_aquired;
    submit_info.pWaitDstStageMask = &wait_stage;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &gfx->primary_render_cmd_bufs->data[gfx->sync.swap_img_idx];
    submit_info.signalSemaphoreCount = 1;
    submit_info.pSignalSemaphores = &gfx->sync.frame->render_finished;

    validate(vkQueueSubmit(gfx->queue.graphics, 1, &submit_info, gfx->sync.frame->in_flight), "vkQueueSubmit failed");

    // Presentation
    VkPresentInfoKHR present_info = {};
    present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    present_info.pNext = NULL;
    present_info.waitSemaphoreCount = 1;
    present_info.pWaitSemaphores = &gfx->sync.frame->render_finished;
    present_info.swapchainCount = 1;
    present_info.pSwapchains = &gfx->swapchain->handle;
    present_info.pImageIndices = &gfx->sync.swap_img_idx;
    present_info.pResults = NULL;

    validate(vkQueuePresentKHR(gfx->queue.present, &present_info), "vkQueuePresentKHR failed");
}
