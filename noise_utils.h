#pragma once

#include "ctk/ctk.h"
#include "ctk/memory.h"
#include "ctk/containers.h"
#include "stk/stk.h"
#include "noise_test/game.h"
#include "noise_test/permutation.h"

using namespace ctk;
using namespace stk;

typedef f32 (*InterpFunc)(f32 t);

static f32 lerp(f32 a, f32 b, f32 t) {
    return a + ((b - a) * t);
}

static f32 linear(f32 t) {
    return t;
}

static f32 smoothstep(f32 t) {
    return t * t * (3 - (2 * t));
}

static f32 smootherstep(f32 t) {
    return t * t * t * ((3 * t * ((2 * t) - 5)) + 10);
}

static void generate_noise(Array<f32> *noise, u32 seed) {
    random_seed(seed);
    for (u32 graph_idx = 0; graph_idx < noise->count; ++graph_idx)
        set(noise, graph_idx, random_range(0.0f, 1.0f));
}

static bool interp_func_controls(Window *window, InterpFunc *interp_func) {
    if (key_down(window, Key::F1)) {
        *interp_func = linear;
        return true;
    }
    else if (key_down(window, Key::F2)) {
        *interp_func = smoothstep;
        return true;
    }
    else if (key_down(window, Key::F3)) {
        *interp_func = smootherstep;
        return true;
    }

    return false;
}
