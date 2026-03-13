#ifndef _PHERONOME_HPP_
#define _PHERONOME_HPP_

#include <algorithm>
#include <array>
#include <vector>
#include <omp.h>
#include "basic_types.hpp"

class pheronome {
public:
    using size_t      = unsigned long;
    using pheronome_t = std::array< double, 2 >;

    pheronome( size_t dim, const position_t& pos_food, const position_t& pos_nest,
               double alpha = 0.7, double beta = 0.999 )
        : m_dim( dim ), m_stride( dim + 2 ), m_alpha(alpha), m_beta(beta),
          m_map_of_pheronome( m_stride * m_stride, {{0., 0.}} ),
          m_buffer_pheronome( m_stride * m_stride, {{0., 0.}} ),
          m_pos_nest( pos_nest ), m_pos_food( pos_food ) 
    {
        m_map_of_pheronome[index(m_pos_food)][0] = 1.0;
        m_map_of_pheronome[index(m_pos_nest)][1] = 1.0;
        cl_update(m_map_of_pheronome);
        m_buffer_pheronome = m_map_of_pheronome;
    }

    // --- MÉTODOS PARA MPI ---

    /**
     * @brief Retorna o ponteiro bruto para os dados do buffer.
     * Como pheronome_t é array<double, 2>, os dados são contíguos.
     */
    double* get_buffer_ptr() {
        return &m_buffer_pheronome[0][0];
    }

    /**
     * @brief Retorna a quantidade total de DOUBLES no buffer.
     * (Dimensão total da grade * 2 tipos de feromônio)
     */
    size_t get_buffer_size() const {
        return m_buffer_pheronome.size() * 2;
    }

    // --- LÓGICA DE ATUALIZAÇÃO ---

    void do_evaporation() {
        // Na estratégia MPI, todos podem evaporar tudo após o Allreduce,
        // ou você pode segmentar por rank/size se quiser otimizar.
        #pragma omp parallel for collapse(2)
        for ( std::size_t i = 1; i <= m_dim; ++i ) {
            for ( std::size_t j = 1; j <= m_dim; ++j ) {
                size_t idx = i * m_stride + j;
                m_buffer_pheronome[idx][0] *= m_beta;
                m_buffer_pheronome[idx][1] *= m_beta;
            }
        }
    }

    void mark_pheronome( size_t i, size_t j ) {
        if ((i == (size_t)m_pos_food.x && j == (size_t)m_pos_food.y) || 
            (i == (size_t)m_pos_nest.x && j == (size_t)m_pos_nest.y))
            return;

        const pheronome_t& left   = (*this)( i - 1, j );
        const pheronome_t& right  = (*this)( i + 1, j );
        const pheronome_t& upper  = (*this)( i, j - 1 );
        const pheronome_t& bottom = (*this)( i, j + 1 );

        size_t idx = ( i + 1 ) * m_stride + ( j + 1 );

        m_buffer_pheronome[idx][0] = m_alpha * std::max({left[0], right[0], upper[0], bottom[0]}) + 
                                     (1.0 - m_alpha) * 0.25 * (left[0] + right[0] + upper[0] + bottom[0]);

        m_buffer_pheronome[idx][1] = m_alpha * std::max({left[1], right[1], upper[1], bottom[1]}) + 
                                     (1.0 - m_alpha) * 0.25 * (left[1] + right[1] + upper[1] + bottom[1]);
    }

    void update() {
        m_map_of_pheronome.swap( m_buffer_pheronome );
        cl_update( m_map_of_pheronome );
        
        m_map_of_pheronome[index(m_pos_food)][0] = 1.0;
        m_map_of_pheronome[index(m_pos_nest)][1] = 1.0;

        // Sincroniza o buffer para a próxima iteração
        #pragma omp parallel for
        for (std::size_t i = 0; i < m_map_of_pheronome.size(); ++i) {
            m_buffer_pheronome[i] = m_map_of_pheronome[i];
        }
    }

    // Acessores básicos
    const pheronome_t& operator( )( size_t i, size_t j ) const {
        return m_map_of_pheronome[( i + 1 ) * m_stride + ( j + 1 )];
    }
    size_t dim() const { return m_dim; }

private:
    size_t index( const position_t& pos ) const {
        return (pos.x + 1) * m_stride + (pos.y + 1);
    }

    void cl_update( std::vector<pheronome_t>& map ) {
        for ( size_t j = 0; j < m_stride; ++j ) {
            map[j]                            = {{-1., -1.}};
            map[j + m_stride * ( m_dim + 1 )] = {{-1., -1.}};
            map[j * m_stride]                 = {{-1., -1.}};
            map[j * m_stride + m_dim + 1]     = {{-1., -1.}};
        }
    }

    size_t m_dim, m_stride;
    double m_alpha, m_beta;
    std::vector< pheronome_t > m_map_of_pheronome, m_buffer_pheronome;
    position_t m_pos_nest, m_pos_food;
};

#endif