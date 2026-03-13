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

// Acumuladores de tempo globais para o relatório
double t_comm_total = 0.0;
double t_comp_total = 0.0;

void advance_time(const fractal_land& land, pheronome& phen, 
                  const position_t& pos_nest, const position_t& pos_food,
                  ants& all_ants, std::size_t& local_cpteur, double eps, int rank, int size)
{
    // --- PARTE 1: COMPUTAÇÃO E MIGRAÇÃO DE FORMIGAS ---
    double t_start_comp = MPI_Wtime();
    
    // Agora passa rank e size para gerenciar a migração entre fatias
    all_ants.advance_all(phen, land, pos_food, pos_nest, local_cpteur, eps, rank, size);
    
    t_comp_total += (MPI_Wtime() - t_start_comp);

    // --- PARTE 2: COMUNICAÇÃO DE BORDAS (Halo Exchange) ---
    double t_start_comm = MPI_Wtime();

    // Troca apenas as linhas limítrofes (muito mais rápido que Allreduce)
    phen.exchange_borders(rank, size);

    t_comm_total += (MPI_Wtime() - t_start_comm);

    // --- PARTE 3: ATUALIZAÇÃO ---
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
    
    // Configurações Globais
    std::size_t master_seed = 2026;
    const int nb_ants_total = 5000;
    const double eps = 0.8;
    const std::size_t MAX_ITERATIONS = 4000; 
    
    position_t nest_pos{256, 256};
    position_t food_pos{500, 500};

    fractal_land land(8, 2, 1., 1024);
    
    // Normalização do terreno (feita por todos localmente)
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

    // Inicialização do Pheronome com Rank/Size (Slicing)
    pheronome phen(land.dimensions(), food_pos, nest_pos, 0.7, 0.999, rank, size);

    // Determinar se o ninho está na fatia deste processo
    int rows_per_proc = (int)land.dimensions() / size;
    int y_min = rank * rows_per_proc;
    int y_max = (rank == size - 1) ? (int)land.dimensions() - 1 : y_min + rows_per_proc - 1;

    // Apenas o processo que contém a coordenada Y do ninho começa com as formigas
    std::size_t initial_ants = 0;
    if (nest_pos.y >= y_min && nest_pos.y <= y_max) {
        initial_ants = (std::size_t)nb_ants_total;
    }
    
    // Instanciação das formigas (nb_ants agora é std::size_t)
    ants all_ants(initial_ants, land, (unsigned int)(master_seed + rank * 777));

    Window* win = nullptr;
    Renderer* renderer = nullptr;
    if (rank == 0) {
        win = new Window("Ant Simulation - Domain Decomposition", 2*land.dimensions()+10, land.dimensions()+266);
        renderer = new Renderer( land, phen, nest_pos, food_pos, all_ants );
    }

    std::size_t local_food = 0;
    std::size_t global_food = 0;
    bool cont_loop = true;
    std::size_t it = 0;

    if (rank == 0) std::cout << "Executando Decomposição de Domínio com " << size << " processos MPI..." << std::endl;

    MPI_Barrier(MPI_COMM_WORLD);
    auto start_global = std::chrono::high_resolution_clock::now();

    while (cont_loop && it < MAX_ITERATIONS) {
        it++;
        
        if (rank == 0) {
            SDL_Event event;
            while (SDL_PollEvent(&event)) {
                if (event.type == SDL_QUIT) cont_loop = false;
            }
        }
        // Sincroniza sinal de saída
        MPI_Bcast(&cont_loop, 1, MPI_CXX_BOOL, 0, MPI_COMM_WORLD);

        advance_time( land, phen, nest_pos, food_pos, all_ants, local_food, eps, rank, size );

        // Sincroniza contador de comida
        MPI_Reduce(&local_food, &global_food, 1, MPI_UNSIGNED_LONG, MPI_SUM, 0, MPI_COMM_WORLD);

        if (rank == 0) {
            // Nota: O Renderer no Rank 0 só verá as formigas que estão na fatia 0.
            // Para ver todas, seria necessário um Gatherv (custoso para benchmark).
            renderer->display( *win, global_food );
            win->blit();
        }
    }

    auto end_global = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> total_diff = end_global - start_global;

    if (rank == 0) {
        std::cout << "\n--- RESULTADOS DECOMPOSIÇÃO DE DOMÍNIO ---" << std::endl;
        std::cout << "Tempo Total de Execução : " << total_diff.count() << " s" << std::endl;
        std::cout << "Tempo de Computação Puro: " << t_comp_total << " s" << std::endl;
        std::cout << "Tempo de Comunicação    : " << t_comm_total << " s" << std::endl;
        std::cout << "Eficiência Relativa     : " << (t_comp_total / total_diff.count()) * 100.0 << "%" << std::endl;
        
        delete renderer; 
        delete win;
        SDL_Quit();
    }

    MPI_Finalize();
    return 0;
}