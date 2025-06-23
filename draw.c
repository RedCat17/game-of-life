// draw.c
#include "raylib.h"

#include "world.h"

void draw_world(World *world, Color *pixelBuffer, const Color *colors, unsigned char full_redraw) {
    unsigned char* current = world->current_world;
    unsigned int stride = world->stride;
    // for (int i = 0; i < world->height; i++) {
    //     for (int j = 0; j < world->width; j++) {
    //         unsigned char cell = current[(i + 1) * stride + (j + 1)];
    //         pixelBuffer[i * world->width + j] = COLORS[cell];

    //     }
    // }
    int min_y, max_y, min_x, max_x;
    if (full_redraw) {
        min_y = 0;
        max_y = world->width;
        min_x = 0;
        max_x = world->height;
    } else {
        min_y = world->min_living_y - 1;
        max_y = world->max_living_y + 1;
        min_x = world->min_living_x - 1;
        max_x = world->max_living_x + 1;
        if (min_x < 0 || max_x > world->width) {
            max_x = world->width;
            min_x = 0;
        }
        if (min_y < 0 || max_y > world->height) {
            max_y = world->height;
            min_y = 0;
        }     
    }
    for (int i = min_y; i < max_y; i++) {
        for (int j = min_x; j < max_x; j++) {
            unsigned char cell = current[(i + 1) * stride + (j + 1)];
            pixelBuffer[i * world->width + j] = colors[cell];

        }
    }
}