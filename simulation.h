// simulation.h
#ifndef SIMULATION_H
#define SIMULATION_H

#include "world.h"

typedef struct {
    World world;                // Состояние игрового мира
    
    // Параметры симуляции
    unsigned int delay_us;      // Задержка между шагами (мкс)
    double current_speed;       // Текущая скорость (итераций/сек)
    unsigned char running;
    
    // Статистика
    long total_iterations;      // Общее количество итераций
} Simulation;

void init_sim(Simulation *sim);
void step_simulation(Simulation* sim);

#endif