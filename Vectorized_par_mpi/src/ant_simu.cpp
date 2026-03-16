#include <vector>
#include <iostream>
#include <random>
#include <chrono>
#include <omp.h>
#include <mpi.h> 

#include "fractal_land.hpp"
#include "ant.hpp"
#include "pheronome.hpp"
#include "renderer.hpp"
#include "window.hpp"
#include "rand_generator.hpp"

// ----- Variables globales pour accumuler les temps -----
using clock_type = std::chrono::high_resolution_clock;

double total_time_move = 0.0;
double total_time_evap = 0.0;
double total_time_update = 0.0;

void advance_time( const fractal_land& land, pheronome& phen,
                   const position_t& pos_nest, const position_t& pos_food,
                   std::vector<ant>& ants, std::size_t& cpteur )
{
    const std::size_t n = ants.size();

    // 1. ----- Déplacement des fourmis (Local au processus) -----
    auto t1 = clock_type::now();
    #pragma omp parallel for schedule(guided)
    for (std::size_t i = 0; i < n; ++i) {
        ants[i].advance(phen, land, pos_food, pos_nest, cpteur);
    }
    auto t2 = clock_type::now();

    // 2. ----- Synchronisation MPI des phéromones -----
    // On fusionne les cartes de tous les processus en prenant la valeur MAX
    phen.sync_mpi(); 
    auto t_sync = clock_type::now();

    // 3. ----- Evaporation des phéromones -----
    phen.do_evaporation();
    auto t3 = clock_type::now();

    // 4. ----- Mise à jour des phéromones -----
    phen.update(); 
    auto t4 = clock_type::now();

    total_time_move += std::chrono::duration<double, std::milli>(t2 - t1).count();
    total_time_evap += std::chrono::duration<double, std::milli>(t3 - t_sync).count();
    total_time_update += std::chrono::duration<double, std::milli>(t4 - t3).count();
}

int main(int nargs, char* argv[])
{
    // ----- INITIALISATION MPI -----
    MPI_Init(&nargs, &argv);

    int rank, num_procs;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &num_procs);

    if (rank == 0) {
        std::cout << "--- Lancement de la simulation avec " << num_procs << " processus MPI ---" << std::endl;
    }

    std::size_t seed = 2026;
    const int nb_ants = 5000;
    const double eps = 0.8;
    const double alpha = 0.7;
    const double beta = 0.999;

    position_t pos_nest{256,256};
    position_t pos_food{500,500};

    // Tous les processus génèrent exactement le même terrain fractal (même seed)
    fractal_land land(8,2,1.,1024);

    double max_val = 0.0;
    double min_val = 0.0;
    for (fractal_land::dim_t i = 0; i < land.dimensions(); ++i)
        for (fractal_land::dim_t j = 0; j < land.dimensions(); ++j) {
            max_val = std::max(max_val, land(i,j));
            min_val = std::min(min_val, land(i,j));
        }

    double delta = max_val - min_val;
    for (fractal_land::dim_t i = 0; i < land.dimensions(); ++i)
        for (fractal_land::dim_t j = 0; j < land.dimensions(); ++j)
            land(i,j) = (land(i,j) - min_val) / delta;

    ant::set_exploration_coef(eps);

    // ----- Division des fourmis -----
    // On génère TOUTES les positions pour garantir la même séquence aléatoire qu'en séquentiel
    std::vector<ant> all_ants;
    all_ants.reserve(nb_ants);
    auto gen_ant_pos = [&land, &seed]() { return rand_int32(0, land.dimensions()-1, seed); };
    
    for (std::size_t i = 0; i < nb_ants; ++i)
        all_ants.emplace_back(position_t{gen_ant_pos(), gen_ant_pos()}, seed);

    // Calcul de la part de ce processus
    int local_ants_count = nb_ants / num_procs;
    int remainder = nb_ants % num_procs;
    int my_start = rank * local_ants_count + std::min(rank, remainder);
    int my_count = local_ants_count + (rank < remainder ? 1 : 0);
    int my_end = my_start + my_count;

    // Ce processus ne garde que SA part des fourmis
    std::vector<ant> my_ants(all_ants.begin() + my_start, all_ants.begin() + my_end);

    // Initialisation des phéromones
    pheronome phen(land.dimensions(), pos_food, pos_nest, alpha, beta);

    // ----- Gestion de l'affichage (Seul le Processus 0 s'en charge) -----
    Window* win = nullptr;
    Renderer* renderer = nullptr;
    if (rank == 0) {
        SDL_Init(SDL_INIT_VIDEO);
        win = new Window("Ant Simulation", 2*land.dimensions()+10, land.dimensions()+266);
        // Note: Le processus 0 n'affichera visuellement que ses propres fourmis, 
        // mais les phéromones seront correctes grâce à la synchronisation.
        renderer = new Renderer(land, phen, pos_nest, pos_food, my_ants);
    }

    std::size_t local_food_quantity = 0;
    std::size_t global_food_quantity = 0;
    
    SDL_Event event;
    bool cont_loop = true;
    bool not_food_in_nest = true;
    std::size_t it = 0;

    auto start_time = clock_type::now();

    // ----- BOUCLE PRINCIPALE -----
    while (cont_loop)
    {
        ++it;

        if (rank == 0) {
            while (SDL_PollEvent(&event)) {
                if (event.type == SDL_QUIT) cont_loop = false;
            }
        }

        // On doit propager l'état de "cont_loop" du Processus 0 vers les autres (si on ferme la fenêtre)
        MPI_Bcast(&cont_loop, 1, MPI_CXX_BOOL, 0, MPI_COMM_WORLD);
        if (!cont_loop) break;

        // Déplacement local + Évaporation
        advance_time(land, phen, pos_nest, pos_food, my_ants, local_food_quantity);

        // Synchronisation du compteur de nourriture (Somme globale)
        MPI_Allreduce(&local_food_quantity, &global_food_quantity, 1, MPI_UNSIGNED_LONG, MPI_SUM, MPI_COMM_WORLD);

        // Affichage (Uniquement Rank 0)
        if (rank == 0) {
            renderer->display(*win, global_food_quantity);
            win->blit();
        }

        if (not_food_in_nest && global_food_quantity > 0)
        {
            auto end_time = clock_type::now();
            double elapsed_time = std::chrono::duration<double>(end_time - start_time).count();

            if (rank == 0) {
                std::cout << "La première nourriture est arrivée au nid à l'iteration " << it << std::endl;
                std::cout << "Temps écoulé depuis le début : " << elapsed_time << " s" << std::endl;
            }
            not_food_in_nest = false;
            cont_loop = false;
        }
    }

    // ----- Résultats de performance (Processus 0) -----
    if (rank == 0) {
        std::cout << "\n===== RESULTATS DE PERFORMANCE (Rank 0) =====" << std::endl;
        std::cout << "Temps total déplacement des fourmis : " << total_time_move << " ms" << std::endl;
        std::cout << "Temps total evaporation pheromones : " << total_time_evap << " ms" << std::endl;
        std::cout << "Temps total update pheromones : " << total_time_update << " ms" << std::endl;
        std::cout << "\nTemps moyen par iteration :" << std::endl;
        std::cout << "Deplacement fourmis : " << total_time_move / it << " ms" << std::endl;
        std::cout << "Evaporation : " << total_time_evap / it << " ms" << std::endl;
        
        delete renderer;
        delete win;
        SDL_Quit();
    }

    MPI_Finalize();
    return 0;
}