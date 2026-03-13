#ifndef _ANTS_HPP_
#define _ANTS_HPP_

#include <vector>
#include "basic_types.hpp"
#include "pheronome.hpp"
#include "fractal_land.hpp"
#include <cstddef>

class ants {
public:
    struct data {
        std::vector<int> states;          
        std::vector<int> pos_x;           
        std::vector<int> pos_y;           
        std::vector<std::size_t> seeds;  
    } m_data;

 
    ants(std::size_t nb_ants, const fractal_land& land, unsigned int base_seed);

    void advance_all(pheronome& phen, const fractal_land& land,
                     const position_t& pos_food, const position_t& pos_nest, 
                     std::size_t& local_food_counter, double eps);


    std::size_t size() const { return m_data.pos_x.size(); }
};

#endif