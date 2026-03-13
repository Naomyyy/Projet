#include <vector>
#include <iostream>
#include <random>
#include "fractal_land.hpp"
#include "ants.hpp"      // Sua nova classe vetorizada
#include "pheronome.hpp"
#include "renderer.hpp"
#include "window.hpp"
#include "rand_generator.hpp"
#include <chrono>

void advance_time(const fractal_land& land, pheronome& phen, 
                  const position_t& pos_nest, const position_t& pos_food,
                  ants& all_ants, std::size_t& cpteur, double eps)
{
    // O loop interno agora acontece de forma vetorizada/paralela aqui dentro
    all_ants.advance_all(phen, land, pos_food, pos_nest, cpteur, eps);
    phen.do_evaporation();
    phen.update();
}

int main(int nargs, char* argv[])
{
    SDL_Init( SDL_INIT_VIDEO );
    
    std::size_t seed = 2026;
    const int nb_ants = 5000;
    const double eps = 0.8;
    const double alpha = 0.7;
    const double beta = 0.999;
    
    position_t pos_nest{256, 256};
    position_t pos_food{500, 500};
    
    fractal_land land(8, 2, 1., 1024);
    
    // --- Normalização do terreno ---
    double max_val = 0.0, min_val = 0.0;
    for (fractal_land::dim_t i = 0; i < land.dimensions(); ++i)
        for (fractal_land::dim_t j = 0; j < land.dimensions(); ++j) {
            max_val = std::max(max_val, land(i,j));
            min_val = std::min(min_val, land(i,j));
        }
    double delta = max_val - min_val;
    for (fractal_land::dim_t i = 0; i < land.dimensions(); ++i)
        for (fractal_land::dim_t j = 0; j < land.dimensions(); ++j)
            land(i,j) = (land(i,j)-min_val)/delta;

   
    ants all_ants(nb_ants, land, seed);

    pheronome phen(land.dimensions(), pos_food, pos_nest, alpha, beta);

    Window win("Ant Simulation (Vectorized)", 2*land.dimensions()+10, land.dimensions()+266);
    Renderer renderer( land, phen, pos_nest, pos_food, all_ants );

    size_t food_quantity = 0;
    SDL_Event event;
    bool cont_loop = true;
    bool not_food_in_nest = true;
    std::size_t it = 0;
    const std::size_t MAX_ITERATIONS = 4000; 

    double total_time_logic = 0.0;
    double total_time_render = 0.0;

    std::cout << "Lancement du benchmark VECTORISÉ pour " << MAX_ITERATIONS << " iterations..." << std::endl;

    while (cont_loop && it < MAX_ITERATIONS) {
        ++it;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) cont_loop = false;
        }

        auto start_logic = std::chrono::high_resolution_clock::now();

        advance_time( land, phen, pos_nest, pos_food, all_ants, food_quantity, eps );

        auto end_logic = std::chrono::high_resolution_clock::now();

        renderer.display( win, food_quantity );
        win.blit();

        auto end_render = std::chrono::high_resolution_clock::now();

        std::chrono::duration<double> diff_logic = end_logic - start_logic;
        std::chrono::duration<double> diff_render = end_render - end_logic;

        total_time_logic += diff_logic.count();
        total_time_render += diff_render.count();

        if ( not_food_in_nest && food_quantity > 0 ) {
            std::cout << "La premiere nourriture est arrivee au nid a l'iteration " << it << std::endl;
            not_food_in_nest = false;
        }
    }

    std::cout << "\n=== RÉSULTATS DU BENCHMARK VECTORISÉ/PARALLEL ===" << std::endl;
    std::cout << "Iterations completees : " << it << std::endl;
    std::cout << "Temps Total Logique (CPU) : " << total_time_logic << " secondes" << std::endl;
    std::cout << "Temps Total Rendu : " << total_time_render << " secondes" << std::endl;
    std::cout << "==========================================" << std::endl;

    SDL_Quit();
    return 0;
}