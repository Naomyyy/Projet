#ifndef _ANT_HPP_
#define _ANT_HPP_

#include <vector>
#include "pheronome.hpp"
#include "fractal_land.hpp"
#include "basic_types.hpp"

class ant
{
public:

    enum state { unloaded = 0, loaded = 1 };

    ant(const position_t& pos, std::size_t seed);

    void set_loaded() { states[m_id] = loaded; }
    void unset_loaded() { states[m_id] = unloaded; }

    bool is_loaded() const { return states[m_id] == loaded; }

    position_t get_position() const
{
    return position_t{xs[m_id], ys[m_id]};
}

    static void set_exploration_coef(double eps) { m_eps = eps; }

    void advance( pheronome& phen,
                  const fractal_land& land,
                  const position_t& pos_food,
                  const position_t& pos_nest,
                  std::size_t& cpteur_food );

private:

    // index de la fourmi
    std::size_t m_id;

    static double m_eps;

    // stockage vectorisé des données
    static std::vector<int> xs;
    static std::vector<int> ys;
    static std::vector<state> states;
    static std::vector<std::size_t> seeds;
    };

#endif