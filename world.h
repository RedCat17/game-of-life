// world.h
#ifndef WORLD_H
#define WORLD_H

typedef struct {
    unsigned int width, height;
    unsigned int stride;
    unsigned char types;
    unsigned char* world_1; // призрачные ячейки включены
    unsigned char* world_2;
    unsigned char *current_world;
    unsigned char *next_world;
    unsigned int min_living_x, min_living_y, max_living_x, max_living_y;
} World;

void init_world(World *world);
void rand_world(World *world, unsigned char types);
void free_world(World *world);
void set_cell(World *world, unsigned int x, unsigned int y, unsigned char value);
void wrap_edges(World* world);
void step_world(World *world);

#endif