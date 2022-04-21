#pragma once

#include "ctk/ctk.h"
#include "ctk/math.h"
#include "ctk/memory.h"
#include "noise_test/vtk.h"
#include "stk/stk.h"

using namespace ctk;
using namespace vtk;
using namespace stk;

struct EntityInfo {
    Transform transform;
    Mesh *mesh;
    Pipeline *pipeline;
};

struct Pencil {
    u32 color;
    s32 scale;
};

struct Game {
    static constexpr u32 MAX_ENTITIES = 1024;

    struct {
        Memory *perm;
        Memory *temp;
    } mem;

    struct {
        Vec2<s32> mouse_position;
        Vec2<s32> mouse_delta;
        Vec2<s32> last_mouse_position;
    } input;

    struct {
        Mesh *tri;
        Mesh *quad;
        Mesh *hex;
    } mesh;

    View *view;

    struct {
        Transform transform[MAX_ENTITIES];
        Matrix model[MAX_ENTITIES];
        Matrix mvp[MAX_ENTITIES];
        Mesh *mesh[MAX_ENTITIES];
        Pipeline *pipeline[MAX_ENTITIES];
        u32 count;
    } entity_data;

    struct {
        u32 tri;
        u32 quad;
    } entity;

    struct {
        u32 *data;
        u32 size;
        u32 width;
        u32 height;
    } display;
};

static void create_meshes(Game *game, Graphics *gfx) {
    {
        auto quad = create_mesh(gfx, { .max_vertex_count = 16, .max_index_count = 64 });

        push(quad->vertexes, { { -0.5f, 0.5f, 0 }, { 0, 0 } });
        push(quad->vertexes, { { -0.5f,-0.5f, 0 }, { 0, 1 } });
        push(quad->vertexes, { {  0.5f,-0.5f, 0 }, { 1, 1 } });
        push(quad->vertexes, { {  0.5f, 0.5f, 0 }, { 1, 0 } });

        push(quad->indexes, 0u);
        push(quad->indexes, 1u);
        push(quad->indexes, 2u);

        push(quad->indexes, 0u);
        push(quad->indexes, 2u);
        push(quad->indexes, 3u);

        game->mesh.quad = quad;
    }
    {
        auto tri = create_mesh(gfx, { .max_vertex_count = 16, .max_index_count = 64 });

        push(tri->vertexes, { { -0.5f, 0.5f, 0 }, { 0.0f, 0.0f } });
        push(tri->vertexes, { { -0.0f,-0.5f, 0 }, { 0.5f, 1.0f } });
        push(tri->vertexes, { {  0.5f, 0.5f, 0 }, { 1.0f, 1.0f } });

        push(tri->indexes, 0u);
        push(tri->indexes, 1u);
        push(tri->indexes, 2u);

        game->mesh.tri = tri;
    }
    {
        auto hex = create_mesh(gfx, { .max_vertex_count = 16, .max_index_count = 64 });

        push(hex->vertexes, { { -0.25f, -0.5f, 0 }, { 0, 0 } });
        push(hex->vertexes, { {  0.25f, -0.5f, 0 }, { 0, 0 } });
        push(hex->vertexes, { {  0.5f,   0.0f, 0 }, { 0, 0 } });
        push(hex->vertexes, { {  0.25f,  0.5f, 0 }, { 0, 0 } });
        push(hex->vertexes, { { -0.25f,  0.5f, 0 }, { 0, 0 } });
        push(hex->vertexes, { { -0.5f,   0.0f, 0 }, { 0, 0 } });

        push(hex->indexes, 0u);
        push(hex->indexes, 1u);
        push(hex->indexes, 2u);

        push(hex->indexes, 0u);
        push(hex->indexes, 2u);
        push(hex->indexes, 3u);

        push(hex->indexes, 0u);
        push(hex->indexes, 3u);
        push(hex->indexes, 4u);

        push(hex->indexes, 0u);
        push(hex->indexes, 4u);
        push(hex->indexes, 5u);

        game->mesh.hex = hex;
    }

    push_mesh_data(gfx, game->mesh.tri);
    push_mesh_data(gfx, game->mesh.quad);
    push_mesh_data(gfx, game->mesh.hex);
}

static void create_view(Game *game, Graphics *gfx) {
    game->view = allocate<View>(game->mem.perm, 1);
    game->view->transform.position = { 0.0f, 0.0f, -1.0f };
    game->view->transform.scale = { 1.0f, 1.0f, 1.0f };
    game->view->perspective_info = {
        .vertical_fov = 90,
        .aspect = (f32)gfx->swapchain->extent.width / gfx->swapchain->extent.height,
        .z_near = 0.1f,
        .z_far = 1000,
    };
    game->view->max_x_angle = 89.0f;
}

static u32 push_entity(Game *game, EntityInfo info) {
    if (game->entity_data.count + 1 > Game::MAX_ENTITIES)
        CTK_FATAL("already at max entity count of %u", Game::MAX_ENTITIES);

    u32 entity = game->entity_data.count;
    game->entity_data.count++;

    game->entity_data.transform[entity] = info.transform;
    game->entity_data.mesh[entity] = info.mesh;
    game->entity_data.pipeline[entity] = info.pipeline;

    return entity;
}

static void create_entities(Game *game, Graphics *gfx) {
    game->entity.tri = push_entity(game, {
        .transform {
            .position = { .z = 0.0f },
            .rotation = {},
            .scale = { 16.0f, 9.0f, 1.0f },
        },
        .mesh = game->mesh.tri,
        .pipeline = gfx->pipeline.test,
    });

    game->entity.quad = push_entity(game, {
        .transform {
            .position = {},
            .rotation = {},
            .scale = { 16.0f, 9.0f, 1.0f },
        },
        .mesh = game->mesh.quad,
        .pipeline = gfx->pipeline.texture,
    });
}

static void clear_display(Game *game, u32 color) {
    memset(game->display.data, color, game->display.size * sizeof(u32));
}

static void create_display(Game *game, Graphics *gfx) {
    VkExtent2D swap_img_extent = gfx->swapchain->extent;
    game->display.width = swap_img_extent.width;
    game->display.height = swap_img_extent.height;
    game->display.size = game->display.width * game->display.height;
    game->display.data = allocate<u32>(game->mem.perm, game->display.size);
}

static Game *create_game(Memory *mem, Graphics *gfx) {
    auto game = allocate<Game>(mem, 1);
    game->mem.perm = mem;
    game->mem.temp = create_stack(game->mem.perm, megabyte(32));

    // Initialization.
    create_meshes(game, gfx);
    create_view(game, gfx);
    create_entities(game, gfx);
    create_display(game, gfx);

    return game;
}

static void update_mouse(Game *game, Window *window, Graphics *gfx) {
    game->input.mouse_position = get_mouse_position(window);
    game->input.mouse_delta = game->input.mouse_position - game->input.last_mouse_position;
    game->input.last_mouse_position = game->input.mouse_position;
}

static void local_translate(Transform *transform, Vec3<f32> translation) {
    Matrix matrix = ID_MATRIX;
    matrix = rotate(matrix, transform->rotation.x, Axis::X);
    matrix = rotate(matrix, transform->rotation.y, Axis::Y);
    matrix = rotate(matrix, transform->rotation.z, Axis::Z);

    Vec3<f32> forward = { matrix[0][2], matrix[1][2], matrix[2][2] };
    Vec3<f32> right = { matrix[0][0], matrix[1][0], matrix[2][0] };
    transform->position += translation.z * forward;
    transform->position += translation.x * right;
    transform->position.y += translation.y;
}

static void draw_point(Game *game, u32 x, u32 y, Pencil p) {
    for (s32 ny = -(p.scale - 1); ny < p.scale; ++ny)
    for (s32 nx = -(p.scale - 1); nx < p.scale; ++nx) {
        s32 pixel_x = x + nx;
        s32 pixel_y = y + ny;

        if (pixel_x >= 0 && pixel_x < game->display.width &&
            pixel_y >= 0 && pixel_y < game->display.height)
        {
            game->display.data[(pixel_y * game->display.width) + pixel_x] = p.color;
        }
    }
}

static void controls(Game *game, Graphics *gfx, Window *window) {
    if (key_down(window, Key::ESCAPE)) {
        platform->window->open = false;
        return;
    }

    // View Controls
    View *view = game->view;

    // Translation
    static constexpr f32 BASE_TRANSLATION_SPEED = 0.01f;
    f32 mod = key_down(window, Key::SHIFT) ? 10 : 1;
    f32 translation_speed = BASE_TRANSLATION_SPEED * mod;
    Vec3<f32> translation = {};

    if (key_down(window, Key::W)) translation.z += translation_speed;
    if (key_down(window, Key::S)) translation.z -= translation_speed;
    if (key_down(window, Key::D)) translation.x += translation_speed;
    if (key_down(window, Key::A)) translation.x -= translation_speed;
    if (key_down(window, Key::E)) translation.y -= translation_speed;
    if (key_down(window, Key::Q)) translation.y += translation_speed;

    local_translate(&view->transform, translation);

    // Rotation
    if (mouse_button_down(window, 1)) {
        static constexpr f32 ROTATION_SPEED = 0.2f;
        view->transform.rotation.x += game->input.mouse_delta.y * ROTATION_SPEED;
        view->transform.rotation.y -= game->input.mouse_delta.x * ROTATION_SPEED;
        view->transform.rotation.x = clamp(view->transform.rotation.x, -view->max_x_angle, view->max_x_angle);
    }
}

static void update_display(Game *game, Graphics *gfx) {
    clear(gfx->gfx_mem.staging);
    push(gfx, gfx->gfx_mem.staging, game->display.data, game->display.size * sizeof(u32));

    begin_temp_cmd_buf(gfx->temp_cmd_buf);
        copy_to_image(gfx->temp_cmd_buf, gfx->gfx_mem.staging->mem->buffer, 0, gfx->image.display);
    submit_temp_cmd_buf(gfx->temp_cmd_buf, gfx->queue.graphics);
}

static Matrix calculate_view_space_matrix(View *view) {
    // View Matrix
    Matrix model_matrix = ID_MATRIX;
    model_matrix = rotate(model_matrix, view->transform.rotation.x, Axis::X);
    model_matrix = rotate(model_matrix, view->transform.rotation.y, Axis::Y);
    model_matrix = rotate(model_matrix, view->transform.rotation.z, Axis::Z);
    Vec3<f32> forward = { model_matrix[0][2], model_matrix[1][2], model_matrix[2][2] };
    Matrix view_matrix = look_at(view->transform.position, view->transform.position + forward, { 0.0f, -1.0f, 0.0f });

    // Projection Matrix
    Matrix projection_matrix = perspective_matrix(view->perspective_info);
    projection_matrix[1][1] *= -1; // Flip y value for scale (glm is designed for OpenGL).

    return projection_matrix * view_matrix;
}

static void update_entity_data(Game *game) {
    Matrix view_space_matrix = calculate_view_space_matrix(game->view);

    // Calculate entity model matrixes.
    for (u32 i = 0; i < game->entity_data.count; ++i) {
        Transform *transform = game->entity_data.transform + i;

        Matrix model_matrix = ID_MATRIX;
        model_matrix = translate(ID_MATRIX, transform->position);
        model_matrix = rotate(model_matrix, transform->rotation.x, Axis::X);
        model_matrix = rotate(model_matrix, transform->rotation.y, Axis::Y);
        model_matrix = rotate(model_matrix, transform->rotation.z, Axis::Z);
        model_matrix = scale(model_matrix, transform->scale);

        game->entity_data.mvp[i] = view_space_matrix * model_matrix;
    }
}

static void update_descriptor_data(Game *game, Graphics *gfx) {
    // Matrix mvp = calculate_view_space_matrix(game->view);

    // clear(gfx->gfx_mem.staging);
    // push(gfx, gfx->gfx_mem.staging, &mvp, sizeof(mvp));
    // GraphicsMemory *view_matrix = gfx->descriptor_set_data.view_matrix->data[gfx->sync.swap_img_idx];
    // begin_temp_cmd_buf(gfx->temp_cmd_buf);
    //     copy_to_buffer(gfx->device, gfx->temp_cmd_buf, {
    //         .src_buffer = gfx->gfx_mem.staging->mem->buffer,
    //         .src_offset = 0,
    //         .dst_buffer = view_matrix->buffer,
    //         .dst_offset = view_matrix->offset,
    //         .size = sizeof(mvp),
    //     });
    // submit_temp_cmd_buf(gfx->temp_cmd_buf, gfx->queue.graphics);
}

static void record_render_cmds(Game *game, Graphics *gfx) {
    VkCommandBuffer cmd_buf = gfx->primary_render_cmd_bufs->data[gfx->sync.swap_img_idx];
    begin_render_cmds(gfx, cmd_buf);
    for (u32 i = 0; i < game->entity_data.count; ++i) {
        // Pipeline Binding
        Pipeline *pipeline = game->entity_data.pipeline[i];
        vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->handle);

        if (pipeline == gfx->pipeline.texture) {
            VkDescriptorSet descriptor_sets[] = {
                gfx->descriptor_set.texture->handles[0],
            };
            vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->layout,
                                    0, CTK_ARRAY_SIZE(descriptor_sets), descriptor_sets,
                                    0, NULL);
        }

        vkCmdPushConstants(cmd_buf, gfx->pipeline.test->layout, VK_SHADER_STAGE_VERTEX_BIT, 0, 64,
                           game->entity_data.mvp + i);

        // Mesh Drawing
        bind_mesh_data(gfx, cmd_buf);
        draw_mesh(gfx, cmd_buf, game->entity_data.mesh[i]);
    }

    end_render_cmds(gfx, cmd_buf);
}
