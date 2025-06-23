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
    init_world(world);
    for (int i = 1; i <= world->height; i++) {
        for (int j = 1; j <= world->width; j++) {
            world->current_world[i * world->stride + j] = rand() % TYPES;
            // current_world[i * width + j] = (i+j) % 2;
        }
    }
}

void draw_world(World *world, Camera2D camera) {

    ClearBackground(DARKGRAY);
    DrawRectangle(CELL_SIZE, CELL_SIZE, world->width * CELL_SIZE, world->height * CELL_SIZE, COLORS[0]);
    
    float camera_left   = camera.target.x - (camera.offset.x / camera.zoom);
    float camera_right  = camera.target.x + (camera.offset.x / camera.zoom);
    float camera_top    = camera.target.y - (camera.offset.y / camera.zoom);
    float camera_bottom = camera.target.y + (camera.offset.y / camera.zoom);

    int render_left = max(0, (int)camera_left);
    int render_right = min(world->width + 2, (int)camera_right + 1); // drawing ghost cells too for debug
    int render_top = max(0, (int)camera_top);
    int render_bottom = min(world->height + 2, (int)camera_bottom + 1);

    
    for (int i = render_top; i < render_bottom; i++) {
        DrawLine(0, i*CELL_SIZE, world->width + 2, i*CELL_SIZE, GRAY);
    }
    for (int j = render_left; j < render_right; j++) {
        DrawLine(j*CELL_SIZE, 0, j*CELL_SIZE, world->height + 2, GRAY);
    }

    unsigned char* current = world->current_world;
    for (int i = render_top; i < render_bottom; i++) {
        for (int j = render_left; j < render_right; j++) {
            unsigned char cell = current[i * world->stride + j];
            if (cell != 0) {
                DrawRectangle(j*CELL_SIZE, i*CELL_SIZE, CELL_SIZE, CELL_SIZE, COLORS[cell]);
            }

        }
    }
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
    for (int x = 1; x < w + 1; x++) {
        world->current_world[x] = world->current_world[(h) * world->stride + x]; // setting top ghost row to real bottom row
        world->current_world[(h + 1) * world->stride + x] = world->current_world[1 * world->stride + x]; // bottom ghost row
    }
    for (int y = 0; y < h; y++) {
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
    for (int i = min_y; i <= max_y; i++) {
        for (int j = min_x; j <= max_x; j++) {
            int count = count_neighbors(j, i, 1, world);
            unsigned char cell = current[i * world->stride + j];
            if (cell) {
                if (i < world->min_living_y) world->min_living_y = i;
                if (i > world->max_living_y) world->max_living_y = i;
                if (j < world->min_living_x) world->min_living_x = j;
                if (j > world->max_living_x) world->max_living_x = j;
            }
            next[i * world->stride + j] = ((cell == 1 && (count == 2 || count == 3)) || (cell == 0 && (count == 3)));
            // if ((cell == 1 && (count == 2 || count == 3)) || (cell == 0 && (count == 3))) {
            //     next[i * world->stride + j] = 1;
            // } else {
            //     next[i * world->stride + j] = 0;
            // }

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

    bool dragging = false;
    Vector2 lastMousePosition = {0};


    int size = 32;

    size += 2;
    sim.world.width = 64; sim.world.height = 16;
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
    InitWindow(WIDTH, HEIGHT, "raylib window");
    SetTargetFPS(60);

    while (!WindowShouldClose()) {
        if (sim.running) step_simulation(&sim);
        if (IsKeyPressed(KEY_F)) step_simulation(&sim);

        if (IsKeyDown(KEY_D)) camera.target.x += 10.0f / camera.zoom;
        if (IsKeyDown(KEY_A)) camera.target.x -= 10.0f / camera.zoom;
        if (IsKeyDown(KEY_S)) camera.target.y += 10.0f / camera.zoom;
        if (IsKeyDown(KEY_W)) camera.target.y -= 10.0f / camera.zoom;

        if (GetMouseWheelMove() == 1) camera.zoom *= 1.1f;
        if (GetMouseWheelMove() == -1) camera.zoom /= 1.1f;
        if (camera.zoom < 0.1f) camera.zoom = 0.1f;

        if (IsKeyPressed(KEY_P)) sim.running = sim.running ? 0 : 1;

        if (IsKeyPressed(KEY_N)) init_world(&sim.world);
        if (IsKeyPressed(KEY_R)) rand_world(&sim.world);

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