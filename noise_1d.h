#pragma once

#include "ctk/ctk.h"
#include "ctk/math.h"
#include "ctk/memory.h"
#include "stk/stk.h"
#include "noise_test/game.h"
#include "noise_test/noise_utils.h"
#include "noise_test/permutation.h"

using namespace ctk;
using namespace stk;

struct Graph {
    u32 width;
    u32 height;
    u32 x_origin;
    u32 y_origin;
    Array<f32> *sample;
};

static u32 constexpr BASE_GRAPH_COUNT = 3;
static u32 constexpr COMPOSITE_GRAPH_INDEX = BASE_GRAPH_COUNT;

struct NoiseTest {
    Array<f32> *noise;
    Array<Graph> *graphs;
    InterpFunc interp_func;
};

static u32 centered(u32 container_dimension, u32 graph_dimension) {
    return (container_dimension - graph_dimension) / 2;
}

static f32 noise_val(Array<f32> *noise, u32 i) {
    CTK_ASSERT(i < PERMUTATION_SIZE);
    return get(noise, PERMUTATION[i]);
}

static f32 sample(Array<f32> *noise, f32 noise_offset, InterpFunc interp_func) {
    u32 noise_offset_index = (u32)noise_offset;
    f32 val_offset = interp_func(noise_offset - noise_offset_index);

    u32 curr = noise_offset_index & PERMUTATION_SIZE_MASK;
    u32 next = (curr + 1) & PERMUTATION_SIZE_MASK;

    f32 curr_val = noise_val(noise, curr);
    f32 next_val = noise_val(noise, next);

    return lerp(curr_val, next_val, val_offset);
}

static void generate_graph_samples(NoiseTest *noise_test) {
    // Base Graph Samples
    f32 frequency = 128.0f;
    f32 amplitude = 1.0f;

    for (u32 graph_idx = 0; graph_idx < BASE_GRAPH_COUNT; ++graph_idx) {
        Graph *graph = get_ptr(noise_test->graphs, graph_idx);
        f32 sample_amplitude = graph->height * amplitude;

        for (u32 graph_pixel_x = 0; graph_pixel_x < graph->width; ++graph_pixel_x) {
            f32 sample_offset = graph_pixel_x / frequency;
            f32 val = sample(noise_test->noise, sample_offset, noise_test->interp_func) * sample_amplitude;
            set(graph->sample, graph_pixel_x, val);
        }

        frequency /= 4;
        amplitude /= 4;
    }

    // Composite Graph Sample
    Graph *graph = get_ptr(noise_test->graphs, COMPOSITE_GRAPH_INDEX);

    for (u32 graph_pixel_x = 0; graph_pixel_x < graph->width; ++graph_pixel_x) {
        f32 val = 0.0f;

        for (u32 base_graph_index = 0; base_graph_index < BASE_GRAPH_COUNT; ++base_graph_index)
            val += get(get_ptr(noise_test->graphs, base_graph_index)->sample, graph_pixel_x);

        val /= BASE_GRAPH_COUNT;
        set(graph->sample, graph_pixel_x, val);
    }
}

static NoiseTest *create_noise_test(Game *game) {
    auto noise_test = allocate<NoiseTest>(game->mem.perm, 1);
    noise_test->noise = create_array_full<f32>(game->mem.perm, PERMUTATION_SIZE);
    noise_test->interp_func = smootherstep;

    // Graphs
    static u32 constexpr TOTAL_GRAPH_COUNT = BASE_GRAPH_COUNT + 1;

    noise_test->graphs = create_array<Graph>(game->mem.perm, TOTAL_GRAPH_COUNT);
    u32 graph_width = game->display.width;
    u32 graph_height = game->display.height / TOTAL_GRAPH_COUNT;
    u32 x_origin = centered(game->display.width, graph_width);

    for (u32 i = 0; i < TOTAL_GRAPH_COUNT; ++i) {
        push(noise_test->graphs, {
            .width = graph_width,
            .height = graph_height,
            .x_origin = x_origin,
            .y_origin = game->display.height - (graph_height * (i + 1)),
            .sample = create_array_full<f32>(game->mem.perm, graph_width),
        });
    }

    // Adjust game view to show full display.
    game->view->transform.position.z = -4.5f;

    generate_noise(noise_test->noise, time(NULL));
    generate_graph_samples(noise_test);

    return noise_test;
}

static void draw_graph(Game *game, InterpFunc interp_func, Graph *graph) {
    s32 prev_val = get(graph->sample, 0);

    for (u32 graph_pixel_x = 0; graph_pixel_x < graph->width; ++graph_pixel_x) {
        s32 val = (s32)get(graph->sample, graph_pixel_x);
        s32 dir = val > prev_val ? 1 :
                  val < prev_val ? -1 :
                  0;
        s32 y = prev_val + dir;

        do {
            u32 pixel_x = graph->x_origin + graph_pixel_x;
            u32 pixel_y = graph->y_origin + y;
            draw_point(game, pixel_x, pixel_y, { .color = 0xFF0000FF, .scale = 1 });
            y += dir;
        } while (y != val + dir);

        prev_val = val;
    }
}

static void noise_test_display(Game *game, NoiseTest *noise_test) {
    for (u32 i = 0; i < noise_test->graphs->count; ++i)
        draw_graph(game, noise_test->interp_func, get_ptr(noise_test->graphs, i));
}

static void noise_test_controls(Window *window, NoiseTest *noise_test) {
    if (interp_func_controls(window, &noise_test->interp_func))
        generate_graph_samples(noise_test);

    if (key_pressed(window, Key::G)) {
        generate_noise(noise_test->noise, time(NULL));
        generate_graph_samples(noise_test);
    }
}
