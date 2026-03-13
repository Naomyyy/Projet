#ifndef _ANTS_HPP_
#define _ANTS_HPP_

#include <vector>
#include <random>
#include "basic_types.hpp"
#include "pheronome.hpp"
#include "fractal_land.hpp"
#include <cstddef>

class ants {
public:
    // Estrutura de Arrays (SoA)
    struct data {
        std::vector<int> states;          // 0: unloaded, 1: loaded
        std::vector<int> pos_x;           // Coordenada X
        std::vector<int> pos_y;           // Coordenada Y
        std::vector<std::size_t> seeds;  // Sementes para geração aleatória local
    } m_data;

    ants(std::size_t nb_ants, const fractal_land& land, unsigned int base_seed);

    // Método principal que processa todas as formigas
    void advance_all(pheronome& phen, const fractal_land& land,
                     const position_t& pos_food, const position_t& pos_nest, 
                     std::size_t& total_food_counter, double eps);

private:
    // Função auxiliar para o movimento de UMA formiga (chamada dentro do loop)
    void move_ant(std::size_t i, pheronome& phen, const fractal_land& land,
                  const position_t& pos_food, const position_t& pos_nest, 
                  std::size_t& food_counter, double eps);
};

#endif