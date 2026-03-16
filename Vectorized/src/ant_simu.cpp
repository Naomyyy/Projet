#include <vector>
#include <iostream>
#include <random>
#include <chrono>

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
    // Nombre de fourmis (évite de recalculer size() à chaque itération)
    const std::size_t n = ants.size();

    // ----- Mesure du temps du déplacement des fourmis -----
    auto t1 = clock_type::now();

    for (std::size_t i = 0; i < n; ++i)
        ants[i].advance(phen, land, pos_food, pos_nest, cpteur);

    auto t2 = clock_type::now();


    // ----- Mesure du temps d'évaporation des phéromones -----
    phen.do_evaporation();

    auto t3 = clock_type::now();


    // ----- Mesure du temps de mise à jour des phéromones -----
    phen.update();

    auto t4 = clock_type::now();


    // Calcul des durées
    double move_time =
        std::chrono::duration<double, std::milli>(t2 - t1).count();

    double evap_time =
        std::chrono::duration<double, std::milli>(t3 - t2).count();

    double update_time =
        std::chrono::duration<double, std::milli>(t4 - t3).count();


    // Accumulation des temps
    total_time_move += move_time;
    total_time_evap += evap_time;
    total_time_update += update_time;
}


int main(int nargs, char* argv[])
{
    SDL_Init(SDL_INIT_VIDEO);

    std::size_t seed = 2026;

    const int nb_ants = 5000;
    const double eps = 0.8;
    const double alpha = 0.7;
    const double beta = 0.999;

    // Position du nid
    position_t pos_nest{256,256};

    // Position de la nourriture
    position_t pos_food{500,500};

    // Génération du terrain fractal
    fractal_land land(8,2,1.,1024);

    double max_val = 0.0;
    double min_val = 0.0;

    for (fractal_land::dim_t i = 0; i < land.dimensions(); ++i)
        for (fractal_land::dim_t j = 0; j < land.dimensions(); ++j) {
            max_val = std::max(max_val, land(i,j));
            min_val = std::min(min_val, land(i,j));
        }

    double delta = max_val - min_val;

    // Normalisation du terrain
    for (fractal_land::dim_t i = 0; i < land.dimensions(); ++i)
        for (fractal_land::dim_t j = 0; j < land.dimensions(); ++j)
            land(i,j) = (land(i,j) - min_val) / delta;


    // Paramètre d'exploration des fourmis
    ant::set_exploration_coef(eps);


    // ----- Création des fourmis -----
    std::vector<ant> ants;
    ants.reserve(nb_ants);

    auto gen_ant_pos = [&land, &seed]()
    {
        return rand_int32(0, land.dimensions()-1, seed);
    };

    for (std::size_t i = 0; i < nb_ants; ++i)
        ants.emplace_back(position_t{gen_ant_pos(), gen_ant_pos()}, seed);


    // Initialisation des phéromones
    pheronome phen(land.dimensions(), pos_food, pos_nest, alpha, beta);


    Window win("Ant Simulation", 2*land.dimensions()+10, land.dimensions()+266);
    Renderer renderer(land, phen, pos_nest, pos_food, ants);


    // Compteur de nourriture
    std::size_t food_quantity = 0;

    SDL_Event event;
    bool cont_loop = true;
    bool not_food_in_nest = true;

    std::size_t it = 0;


    // ----- Chronomètre global -----
    auto start_time = clock_type::now();


    while (cont_loop)
    {
        ++it;

        while (SDL_PollEvent(&event))
        {
            if (event.type == SDL_QUIT)
                cont_loop = false;
        }


        advance_time(land, phen, pos_nest, pos_food, ants, food_quantity);


        renderer.display(win, food_quantity);
        win.blit();


        if (not_food_in_nest && food_quantity > 0)
        {
            auto end_time = clock_type::now();

            double elapsed_time =
                std::chrono::duration<double>(end_time - start_time).count();

            std::cout << "La première nourriture est arrivée au nid à l'iteration "
                      << it << std::endl;

            std::cout << "Temps écoulé depuis le début : "
                      << elapsed_time << " s" << std::endl;

            not_food_in_nest = false;

            cont_loop = false;
        }
    }


    SDL_Quit();


    // ----- Résultats de performance -----
    std::cout << "\n===== RESULTATS DE PERFORMANCE =====" << std::endl;

    std::cout << "Temps total déplacement des fourmis : "
              << total_time_move << " ms" << std::endl;

    std::cout << "Temps total evaporation pheromones : "
              << total_time_evap << " ms" << std::endl;

    std::cout << "Temps total update pheromones : "
              << total_time_update << " ms" << std::endl;


    std::cout << "\nTemps moyen par iteration :" << std::endl;

    std::cout << "Deplacement fourmis : "
              << total_time_move / it << " ms" << std::endl;

    std::cout << "Evaporation : "
              << total_time_evap / it << " ms" << std::endl;

    std::cout << "Update pheromones : "
              << total_time_update / it << " ms" << std::endl;


    return 0;
}