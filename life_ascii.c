#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <unistd.h>
#include <termios.h>
#include <fcntl.h>
#include <time.h>

#define RESET   "\x1b[0m"
#define GREEN   "\x1b[32m"

#define MAX_WORLD_SIZE 8192
#define TYPES 2
const char CHARS[] = {
    [0] = '.',  // Empty
    [1] = '#',  // Living
};

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

void print_stats(const Simulation* sim) {
    double uptime = difftime(time(NULL), sim->start_time);
    printf("Скорость: %.1f итер/сек | Всего итераций: %ld | Время работы: %.0f сек\n",
           sim->current_speed, sim->total_iterations, uptime);
}

void set_input_mode(int enable) {
    static struct termios oldt, newt;
    if (enable) {
        tcgetattr(STDIN_FILENO, &oldt);
        newt = oldt;
        newt.c_lflag &= ~(ICANON | ECHO); // disable buffering and echo
        tcsetattr(STDIN_FILENO, TCSANOW, &newt);
    } else {
        tcsetattr(STDIN_FILENO, TCSANOW, &oldt); // restore old settings
    }
}

int kbhit(void) {
    struct timeval tv = {0L, 0L};
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(STDIN_FILENO, &fds);
    return select(STDIN_FILENO+1, &fds, NULL, NULL, &tv);
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

void print_world(World *world) {
    for (int i = 1; i < world->width - 1; i++) {
        for (int j = 1; j < world->height - 1; j++) {
            unsigned char cell = world->current_world[i * world->width + j];
            // printf(" %c", CHARS[cell]);
            printf(" %s%c%s", (cell == 1 ? GREEN : ""), CHARS[cell], RESET);

        }
        printf("\n");
    }
    printf("ПОКОЛЕНИЕ %d\n", world->generation);
}

void debug_dump(World *world) {
    for (int i = 0; i < world->width; i++) {
        // printf("%3d: ", i);
        for (int j = 0; j < world->height; j++) {
            // printf(" %3d:%d", j, current_world[i * width + j]);
            printf(" %3d:%d:%d", i, j, world->current_world[i * world->width + j]);
        }
        printf("\n");
    }
    for (int i = 0; i < world->width; i++) {
        for (int j = 0; j < world->height; j++) {
            printf(" %c", CHARS[world->current_world[i * world->width + j]]);
        }
        printf("\n");
    }
    printf("\n");
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
    for (int i = 1; i < world->height - 1; i++) {
        for (int j = 1; j < world->width - 1; j++) {
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

// void spawn_glider(int x, int y) {
//     current_world[(y + 1) * width + x + 2] = 1;
//     current_world[(y + 2) * width + x + 3] = 1;
//     current_world[(y + 3) * width + x + 1] = 1;
//     current_world[(y + 3) * width + x + 2] = 1;
//     current_world[(y + 3) * width + x + 3] = 1;
// }

Simulation sim;

int main_loop() {
    srand(time(NULL)); 
    char input;

    while (1) {
        printf("\x1b[2J\x1b[H");
        switch (state) {
            case 0: {
                printf("Введите команду (q: выход, n: новый мир, c: продолжить, l: загрузка из файла): ");
                scanf(" %c", &input);
                
                switch (input) {
                    case 'n': {
                        int size;
                        printf("Введите размер мира: ");
                        scanf("%i", &size);
                        if (size > MAX_WORLD_SIZE) {
                            printf("Максимальный размер: %d\n", MAX_WORLD_SIZE);
                            break;
                        }

                        size += 2;
                        sim.world.width = size; sim.world.height = size;
                        init_world(&sim.world);
                        rand_world(&sim.world);
                        state = 1;
                    } break;
                    case 'c': {
                        if (sim.world.current_world) {
                            state = 1;
                        } else {
                            printf("Нечего продолжать!");
                        }
                    } break;
                    case 'l': {
                        printf("Не реализовано\n");
                    } break;
                    case 'q': {
                        return 1;
                    } break;
                }
            } break;
            case 1:
                print_world(&sim.world);
                printf("Введите команду (q: выход в меню, s: шаг, r: реальное время, f: 100 итераций): ");
                scanf(" %c", &input);
                switch (input) {
                    case 's': {
                        step_world(&sim.world);
                    } break;
                    case 'f': { // 'f' for "fast-forward"
                        for (int i = 0; i < 100; i++) {
                            step_world(&sim.world);
                        }
                    } break;
                    case 'r': {  // 'r' for "real-time"
                        set_input_mode(1);
                        while (1) {
                            if (kbhit()) {
                                char c = getchar();
                                if (c == 'q') break;
                            }
                            step_world(&sim.world);
                            system("clear");
                            print_world(&sim.world);
                            printf("Реальное время: нажмите q для выхода...\n");
                            struct timespec delay = {0, 100*1000*1000}; // 100ms
                            nanosleep(&delay, NULL); 
                        }
                        set_input_mode(0);
                    } break;

                    case 'd': {
                        debug_dump(&sim.world);
                    } break;
                    case 'q': {
                        state = 0;
                    } break;
                }
            break;
        }
        

    } 
}

int main() {

    printf("Добро пожаловать в игру Жизнь v0.3!\n");
    main_loop();
}