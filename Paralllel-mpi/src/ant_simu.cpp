#include <vector>
#include <iostream>
#include <chrono>
#include <mpi.h>
#include "fractal_land.hpp"
#include "ants.hpp"      
#include "pheronome.hpp"
#include "renderer.hpp"
#include "window.hpp"
#include "rand_generator.hpp"

// Acumuladores de tempo para o relatório técnico
double t_comm_total = 0.0;
double t_comp_total = 0.0;

void advance_time(const fractal_land& land, pheronome& phen, 
                  const position_t& pos_nest, const position_t& pos_food,
                  ants& all_ants, std::size_t& local_cpteur, double eps, int rank, int size)
{
    // --- PARTE 1: COMPUTAÇÃO LOCAL ---
    double t_start_comp = MPI_Wtime();
    
    all_ants.advance_all(phen, land, pos_food, pos_nest, local_cpteur, eps);
    
    t_comp_total += (MPI_Wtime() - t_start_comp);

    // --- PARTE 2: COMUNICAÇÃO (O GARGALO) ---
    double t_start_comm = MPI_Wtime();

    // Sincroniza o mapa de feromônios entre todos os processos
    MPI_Allreduce(MPI_IN_PLACE, 
                  phen.get_buffer_ptr(), 
                  phen.get_buffer_size(), 
                  MPI_DOUBLE, 
                  MPI_MAX, 
                  MPI_COMM_WORLD);

    t_comm_total += (MPI_Wtime() - t_start_comm);

    // --- PARTE 3: ATUALIZAÇÃO PÓS-MPI ---
    phen.do_evaporation(); 
    phen.update();
}

int main(int nargs, char* argv[])
{
    int rank, size;
    MPI_Init(&nargs, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    if (rank == 0) SDL_Init( SDL_INIT_VIDEO );
    
    // Configurações
    std::size_t seed = 2026;
    const int nb_ants_total = 5000;
    const int nb_ants_local = nb_ants_total / size; 
    const double eps = 0.8;
    const std::size_t MAX_ITERATIONS = 4000; 

    fractal_land land(8, 2, 1., 1024);
    
    // Normalização (feita por todos para economizar comunicação de mapa)
    double max_val = -1e30, min_val = 1e30;
    for (fractal_land::dim_t i = 0; i < land.dimensions(); ++i)
        for (fractal_land::dim_t j = 0; j < land.dimensions(); ++j) {
            max_val = std::max(max_val, land(i,j));
            min_val = std::min(min_val, land(i,j));
        }
    double delta = max_val - min_val;
    for (fractal_land::dim_t i = 0; i < land.dimensions(); ++i)
        for (fractal_land::dim_t j = 0; j < land.dimensions(); ++j)
            land(i,j) = (land(i,j)-min_val)/delta;

    // Cada processo cuida de uma semente diferente baseada no seu rank
    ants all_ants(nb_ants_local, land, seed + rank * 777);
    pheronome phen(land.dimensions(), {500, 500}, {256, 256}, 0.7, 0.999);

    Window* win = nullptr;
    Renderer* renderer = nullptr;
    if (rank == 0) {
        win = new Window("Ant Simulation (Hybrid MPI/OMP)", 2*land.dimensions()+10, land.dimensions()+266);
        renderer = new Renderer( land, phen, {256,256}, {500,500}, all_ants );
    }

    size_t local_food = 0, global_food = 0;
    bool cont_loop = true;
    std::size_t it = 0;

    if (rank == 0) std::cout << "Executando com " << size << " processos MPI..." << std::endl;

    // Sincronização inicial para o timer
    MPI_Barrier(MPI_COMM_WORLD);
    auto start_global = std::chrono::high_resolution_clock::now();

    while (cont_loop && it < MAX_ITERATIONS) {
        it++;
        
        if (rank == 0) {
            SDL_Event event;
            while (SDL_PollEvent(&event)) if (event.type == SDL_QUIT) cont_loop = false;
        }
        MPI_Bcast(&cont_loop, 1, MPI_CXX_BOOL, 0, MPI_COMM_WORLD);

        advance_time( land, phen, {256,256}, {500,500}, all_ants, local_food, eps, rank, size );

        // Reduz a comida total para o Rank 0 exibir
        MPI_Reduce(&local_food, &global_food, 1, MPI_UNSIGNED_LONG, MPI_SUM, 0, MPI_COMM_WORLD);

        if (rank == 0) {
            renderer->display( *win, global_food );
            win->blit();
        }
    }

    auto end_global = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> total_diff = end_global - start_global;

    if (rank == 0) {
        std::cout << "\n--- ANÁLISE DE PERFORMANCE MPI ---" << std::endl;
        std::cout << "Tempo Total Lógica: " << total_diff.count() << " s" << std::endl;
        std::cout << "Tempo Computação (Formigas): " << t_comp_total << " s" << std::endl;
        std::cout << "Tempo Comunicação (Rede): " << t_comm_total << " s" << std::endl;
        std::cout << "Eficiência: " << (t_comp_total / total_diff.count()) * 100 << "%" << std::endl;
        
        delete renderer; delete win;
        SDL_Quit();
    }

    MPI_Finalize();
    return 0;
}