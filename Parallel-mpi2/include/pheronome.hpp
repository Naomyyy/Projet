#ifndef _PHERONOME_HPP_
#define _PHERONOME_HPP_

#include <algorithm>
#include <array>
#include <vector>
#include <mpi.h>
#include <omp.h>
#include "basic_types.hpp"

class pheronome {
public:
    using size_t      = unsigned long;
    using pheronome_t = std::array< double, 2 >;

    pheronome( size_t dim, const position_t& pos_food, const position_t& pos_nest,
               double alpha, double beta, int rank, int size )
        : m_dim( dim ), m_stride( dim + 2 ), m_alpha(alpha), m_beta(beta),
          m_pos_nest( pos_nest ), m_pos_food( pos_food ) 
    {
        // Cálculo da fatia (Slicing)
        size_t rows_per_proc = dim / size;
        m_i_start = rank * rows_per_proc + 1;
        m_i_end   = (rank == size - 1) ? dim : m_i_start + rows_per_proc - 1;

        // Alocamos o mapa inteiro para facilitar o Renderer, 
        // mas cada processo SÓ processará sua faixa.
        m_map_of_pheronome.assign( m_stride * m_stride, {{0., 0.}} );
        m_buffer_pheronome.assign( m_stride * m_stride, {{0., 0.}} );

        m_map_of_pheronome[index(m_pos_food)][0] = 1.0;
        m_map_of_pheronome[index(m_pos_nest)][1] = 1.0;
        
        cl_update(m_map_of_pheronome);
        m_buffer_pheronome = m_map_of_pheronome;
    }

    /**
     * @brief Troca de bordas (Halo Exchange)
     * Substitui o Allreduce. Muito mais leve!
     */
    void exchange_borders(int rank, int size) {
        int up_proc   = (rank == 0) ? MPI_PROC_NULL : rank - 1;
        int down_proc = (rank == size - 1) ? MPI_PROC_NULL : rank + 1;

        // 1. Enviar minha primeira linha real para cima / Receber na minha ghost line de cima
        // Cada pheronome_t tem 2 doubles, por isso count = m_stride * 2
        MPI_Sendrecv(&m_map_of_pheronome[m_i_start * m_stride], m_stride * 2, MPI_DOUBLE, up_proc, 0,
                     &m_map_of_pheronome[(m_i_start - 1) * m_stride], m_stride * 2, MPI_DOUBLE, up_proc, 0,
                     MPI_COMM_WORLD, MPI_STATUS_IGNORE);

        // 2. Enviar minha última linha real para baixo / Receber na minha ghost line de baixo
        MPI_Sendrecv(&m_map_of_pheronome[m_i_end * m_stride], m_stride * 2, MPI_DOUBLE, down_proc, 1,
                     &m_map_of_pheronome[(m_i_end + 1) * m_stride], m_stride * 2, MPI_DOUBLE, down_proc, 1,
                     MPI_COMM_WORLD, MPI_STATUS_IGNORE);
    }

    void do_evaporation() {
        // Cada processo evapora apenas a sua faixa
        #pragma omp parallel for collapse(2)
        for ( size_t i = m_i_start; i <= m_i_end; ++i ) {
            for ( size_t j = 1; j <= m_dim; ++j ) {
                size_t idx = i * m_stride + j;
                m_buffer_pheronome[idx][0] *= m_beta;
                m_buffer_pheronome[idx][1] *= m_beta;
            }
        }
    }

    void mark_pheronome( size_t i, size_t j ) {
        // A formiga só marca se estiver na minha faixa
        if (i < m_i_start || i > m_i_end) return;

        if ((i == (size_t)m_pos_food.x && j == (size_t)m_pos_food.y) || 
            (i == (size_t)m_pos_nest.x && j == (size_t)m_pos_nest.y))
            return;

        const pheronome_t& left   = m_map_of_pheronome[i * m_stride + (j)];
        const pheronome_t& right  = m_map_of_pheronome[i * m_stride + (j + 2)];
        const pheronome_t& upper  = m_map_of_pheronome[(i - 1) * m_stride + (j + 1)];
        const pheronome_t& bottom = m_map_of_pheronome[(i + 1) * m_stride + (j + 1)];

        size_t idx = i * m_stride + (j + 1);

        // Otimização: max manual para evitar alocação de initializer_list
        for (int p = 0; p < 2; ++p) {
            double m1 = std::max(left[p], right[p]);
            double m2 = std::max(upper[p], bottom[p]);
            double max_val = std::max(m1, m2);
            double avg_val = 0.25 * (left[p] + right[p] + upper[p] + bottom[p]);
            m_buffer_pheronome[idx][p] = m_alpha * max_val + (1.0 - m_alpha) * avg_val;
        }
    }

    void update() {
        m_map_of_pheronome.swap( m_buffer_pheronome );
        
        // Reforçar limites apenas na minha faixa
        m_map_of_pheronome[index(m_pos_food)][0] = 1.0;
        m_map_of_pheronome[index(m_pos_nest)][1] = 1.0;

        #pragma omp parallel for
        for (size_t i = m_i_start; i <= m_i_end; ++i) {
            for(size_t j=0; j < m_stride; ++j) {
                size_t idx = i * m_stride + j;
                m_buffer_pheronome[idx] = m_map_of_pheronome[idx];
            }
        }
    }

    // Acessores
    const pheronome_t& operator( )( size_t i, size_t j ) const {
        return m_map_of_pheronome[(i + 1) * m_stride + (j + 1)];
    }
    size_t dim() const { return m_dim; }
    
    // Para o Renderer saber onde as formigas podem estar
    size_t i_start() const { return m_i_start; }
    size_t i_end() const { return m_i_end; }

private:
    size_t index( const position_t& pos ) const {
        return (pos.x + 1) * m_stride + (pos.y + 1);
    }

    void cl_update( std::vector<pheronome_t>& map ) {
        // Barreiras externas (borda do mundo)
        for ( size_t j = 0; j < m_stride; ++j ) {
            map[j]                            = {{-1., -1.}};
            map[j + m_stride * ( m_dim + 1 )] = {{-1., -1.}};
            map[j * m_stride]                 = {{-1., -1.}};
            map[j * m_stride + m_dim + 1]     = {{-1., -1.}};
        }
    }

    size_t m_dim, m_stride;
    size_t m_i_start, m_i_end; // Limites da fatia
    double m_alpha, m_beta;
    std::vector< pheronome_t > m_map_of_pheronome, m_buffer_pheronome;
    position_t m_pos_nest, m_pos_food;
};

#endif