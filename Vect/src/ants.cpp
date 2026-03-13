#include "ants.hpp"
#include "rand_generator.hpp"
#include <algorithm>

ants::ants(std::size_t nb_ants, const fractal_land& land, unsigned int base_seed) {
    m_data.states.resize(nb_ants, 0);
    m_data.pos_x.resize(nb_ants);
    m_data.pos_y.resize(nb_ants);
    m_data.seeds.resize(nb_ants);

    std::size_t master_seed = base_seed;
    int max_dim = static_cast<int>(land.dimensions()) - 1;

    for (std::size_t i = 0; i < nb_ants; ++i) {
        m_data.seeds[i] = base_seed + i;
        m_data.pos_x[i] = rand_int32(0, max_dim, master_seed);
        m_data.pos_y[i] = rand_int32(0, max_dim, master_seed);
    }
}

void ants::advance_all(pheronome& phen, const fractal_land& land,
                       const position_t& pos_food, const position_t& pos_nest, 
                       std::size_t& total_food_counter, double eps) {

    for (std::size_t i = 0; i < m_data.pos_x.size(); ++i) {
        
        double consumed_time = 0.;
        
        while (consumed_time < 1.0) {
            int ind_pher = (m_data.states[i] == 1 ? 1 : 0);
            double choix = rand_double(0., 1., m_data.seeds[i]);
            
            int cur_x = m_data.pos_x[i];
            int cur_y = m_data.pos_y[i];
            int next_x = cur_x;
            int next_y = cur_y;

            double max_phen = std::max({
                phen(cur_x - 1, cur_y)[ind_pher],
                phen(cur_x + 1, cur_y)[ind_pher],
                phen(cur_x, cur_y - 1)[ind_pher],
                phen(cur_x, cur_y + 1)[ind_pher]
            });

            if ((choix > eps) || (max_phen <= 0.)) {
                do {
                    next_x = cur_x;
                    next_y = cur_y;
                    int d = rand_int32(1, 4, m_data.seeds[i]);
                    if (d == 1) next_x -= 1;
                    if (d == 2) next_y -= 1;
                    if (d == 3) next_x += 1;
                    if (d == 4) next_y += 1;
                } while (phen(next_x, next_y)[ind_pher] == -1);
            } else {
                if (phen(cur_x - 1, cur_y)[ind_pher] == max_phen)
                    next_x -= 1;
                else if (phen(cur_x + 1, cur_y)[ind_pher] == max_phen)
                    next_x += 1;
                else if (phen(cur_x, cur_y - 1)[ind_pher] == max_phen)
                    next_y -= 1;
                else
                    next_y += 1;
            }

            consumed_time += land(next_x, next_y);
            
            phen.mark_pheronome(next_x, next_y);
            
            m_data.pos_x[i] = next_x;
            m_data.pos_y[i] = next_y;

            if (m_data.pos_x[i] == pos_nest.x && m_data.pos_y[i] == pos_nest.y) {
                if (m_data.states[i] == 1) total_food_counter += 1;
                m_data.states[i] = 0;
            }
            if (m_data.pos_x[i] == pos_food.x && m_data.pos_y[i] == pos_food.y) {
                m_data.states[i] = 1;
            }
        }
    }
}