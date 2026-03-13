#include "ants.hpp"
#include "rand_generator.hpp"
#include <algorithm>
#include <omp.h> // Incluir para OpenMP

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
    
    // 1. Pegamos o tamanho uma vez para evitar chamadas de função no loop
    const std::size_t nb_ants = m_data.pos_x.size();

    // 2. Usamos dynamic scheduling porque o 'while' interno é imprevisível
    #pragma omp parallel for reduction(+:total_food_counter) schedule(dynamic, 128)
    for (std::size_t i = 0; i < nb_ants; ++i) {
        
        // --- CÓPIA PARA REGISTRADORES ---
        // Ler dos vetores uma única vez. Isso evita o tráfego de memória constante.
        int x = m_data.pos_x[i];
        int y = m_data.pos_y[i];
        int state = m_data.states[i];
        std::size_t seed_local = m_data.seeds[i];
        
        double consumed_time = 0.;
        
        while (consumed_time < 1.0) {
            int ind_pher = (state == 1 ? 1 : 0);
            
            // Usando as variáveis locais 'x' e 'y' em vez de acessar o vetor
            double p1 = phen(x - 1, y)[ind_pher];
            double p2 = phen(x + 1, y)[ind_pher];
            double p3 = phen(x, y - 1)[ind_pher];
            double p4 = phen(x, y + 1)[ind_pher];

            // 3. Substituindo std::max({...}) por if/else manuais
            // Isso elimina QUALQUER chance de alocação temporária
            double max_phen = p1;
            if (p2 > max_phen) max_phen = p2;
            if (p3 > max_phen) max_phen = p3;
            if (p4 > max_phen) max_phen = p4;

            int next_x = x;
            int next_y = y;
            double choix = rand_double(0., 1., seed_local);

            if ((choix > eps) || (max_phen <= 0.)) {
                do {
                    next_x = x; next_y = y;
                    int d = rand_int32(1, 4, seed_local);
                    if (d == 1) next_x--;
                    else if (d == 2) next_y--;
                    else if (d == 3) next_x++;
                    else next_y++;
                } while (phen(next_x, next_y)[ind_pher] == -1);
            } else {
                // Lógica de decisão sem criar objetos temporários
                if (p1 == max_phen)      next_x = x - 1;
                else if (p2 == max_phen) next_x = x + 1;
                else if (p3 == max_phen) next_y = y - 1;
                else                     next_y = y + 1;
            }

            consumed_time += land(next_x, next_y);
            phen.mark_pheronome(next_x, next_y);
            
            x = next_x;
            y = next_y;

            if (x == pos_nest.x && y == pos_nest.y) {
                if (state == 1) total_food_counter++;
                state = 0;
            }
            if (x == pos_food.x && y == pos_food.y) state = 1;
        }

        // --- SALVAMENTO FINAL ---
        // Escreve de volta nos vetores apenas no final do passo de tempo
        m_data.pos_x[i] = x;
        m_data.pos_y[i] = y;
        m_data.states[i] = state;
        m_data.seeds[i] = seed_local;
    }
}