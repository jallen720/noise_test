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

struct DisplayInfo {
    f32 frequency;
    f32 frequency_scaling_factor;
    u32 size;
    u32 x_origin;
    u32 y_origin;
};

struct NoiseTest {
    DisplayInfo *display_info;
    Array<f32> *noise;
    InterpFunc interp_func;
};

static DisplayInfo *create_display_info(Game *game) {
    auto display_info = allocate<DisplayInfo>(game->mem.perm, 1);
    display_info->frequency = 100.0f;
    display_info->frequency_scaling_factor = 1.03f;
    display_info->size = 256;

    // Center display.
    display_info->x_origin = (game->display.width - display_info->size) / 2;
    display_info->y_origin = (game->display.height - display_info->size) / 2;

    // Adjust game view to show full display.
    game->view->transform.position.z = -1.3f;

    return display_info;
}

static NoiseTest *create_noise_test(Game *game) {
    auto noise_test = allocate<NoiseTest>(game->mem.perm, 1);
    noise_test->noise = create_noise(game, 0xDEADBEEF);
    noise_test->display_info = create_display_info(game);
    noise_test->interp_func = smootherstep;
    return noise_test;
}

static f32 noise_val(Array<f32> *noise, u32 x, u32 y) {
    CTK_ASSERT(x < PERMUTATION_SIZE);
    CTK_ASSERT(y < PERMUTATION_SIZE);
    return get(noise, PERMUTATION[PERMUTATION[x] + y]);
}

static f32 sample(Array<f32> *noise, f32 x, f32 y, InterpFunc interp_func) {
    u32 x_floor = (u32)x;
    u32 y_floor = (u32)y;

    // Remap interpolated x-offset to interp_func value.
    f32 tx = x - x_floor;
    f32 ty = y - y_floor;

    f32 step_x = interp_func(tx);
    f32 step_y = interp_func(ty);

    // Interpolate along north and south edges using remapped x-offset.
    u32 west = x_floor & PERMUTATION_SIZE_MASK;
    u32 east = (west + 1) & PERMUTATION_SIZE_MASK;
    u32 south = y_floor & PERMUTATION_SIZE_MASK;
    u32 north = (south + 1) & PERMUTATION_SIZE_MASK;

    f32 sw_val = noise_val(noise, west, south);
    f32 se_val = noise_val(noise, east, south);
    f32 nw_val = noise_val(noise, west, north);
    f32 ne_val = noise_val(noise, east, north);

    f32 south_edge_val = lerp(sw_val, se_val, step_x);
    f32 north_edge_val = lerp(nw_val, ne_val, step_x);

    // Return interpolated value between interpolated north and south edge values.
    return lerp(south_edge_val, north_edge_val, step_y);
}

static inline u32 shade_color(u8 shade) {
    return 0xFF000000   // A (default 255)
         | shade << 0   // R
         | shade << 8   // G
         | shade << 16; // B
}

static void noise_test_display(Game *game, NoiseTest *noise_test) {
    DisplayInfo *display_info = noise_test->display_info;

    for (u32 y = 0; y < display_info->size; ++y)
    for (u32 x = 0; x < display_info->size; ++x) {
        f32 sample_x = x / display_info->frequency;
        f32 sample_y = y / display_info->frequency;
        u32 color = shade_color(255 * sample(noise_test->noise, sample_x, sample_y, noise_test->interp_func));

        u32 pixel_x = display_info->x_origin + x;
        u32 pixel_y = display_info->y_origin + y;
        draw_point(game, pixel_x, pixel_y, { .color = color, .scale = 1 });
    }
}

static void noise_test_controls(Window *window, NoiseTest *noise_test) {
    DisplayInfo *display_info = noise_test->display_info;

    interp_func_controls(window, &noise_test->interp_func);

    // Frequency
    static constexpr f32 FREQ_MAX = 100.0f;
    static constexpr f32 FREQ_MIN = 1.0f;
    f32 freq_scale_inc = display_info->frequency_scaling_factor;
    f32 freq_scale_dec = 1 / display_info->frequency_scaling_factor;

    if (key_down(window, Key::RIGHT))
        display_info->frequency = min(display_info->frequency * freq_scale_inc, FREQ_MAX);

    if (key_down(window, Key::LEFT))
        display_info->frequency = max(display_info->frequency * freq_scale_dec, FREQ_MIN);
}
