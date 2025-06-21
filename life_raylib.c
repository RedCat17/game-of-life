#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <unistd.h>
#include <termios.h>
#include <fcntl.h>
#include <time.h>
#include "raylib.h"

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
    int width, height;
    int generation;
    unsigned char* world_1; // призрачные ячейки включены
    unsigned char* world_2;
    unsigned char *current_world;
    unsigned char *next_world;
} World;

typedef struct {
    World world;                // Состояние игрового мира
    
    // Параметры симуляции
    unsigned int delay_us;      // Задержка между шагами (мкс)
    double current_speed;       // Текущая скорость (итераций/сек)
    
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

void init_world(World *world) {
    world->generation = 0;
    world->world_1 = (unsigned char*) malloc(world->width * world->height * sizeof(unsigned char));
    world->world_2 = (unsigned char*) malloc(world->width * world->height * sizeof(unsigned char));
    world->current_world = world->world_1;
    world->next_world = world->world_2;
    for (int i = 0; i < world->height; i++) {
        for (int j = 0; j < world->width; j++) {
            world->current_world[i * world->width + j] = 0;
            // current_world[i * width + j] = (i+j) % 2;
        }
    }
}

void rand_world(World *world) {
    for (int i = 1; i < world->width - 1; i++) {
        for (int j = 1; j < world->height - 1; j++) {
            world->current_world[i * world->width + j] = rand() % TYPES;
            // current_world[i * width + j] = (i+j) % 2;
        }
    }
}

void draw_world(World *world, Camera2D camera) {

    ClearBackground(DARKGRAY);
    DrawRectangle(0, 0, world->width * CELL_SIZE, world->width * CELL_SIZE, RAYWHITE);
    
    float camera_left   = camera.target.x - (camera.offset.x / camera.zoom);
    float camera_right  = camera.target.x + (camera.offset.x / camera.zoom);
    float camera_top    = camera.target.y - (camera.offset.y / camera.zoom);
    float camera_bottom = camera.target.y + (camera.offset.y / camera.zoom);

    int render_left = max(1, camera_left);
    int render_right = min(world->height - 1, camera_right + 1);
    int render_top = max(1, camera_top);
    int render_bottom = min(world->width - 1, camera_bottom + 1);
    
    for (int i = render_top; i < render_bottom; i++) {
        for (int j = render_left; j < render_right; j++) {
            unsigned char cell = world->current_world[i * world->width + j];
            // printf(" %c", CHARS[cell]);
            if (cell != 0) {
                DrawRectangle(j*CELL_SIZE, i*CELL_SIZE, CELL_SIZE, CELL_SIZE, COLORS[cell]);
            }

        }
    }
}

int get_neighbors(int x, int y, char type, World *world) {
    int count = 0;
    for (int i = y-1; i<=y+1; i++) {
        for (int j = x-1; j<=x+1; j++) {
            if (i == y && j == x) {
                continue;
            }
            if (world->current_world[i * world->width + j] == type) {
                count++;
            }
        }
    }
    return count;
}

void step_world(World *world) {
    for (int x = 1; x < world->width; x++) {
        world->current_world[x] = world->current_world[(world->height - 2) * world->width + x];
        world->current_world[(world->height - 1) * world->width + x] = world->current_world[world->width + x];
    }
    for (int y = 0; y < world->height; y++) {
        world->current_world[y * world->width] = world->current_world[y * world->width + world->width - 2];
        world->current_world[y * world->width + world->width - 1] = world->current_world[y * world->width + 1];
    }
    int count;
    for (int i = 1; i < world->width - 1; i++) {
        for (int j = 1; j < world->height - 1; j++) {
            count = get_neighbors(j, i, 1, world);
            if ((world->current_world[i * world->width + j] == 1 && (count == 2 || count == 3)) || (world->current_world[i * world->width + j] == 0 && (count == 3))) {
                world->next_world[i * world->width + j] = 1;
            } else {
                world->next_world[i * world->width + j] = 0;
            }
        }
    }
    unsigned char *temp = world->current_world;
    world->current_world = world->next_world;
    world->next_world = temp;

    world->generation++;
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

Simulation sim;

int main() {
    char text_buffer[40]; 
    Camera2D camera = { 0 };
    camera.target = (Vector2){ 0.0f, 0.0f };     // What point in world space the camera looks at
    camera.offset = (Vector2){ WIDTH / 2, HEIGHT / 2 }; // Center of the screen
    camera.rotation = 0.0f;                      // No rotation
    camera.zoom = 1.0f;                          // Normal zoom

    int size = 1024;

    size += 2;
    sim.world.width = size; sim.world.height = size;
    init_world(&sim.world);
    rand_world(&sim.world);
    state = 1;
    InitWindow(WIDTH, HEIGHT, "raylib window");
    SetTargetFPS(60);

    while (!WindowShouldClose()) {
        step_simulation(&sim);

        if (IsKeyDown(KEY_RIGHT)) camera.target.x += 10.0f / camera.zoom;
        if (IsKeyDown(KEY_LEFT))  camera.target.x -= 10.0f / camera.zoom;
        if (IsKeyDown(KEY_DOWN))  camera.target.y += 10.0f / camera.zoom;
        if (IsKeyDown(KEY_UP))    camera.target.y -= 10.0f / camera.zoom;

        if (IsKeyDown(KEY_Q)) camera.zoom *= 1.05f;
        if (IsKeyDown(KEY_E)) camera.zoom /= 1.05f;
        if (camera.zoom < 0.1f) camera.zoom = 0.1f; // Prevent inverting

        BeginDrawing();
        BeginMode2D(camera); 
            draw_world(&sim.world, camera);        
        EndMode2D(); 
        sprintf(text_buffer, "FPS: %d", GetFPS());
        DrawText(text_buffer, 10, 10, 20, BLACK);
        EndDrawing();
    }

    CloseWindow();
    return 0;
}