// world.c
#include "world.h"
#include <stdlib.h>
#include <string.h>

#include "util.h"

void init_world(World *world) {
    world->stride = world->width + 2;
    world->world_1 = (unsigned char*) malloc(world->stride * (world->height + 2) * sizeof(unsigned char));
    world->world_2 = (unsigned char*) malloc(world->stride * (world->height + 2) * sizeof(unsigned char));
    world->current_world = world->world_1;
    world->next_world = world->world_2;
    world->min_living_x = 1;
    world->min_living_y = 1;
    world->max_living_x = world->width;
    world->max_living_y = world->height;
    
    memset(world->current_world, 0, (world->height + 2) * world->stride * sizeof(*world->current_world));
    memset(world->next_world, 0, (world->height + 2) * world->stride * sizeof(*world->current_world));
}

void rand_world(World *world, unsigned char types) {
    for (uint i = 1; i <= world->height; i++) {
        for (uint j = 1; j <= world->width; j++) {
            world->current_world[i * world->stride + j] = rand() % types;
            // current_world[i * width + j] = (i+j) % 2;
        }
    }
    world->min_living_x = 1;
    world->min_living_y = 1;
    world->max_living_x = world->width;
    world->max_living_y = world->height;
}

void free_world(World *world) {
    if (world->world_1) free(world->world_1);
    if (world->world_2) free(world->world_2);
    world->world_1 = NULL;
    world->world_2 = NULL;
    world->current_world = NULL;
    world->next_world = NULL;
}

void set_cell(World *world, uint x, uint y, unsigned char value) {
    if (x >= 0 && x < world->width && y >= 0 && y < world->height)
        world->current_world[(y + 1) * world->stride + (x + 1)] = value;

    if (value) {
        world->min_living_x = min(world->min_living_x, x);
        world->max_living_x = max(world->max_living_x, x);
        world->min_living_y = min(world->min_living_y, y);
        world->max_living_y = max(world->max_living_y, y);
    }
}

uint count_neighbors(uint x, uint y, char type, World *world) {
    uint count = 0;
    unsigned char* current = world->current_world;
    for (uint i = y-1; i<=y+1; i++) {
        for (uint j = x-1; j<=x+1; j++) {
            if (i == y && j == x) {
                continue;
            }
            if (current[i * world->stride + j] == type) {
                count++;
            }
        }
    }
    return count;
}

void wrap_edges(World* world) {
    uint w = world->width;
    uint h = world->height;
    for (uint x = 1; x <= w; x++) {
        world->current_world[x] = world->current_world[(h) * world->stride + x]; // setting top ghost row to real bottom row
        world->current_world[(h + 1) * world->stride + x] = world->current_world[1 * world->stride + x]; // bottom ghost row
    }
    for (uint y = 0; y <= h + 1; y++) {
        world->current_world[y * world->stride] = world->current_world[y * world->stride + w]; // left ghost column
        world->current_world[y * world->stride + w + 1] = world->current_world[y * world->stride + 1]; // right ghost column
    }
}

void step_world(World *world) {
    wrap_edges(world);
    unsigned char *current = world->current_world;
    unsigned char *next = world->next_world;
    uint min_y = world->min_living_y - 1;
    uint max_y = world->max_living_y + 1;
    uint min_x = world->min_living_x - 1;
    uint max_x = world->max_living_x + 1;
    if (min_x < 1 || max_x > world->width) {
        max_x = world->width;
        min_x = 1;
    }
    if (min_y < 1 || max_y > world->height) {
        max_y = world->height;
        min_y = 1;
    }

    world->min_living_x = world->width;
    world->min_living_y = world->height;
    world->max_living_x = 1;
    world->max_living_y = 1;
    // printf("final %d %d %d %d\n", min_x, max_x, min_y, max_y);
    uint stride = world->stride;
    for (uint i = min_y; i <= max_y; i++) {
        for (uint j = min_x; j <= max_x; j++) {
            uint count = count_neighbors(j, i, 1, world);
            unsigned char cell = current[i * stride + j];
            if (cell) {
                if (i < world->min_living_y) world->min_living_y = i;
                if (i > world->max_living_y) world->max_living_y = i;
                if (j < world->min_living_x) world->min_living_x = j;
                if (j > world->max_living_x) world->max_living_x = j;
            }
            next[i * stride + j] = ((cell == 1 && (count == 2 || count == 3)) || (cell == 0 && (count == 3)));

        }
    }
    unsigned char *temp = world->current_world;
    world->current_world = world->next_world;
    world->next_world = temp;
}