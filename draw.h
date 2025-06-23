// draw.h
#ifndef DRAW_H
#define DRAW_H

#include "raylib.h"

#include "world.h"

void draw_world(World *world, Color *pixelBuffer, const Color *colors, unsigned char full_redraw);

#endif