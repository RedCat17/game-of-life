#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <fcntl.h>
#include <time.h>
#include "raylib.h"
#include "raymath.h"


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

int state = 0; // 0 - menu, 1 - sim

typedef struct {
    unsigned int width, height;
    unsigned int stride;
    unsigned int generation;
    unsigned char* world_1; // призрачные ячейки включены
    unsigned char* world_2;
    unsigned char *current_world;
    unsigned char *next_world;
    unsigned int min_living_x, min_living_y, max_living_x, max_living_y;
} World;

typedef struct {
    World world;                // Состояние игрового мира
    
    // Параметры симуляции
    unsigned int delay_us;      // Задержка между шагами (мкс)
    double current_speed;       // Текущая скорость (итераций/сек)
    unsigned char running;
    
    // Статистика
    time_t start_time;          // Время начала симуляции
    long total_iterations;      // Общее количество итераций
    time_t last_measure_time;   // Время последнего замера скорости
    int iterations_since_measure; // Итерации с последнего замера
} Simulation;

int max(int a, int b) {
    return (a > b) ? a : b;
}

int min(int a, int b) {
    return (a < b) ? a : b;
}

int mod(int x, int m) {
    return (x % m + m) % m;
}


void init_world(World *world) {
    world->generation = 0;
    world->stride = world->width + 2;
    world->world_1 = (unsigned char*) malloc(world->stride * (world->height + 2) * sizeof(unsigned char));
    world->world_2 = (unsigned char*) malloc(world->stride * (world->height + 2) * sizeof(unsigned char));
    world->current_world = world->world_1;
    world->next_world = world->world_2;
    world->min_living_x = 0;
    world->min_living_y = 0;
    world->max_living_x = world->width;
    world->max_living_y = world->height;
    
    for (int i = 0; i < (world->height + 2); i++) {
        for (int j = 0; j < (world->width + 2); j++) {
            world->current_world[i * world->stride + j] = 0;
        }
    }
}

void init_sim(Simulation *sim) {
    init_world(&sim->world);
    sim->running = 1;
}

void rand_world(World *world) {
    for (int i = 1; i <= world->height; i++) {
        for (int j = 1; j <= world->width; j++) {
            world->current_world[i * world->stride + j] = rand() % TYPES;
            // current_world[i * width + j] = (i+j) % 2;
        }
    }
    world->min_living_x = 0;
    world->min_living_y = 0;
    world->max_living_x = world->width;
    world->max_living_y = world->height;
}



int count_neighbors(int x, int y, char type, World *world) {
    int count = 0;
    unsigned char* current = world->current_world;
    for (int i = y-1; i<=y+1; i++) {
        for (int j = x-1; j<=x+1; j++) {
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
    int w = world->width;
    int h = world->height;
    for (int x = 1; x <= w; x++) {
        world->current_world[x] = world->current_world[(h) * world->stride + x]; // setting top ghost row to real bottom row
        world->current_world[(h + 1) * world->stride + x] = world->current_world[1 * world->stride + x]; // bottom ghost row
    }
    for (int y = 0; y <= h + 1; y++) {
        world->current_world[y * world->stride] = world->current_world[y * world->stride + w]; // left ghost column
        world->current_world[y * world->stride + w + 1] = world->current_world[y * world->stride + 1]; // right ghost column
    }
}

void step_world(World *world) {
    wrap_edges(world);
    unsigned char *current = world->current_world;
    unsigned char *next = world->next_world;
    int min_y = world->min_living_y - 1;
    int max_y = world->max_living_y + 1;
    int min_x = world->min_living_x - 1;
    int max_x = world->max_living_x + 1;
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
    world->max_living_x = 0;
    world->max_living_y = 0;
    // printf("final %d %d %d %d\n", min_x, max_x, min_y, max_y);
    unsigned int stride = world->stride;
    for (int i = min_y; i <= max_y; i++) {
        for (int j = min_x; j <= max_x; j++) {
            int count = count_neighbors(j, i, 1, world);
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

    world->generation++;
}

void free_world(World *world) {
    if (world->world_1) free(world->world_1);
    if (world->world_2) free(world->world_2);
    world->world_1 = NULL;
    world->world_2 = NULL;
    world->current_world = NULL;
    world->next_world = NULL;
}

void set_cell(World *world, int x, int y, unsigned char value) {
    world->current_world[y * world->stride + x] = value;
    if (value) {
        world->min_living_x = min(world->min_living_x, x);
        world->max_living_x = max(world->max_living_x, x);
        world->min_living_y = min(world->min_living_y, y);
        world->max_living_y = max(world->max_living_y, y);
    }
}

void step_simulation(Simulation* sim) {
    time_t current_time = time(NULL);
    
    // Обновление скорости
    if (difftime(current_time, sim->last_measure_time) >= 1.0) {
        sim->current_speed = sim->iterations_since_measure / 
                           difftime(current_time, sim->last_measure_time);
        sim->last_measure_time = current_time;
        sim->iterations_since_measure = 0;
    }
    
    // Основной шаг
    step_world(&sim->world);
    
    // Обновление статистики
    sim->total_iterations++;
    sim->iterations_since_measure++;
}

void draw_world(World *world) {

    DrawRectangle(CELL_SIZE, CELL_SIZE, world->width * CELL_SIZE, world->height * CELL_SIZE, COLORS[0]);

    unsigned char* current = world->current_world;
    unsigned int stride = world->stride;
    for (int i = 0; i < world->height; i++) {
        for (int j = 0; j < world->width; j++) {
            unsigned char cell = current[i * stride + j];
            if (cell) {
                DrawPixel(j, i, COLORS[cell]);
            }

        }
    }
}

Simulation sim;

int main() {
    char text_buffer[40]; 

    bool dragging = false;
    Vector2 lastMousePosition = {0};


    int size = 32;

    sim.world.width = 1024; sim.world.height = 1024;
    init_world(&sim.world);
    // rand_world(&sim.world);
    int x = sim.world.width / 2 - 2;
    int y = sim.world.height / 2 - 2;

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
    RenderTexture2D texture = LoadRenderTexture(sim.world.width, sim.world.height);
    // SetTargetFPS(60);

    unsigned char changed = 1;  
    unsigned char rendering = 1;  
    unsigned char grid = 1;  
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
            rendering = rendering ? 0 : 1;
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
            init_world(&sim.world);
            changed = 1;
        }
        if (IsKeyPressed(KEY_R)) {
            rand_world(&sim.world);
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
        if (changed && rendering) {
            // printf("rendering world...\n");`
            BeginTextureMode(texture);
                draw_world(&sim.world);        
            EndTextureMode();
        }
        BeginMode2D(camera); 
            ClearBackground(DARKGRAY);
            
            DrawTexturePro(
            texture.texture,
            (Rectangle){ 0, 0, texture.texture.width, -texture.texture.height }, // flip vertically
            (Rectangle){ 0, 0, sim.world.width * CELL_SIZE, sim.world.height * CELL_SIZE },
            (Vector2){ 0, 0 },
            0.0f,
            WHITE
        ); 
        if (grid && camera.zoom > 3) {
            for (int y = 0; y < sim.world.height; y++) {
                DrawLine(0, y*CELL_SIZE, sim.world.width + 2, y*CELL_SIZE, GRAY);
            }
            for (int x = 0; x < sim.world.width; x++) {
                DrawLine(x*CELL_SIZE, 0, x*CELL_SIZE, sim.world.height + 2, GRAY);
            }
        }

        EndMode2D(); 
        sprintf(text_buffer, "FPS: %d\nZoom: %.2f\nIterations: %ld\n%c %c", GetFPS(), camera.zoom, sim.total_iterations, sim.running ? ' ' : 'P', rendering ? 'R' : ' ');
        DrawText(text_buffer, 10, 10, 20, BLACK);
        EndDrawing();

        changed = 0;
    }

    CloseWindow();
    free_world(&sim.world);
    return 0;
}