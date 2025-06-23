// life_raylib.c
#include <stdio.h>
#include <stdlib.h>
#include "raylib.h"
#include "raymath.h"

#include "world.h"
#include "simulation.h"
#include "draw.h"
#include "util.h"


#define WIDTH 800
#define HEIGHT 600

#define CELL_SIZE 1
#define TARGET_SPS 60.0f

#define TYPES 2
const Color COLORS[] = {
    [0] = RAYWHITE,  // Empty
    [1] = DARKGREEN,  // Living
};

#define MAX_WORLD_SIZE 1024

uint state = 0; // 0 - menu, 1 - sim

Simulation sim;

int main() {
    char text_buffer[128]; 

    bool dragging = false;
    Vector2 lastMousePosition = {0};


    uint size = 32;

    sim.world.width = 2048; sim.world.height = 2048;
    init_world(&sim.world);
    // rand_world(&sim.world);
    uint x = sim.world.width / 2 - 2;
    uint y = sim.world.height / 2 - 2;

    sim.world.current_world[(y + 1) * sim.world.stride + x + 2] = 1;
    sim.world.current_world[(y + 2) * sim.world.stride + x + 3] = 1;
    sim.world.current_world[(y + 3) * sim.world.stride + x + 1] = 1;
    sim.world.current_world[(y + 3) * sim.world.stride + x + 2] = 1;
    sim.world.current_world[(y + 3) * sim.world.stride + x + 3] = 1;
    state = 1;
    Camera2D camera = { 0 };
    camera.target = (Vector2){ sim.world.width / 2, sim.world.height / 2 };     // What point in world space the camera looks at
    camera.offset = (Vector2){ WIDTH / 2, HEIGHT / 2 }; // Center of the screen
    camera.rotation = 0.0f;                      // No rotation
    camera.zoom = 1.0f;                          // Normal zoom

    InitWindow(WIDTH, HEIGHT, "Game of Life");
    Color* pixelBuffer = malloc(sim.world.width * sim.world.height * sizeof(Color));
    Image dummy = {
        .data = pixelBuffer,
        .width = sim.world.width,
        .height = sim.world.height,
        .mipmaps = 1,
        .format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8
    };
    Texture2D texture = LoadTextureFromImage(dummy);

    // RenderTexture2D texture = LoadRenderTexture(sim.world.width, sim.world.height);
    // SetTargetFPS(60);

    unsigned char changed = 1;  
    unsigned char rendering = 1;  
    unsigned char grid = 1;  
    unsigned char full_redraw = 1;  
    while (!WindowShouldClose()) {
        float frametime = GetFrameTime();
        if (sim.running) {
            step_simulation(&sim);
            changed = 1;
        }
        if (IsKeyPressed(KEY_F)) {
            step_simulation(&sim);
            changed = 1;
        }

        if (IsKeyDown(KEY_RIGHT)) camera.target.x += 600.0f / camera.zoom * frametime;
        if (IsKeyDown(KEY_LEFT)) camera.target.x -= 600.0f / camera.zoom * frametime;
        if (IsKeyDown(KEY_DOWN)) camera.target.y += 600.0f / camera.zoom * frametime;
        if (IsKeyDown(KEY_UP)) camera.target.y -= 600.0f / camera.zoom * frametime;

        if (GetMouseWheelMove() == 1) camera.zoom *= 1.1f;
        if (GetMouseWheelMove() == -1) camera.zoom /= 1.1f;
        if (camera.zoom < 0.1f) camera.zoom = 0.1f;

        if (IsKeyPressed(KEY_P)) {
            sim.running = sim.running ? 0 : 1;
            #ifdef DEBUG
                if (sim.running) {
                    printf("running\n");
                }
                else {
                    printf("paused\n");
                }
            #endif
        }
        if (IsKeyPressed(KEY_D)) {
            if (rendering) {
                rendering = 0;
            } else {
                rendering = 1;
                full_redraw = 1;  
            }
            #ifdef DEBUG
                if (rendering) {
                    printf("rendering\n");
                }
                else {
                    printf("not rendering\n");
                }
            #endif
        }

        if (IsKeyPressed(KEY_G)) {
            grid = grid ? 0 : 1;
            changed = 1;
            #ifdef DEBUG
                if (grid) {
                    printf("grid on\n");
                }
                else {
                    printf("grid off\n");
                }
            #endif
        }

        if (IsKeyPressed(KEY_N)) {
            free_world(&sim.world);
            init_sim(&sim);
            changed = 1;
        }
        if (IsKeyPressed(KEY_R)) {
            rand_world(&sim.world, TYPES);
            changed = 1;
        }

        // Start drag
        if (IsMouseButtonPressed(MOUSE_BUTTON_MIDDLE)) {
            dragging = true;
            lastMousePosition = GetMousePosition();
        }

        // End drag
        if (IsMouseButtonReleased(MOUSE_BUTTON_MIDDLE)) {
            dragging = false;
        }

        // Handle dragging
        if (dragging) {
            Vector2 currentMouse = GetMousePosition();
            Vector2 delta = Vector2Subtract(lastMousePosition, currentMouse);
            delta = Vector2Scale(delta, 1.0f / camera.zoom);
            camera.target = Vector2Add(camera.target, delta);
            lastMousePosition = currentMouse;
        }

        if(IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
            Vector2 mousePos = GetMousePosition();
            mousePos.x = (mousePos.x - camera.offset.x) / camera.zoom + camera.target.x;
            mousePos.y = (mousePos.y - camera.offset.y) / camera.zoom + camera.target.y;
            set_cell(&sim.world, mousePos.x, mousePos.y, 1);
            changed = 1;
        }

        if(IsMouseButtonDown(MOUSE_BUTTON_RIGHT)) {
            Vector2 mousePos = GetMousePosition();
            mousePos.x = (mousePos.x - camera.offset.x) / camera.zoom + camera.target.x;
            mousePos.y = (mousePos.y - camera.offset.y) / camera.zoom + camera.target.y;
            set_cell(&sim.world, mousePos.x, mousePos.y, 0);
            changed = 1;
        }

        BeginDrawing();
        if (rendering) {
            // printf("rendering world...\n");`
            draw_world(&sim.world, pixelBuffer, COLORS, full_redraw);        
            UpdateTexture(texture, pixelBuffer);
        }
        BeginMode2D(camera); 
            ClearBackground(DARKGRAY);
            
            DrawTexturePro(
            texture,
            (Rectangle){ 0, 0, texture.width, texture.height }, // flip vertically
            (Rectangle){ 0, 0, texture.width * CELL_SIZE, texture.height * CELL_SIZE },
            (Vector2){ 0, 0 },
            0.0f,
            WHITE
        ); 
        if (grid && camera.zoom > 5) {
            for (uint y = 0; y <= sim.world.height; y++) {
                DrawLine(0, y*CELL_SIZE, sim.world.width, y*CELL_SIZE, GRAY);
            }
            for (uint x = 0; x <= sim.world.width; x++) {
                DrawLine(x*CELL_SIZE, 0, x*CELL_SIZE, sim.world.height, GRAY);
            }
        }

        EndMode2D(); 
        sprintf(text_buffer, "FPS: %d\nZoom: %.2f\nIterations: %ld\n%c %c", GetFPS(), camera.zoom, sim.total_iterations, sim.running ? ' ' : 'P', rendering ? 'R' : ' ');
        DrawText(text_buffer, 10, 10, 20, BLACK);
        EndDrawing();

        changed = 0;
        full_redraw = 0;
    }

    CloseWindow();
    free_world(&sim.world);
    free(pixelBuffer);

    return 0;
}