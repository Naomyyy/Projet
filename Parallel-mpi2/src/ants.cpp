#include "ants.hpp"
#include "rand_generator.hpp"
#include <algorithm>
#include <omp.h>
#include <mpi.h>

// Struct para empacotar os dados da formiga para envio via MPI
struct ant_packet {
    int x, y, state;
    std::size_t seed; 
};

// Implementação do Construtor (Necessário para resolver o erro de Linkagem)
ants::ants(std::size_t nb_ants, const fractal_land& land, std::size_t base_seed) {
    m_data.states.resize(nb_ants, 0);
    m_data.pos_x.resize(nb_ants);
    m_data.pos_y.resize(nb_ants);
    m_data.seeds.resize(nb_ants);

    int max_dim = static_cast<int>(land.dimensions()) - 1;
    std::size_t master_seed = base_seed;

    for (std::size_t i = 0; i < nb_ants; ++i) {
        m_data.seeds[i] = base_seed + i; 
        // Inicializa formigas em posições aleatórias (ou no ninho, dependendo da sua main)
        m_data.pos_x[i] = rand_int32(0, max_dim, master_seed);
        m_data.pos_y[i] = rand_int32(0, max_dim, master_seed);
    }
}

void ants::advance_all(pheronome& phen, const fractal_land& land,
                       const position_t& pos_food, const position_t& pos_nest, 
                       std::size_t& local_food_counter, double eps, int rank, int size) {
    
    int dim = static_cast<int>(land.dimensions());
    int rows_per_proc = dim / size;
    int y_min = rank * rows_per_proc;
    int y_max = (rank == size - 1) ? dim - 1 : y_min + rows_per_proc - 1;

    std::vector<ant_packet> send_up, send_down;
    
    #pragma omp parallel
    {
        std::vector<ant_packet> local_up, local_down;
        
        #pragma omp for reduction(+:local_food_counter)
        for (std::size_t i = 0; i < m_data.pos_x.size(); ++i) {
            double consumed_time = 0.;
            int x = m_data.pos_x[i];
            int y = m_data.pos_y[i];
            int state = m_data.states[i];
            std::size_t seed = m_data.seeds[i]; // Tipo correto para rand_generator

            while (consumed_time < 1.0) {
                int ind_pher = (state == 1 ? 1 : 0);
                double choix = rand_double(0., 1., seed);
                
                int next_x = x, next_y = y;

                double p_l = phen(x - 1, y)[ind_pher];
                double p_r = phen(x + 1, y)[ind_pher];
                double p_u = phen(x, y - 1)[ind_pher];
                double p_d = phen(x, y + 1)[ind_pher];

                double max_p = std::max({p_l, p_r, p_u, p_d});

                if ((choix > eps) || (max_p <= 0.)) {
                    do {
                        next_x = x; next_y = y;
                        int d = rand_int32(1, 4, seed);
                        if (d == 1) next_x--;
                        else if (d == 2) next_y--;
                        else if (d == 3) next_x++;
                        else next_y++;
                    } while (phen(next_x, next_y)[ind_pher] == -1);
                } else {
                    if (p_l == max_p) next_x = x - 1;
                    else if (p_r == max_p) next_x = x + 1;
                    else if (p_u == max_p) next_y = y - 1;
                    else next_y = y + 1;
                }

                consumed_time += land(next_x, next_y);
                phen.mark_pheronome(next_x, next_y);
                x = next_x; y = next_y;

                if (x == pos_nest.x && y == pos_nest.y) {
                    if (state == 1) local_food_counter++;
                    state = 0;
                }
                if (x == pos_food.x && y == pos_food.y) state = 1;

                if (y < y_min || y > y_max) break;
            }

            m_data.seeds[i] = seed;

            if (y < y_min) {
                local_up.push_back({x, y, state, seed});
                m_data.pos_x[i] = -1;
            } else if (y > y_max) {
                local_down.push_back({x, y, state, seed});
                m_data.pos_x[i] = -1;
            } else {
                m_data.pos_x[i] = x; m_data.pos_y[i] = y;
                m_data.states[i] = state;
            }
        }
        #pragma omp critical
        {
            send_up.insert(send_up.end(), local_up.begin(), local_up.end());
            send_down.insert(send_down.end(), local_down.begin(), local_down.end());
        }
    }

    // Compactação (remoção de formigas que saíram do processo)
    std::size_t valid = 0;
    for (std::size_t i = 0; i < m_data.pos_x.size(); ++i) {
        if (m_data.pos_x[i] != -1) {
            m_data.pos_x[valid] = m_data.pos_x[i];
            m_data.pos_y[valid] = m_data.pos_y[i];
            m_data.states[valid] = m_data.states[i];
            m_data.seeds[valid] = m_data.seeds[i];
            valid++;
        }
    }
    m_data.pos_x.resize(valid); m_data.pos_y.resize(valid);
    m_data.states.resize(valid); m_data.seeds.resize(valid);

    // Lógica de Migração via MPI_Sendrecv
    auto migrate = [&](std::vector<ant_packet>& to_send, int dest, int tag_send, int tag_recv) {
        if (size == 1) return;
        int send_count = (int)to_send.size();
        int recv_count = 0;
        int target = (dest < 0 || dest >= size) ? MPI_PROC_NULL : dest;

        MPI_Status status;
        MPI_Sendrecv(&send_count, 1, MPI_INT, target, tag_send,
                     &recv_count, 1, MPI_INT, target, tag_recv,
                     MPI_COMM_WORLD, &status);

        if (recv_count > 0 || send_count > 0) {
            std::vector<ant_packet> received(recv_count);
            MPI_Sendrecv(to_send.data(), send_count * sizeof(ant_packet), MPI_BYTE, target, tag_send + 10,
                         received.data(), recv_count * sizeof(ant_packet), MPI_BYTE, target, tag_recv + 10,
                         MPI_COMM_WORLD, &status);
            
            for(const auto& a : received) {
                m_data.pos_x.push_back(a.x); m_data.pos_y.push_back(a.y);
                m_data.states.push_back(a.state); m_data.seeds.push_back(a.seed);
            }
        }
    };

    migrate(send_up, rank - 1, 100, 101);
    migrate(send_down, rank + 1, 101, 100);
}