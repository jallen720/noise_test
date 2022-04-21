#include "ctk/ctk.h"
#include "ctk/math.h"
#include "stk/stk.h"
#include "noise_test/graphics.h"
#include "noise_test/game.h"
#include "noise_test/noise_utils.h"
#include "noise_test/noise_1d.h"
// #include "noise_test/noise_2d.h"

using namespace ctk;
using namespace stk;

s32 main() {
    // Memory
    Memory *mem = create_stack(megabyte(64));
    Memory *platform_mem = create_stack(mem, kilobyte(2));
    Memory *graphics_mem = create_stack(mem, megabyte(8));

    // Modules
    Platform *platform = create_platform(platform_mem);
    Window *window = create_window(platform_mem, {
        .surface = {
            .x = 0,
            .y = 100,
            .width = 1600,
            .height = 900,
        },
        .title = L"Game",
    });

    Graphics *gfx = create_graphics(graphics_mem, platform);
    Game *game = create_game(mem, gfx);
    NoiseTest *noise_test = create_noise_test(game);

    // Main Loop
    while (1) {
        process_events(window);

        // Quit event closed window.
        if (!window->open)
            break;

        if (!window_is_active(window))
            continue;

        // Input and controls.
        update_mouse(game, window, gfx);
        controls(game, gfx, platform);
        noise_test_controls(window, noise_test);

        // Input closed window.
        if (!window->open)
            break;

        next_frame(gfx);

        // Draw noise to display texture.
        clear_display(game, 0xFF101010);
        noise_test_display(game, noise_test);
        update_display(game, gfx);

        // Render entities.
        update_entity_data(game);
        update_descriptor_data(game, gfx);
        record_render_cmds(game, gfx);
        submit_render_cmds(gfx);
    }

    return 0;
}

