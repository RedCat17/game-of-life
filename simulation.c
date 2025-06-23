// simulation.c
#include "simulation.h"
#include "world.h"

void init_sim(Simulation *sim) {
    init_world(&sim->world);
    sim->running = 0;
    sim->total_iterations = 0;
}

void step_simulation(Simulation* sim) {
    // Основной шаг
    step_world(&sim->world);
    
    // Обновление статистики
    sim->total_iterations++;
}