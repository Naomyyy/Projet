#include "ant.hpp"
#include <iostream>
#include "rand_generator.hpp"

double ant::m_eps = 0.;

std::vector<int> ant::xs;
std::vector<int> ant::ys;
std::vector<ant::state> ant::states;
std::vector<std::size_t> ant::seeds;

// ----- Constructeur -----
ant::ant(const position_t& pos, std::size_t seed)
{
    // L'identifiant de la fourmi correspond à l'indice dans les tableaux
    m_id = xs.size();

    xs.push_back(pos.x);
    ys.push_back(pos.y);
    states.push_back(unloaded);
    seeds.push_back(seed);
}

void ant::advance( pheronome& phen, const fractal_land& land,
                   const position_t& pos_food, const position_t& pos_nest,
                   std::size_t& cpteur_food )
{
    // Générateurs aléatoires utilisant la seed de cette fourmi
    auto ant_choice = [this]() mutable { return rand_double(0., 1., seeds[m_id]); };
    auto dir_choice = [this]() mutable { return rand_int32(1, 4, seeds[m_id]); };

    double consumed_time = 0.;

    // Tant que la fourmi peut encore bouger dans le pas de temps imparti
    while (consumed_time < 1.) {

        // Si la fourmi est chargée, elle suit les phéromones de type 1 sinon type 0
        int ind_pher = (states[m_id] == loaded ? 1 : 0);

        double choix = ant_choice();

        // Position actuelle de la fourmi
        position_t old_pos_ant { xs[m_id], ys[m_id] };
        position_t new_pos_ant = old_pos_ant;

        // Recherche de la valeur maximale de phéromone autour
        double max_phen = std::max({
            phen(new_pos_ant.x - 1, new_pos_ant.y)[ind_pher],
            phen(new_pos_ant.x + 1, new_pos_ant.y)[ind_pher],
            phen(new_pos_ant.x, new_pos_ant.y - 1)[ind_pher],
            phen(new_pos_ant.x, new_pos_ant.y + 1)[ind_pher]
        });

        // Exploration aléatoire
        if ((choix > m_eps) || (max_phen <= 0.)) {

            do {
                new_pos_ant = old_pos_ant;

                int d = dir_choice();

                if (d == 1) new_pos_ant.x -= 1;
                if (d == 2) new_pos_ant.y -= 1;
                if (d == 3) new_pos_ant.x += 1;
                if (d == 4) new_pos_ant.y += 1;

            } while (phen[new_pos_ant][ind_pher] == -1);

        }
        else {
            // Choix de la direction avec le plus de phéromone
            if (phen(new_pos_ant.x - 1, new_pos_ant.y)[ind_pher] == max_phen)
                new_pos_ant.x -= 1;
            else if (phen(new_pos_ant.x + 1, new_pos_ant.y)[ind_pher] == max_phen)
                new_pos_ant.x += 1;
            else if (phen(new_pos_ant.x, new_pos_ant.y - 1)[ind_pher] == max_phen)
                new_pos_ant.y -= 1;
            else
                new_pos_ant.y += 1;
        }

        // Temps consommé selon le terrain
        consumed_time += land(new_pos_ant.x, new_pos_ant.y);

        // Dépôt de phéromone
        phen.mark_pheronome(new_pos_ant);

        // Mise à jour de la position dans les tableaux vectorisés
        xs[m_id] = new_pos_ant.x;
        ys[m_id] = new_pos_ant.y;

        // Si la fourmi arrive au nid
        if (xs[m_id] == pos_nest.x && ys[m_id] == pos_nest.y) {

            if (states[m_id] == loaded) {
                cpteur_food += 1;
            }

            states[m_id] = unloaded;
        }

        // Si la fourmi trouve de la nourriture
        if (xs[m_id] == pos_food.x && ys[m_id] == pos_food.y) {
            states[m_id] = loaded;
        }
    }
}