#ifndef _PHERONOME_HPP_
#define _PHERONOME_HPP_

#include <algorithm>
#include <array>
#include <cassert>
#include <iostream>
#include <utility>
#include <vector>
#include "basic_types.hpp"
#include "fractal_land.hpp"

/**
 * @brief Carte des phéronomes (Version Vectorisée/SoA)
 */
class pheronome {
public:
    using size_t      = unsigned long;
    using pheronome_t = std::array< double, 2 >;

    pheronome( size_t dim, const position_t& pos_food, const position_t& pos_nest,
               double alpha = 0.7, double beta = 0.999 )
        : m_dim( dim ),
          m_stride( dim + 2 ),
          m_alpha(alpha), m_beta(beta),
          m_map_of_pheronome( m_stride * m_stride, {{0., 0.}} ),
          m_buffer_pheronome( m_stride * m_stride, {{0., 0.}} ),
          m_pos_nest( pos_nest ),
          m_pos_food( pos_food ) 
    {
        // Inicializa comida e ninho no mapa principal
        m_map_of_pheronome[index(m_pos_food)][0] = 1.0;
        m_map_of_pheronome[index(m_pos_nest)][1] = 1.0;
        
        cl_update(m_map_of_pheronome);
        m_buffer_pheronome = m_map_of_pheronome;
    }

    pheronome( const pheronome& ) = delete;
    pheronome( pheronome&& )      = delete;
    ~pheronome( )                 = default;

    // Acesso rápido (inline) para leitura
    const pheronome_t& operator( )( size_t i, size_t j ) const {
        return m_map_of_pheronome[( i + 1 ) * m_stride + ( j + 1 )];
    }

    const pheronome_t& operator[] ( const position_t& pos ) const {
        return m_map_of_pheronome[index(pos)];
    }

    /**
     * @brief Evaporação global dos phéromones
     */
    void do_evaporation( ) {
        // Percorre apenas o interior da grade (ignorando as bordas/células fantasma)
        for ( std::size_t i = 1; i <= m_dim; ++i ) {
            size_t row_offset = i * m_stride;
            for ( std::size_t j = 1; j <= m_dim; ++j ) {
                size_t idx = row_offset + j;
                m_buffer_pheronome[idx][0] *= m_beta;
                m_buffer_pheronome[idx][1] *= m_beta;
            }
        }
    }

    /**
     * @brief Atualização do phéromone por uma formiga
     * @param i Coordenada X
     * @param j Coordenada Y
     */
    void mark_pheronome( size_t i, size_t j ) {
        // Não sobrescrever a fonte de comida ou ninho
        if ((i == static_cast<size_t>(m_pos_food.x) && j == static_cast<size_t>(m_pos_food.y)) || 
            (i == static_cast<size_t>(m_pos_nest.x) && j == static_cast<size_t>(m_pos_nest.y)))
            return;
          

        // Leitura dos vizinhos no mapa estável
        const pheronome_t& left   = (*this)( i - 1, j );
        const pheronome_t& right  = (*this)( i + 1, j );
        const pheronome_t& upper  = (*this)( i, j - 1 );
        const pheronome_t& bottom = (*this)( i, j + 1 );

        // Cálculo seguindo a fórmula: alpha * max + (1-alpha) * avg
        size_t idx = ( i + 1 ) * m_stride + ( j + 1 );

        // V1 (Caminho para comida)
        double v1_max = std::max({left[0], right[0], upper[0], bottom[0]});
        double v1_avg = 0.25 * (left[0] + right[0] + upper[0] + bottom[0]);
        m_buffer_pheronome[idx][0] = m_alpha * v1_max + (1.0 - m_alpha) * v1_avg;

        // V2 (Caminho para o ninho)
        double v2_max = std::max({left[1], right[1], upper[1], bottom[1]});
        double v2_avg = 0.25 * (left[1] + right[1] + upper[1] + bottom[1]);
        m_buffer_pheronome[idx][1] = m_alpha * v2_max + (1.0 - m_alpha) * v2_avg;
    }

    /**
     * @brief Finaliza o passo de tempo, sincronizando os buffers
     */
    void update( ) {
        // Swap para tornar o buffer o mapa oficial
        m_map_of_pheronome.swap( m_buffer_pheronome );
        
        // Garante que as células de borda continuam como -1 (barreiras)
        cl_update( m_map_of_pheronome );
        
        // Reforça comida e ninho
        m_map_of_pheronome[index(m_pos_food)][0] = 1.0;
        m_map_of_pheronome[index(m_pos_nest)][1] = 1.0;

        // Prepara o buffer para a próxima iteração (cópia do estado atual)
        m_buffer_pheronome = m_map_of_pheronome;
    }

    // Retorna a dimensão (usado pelo Renderer)
    size_t dim() const { return m_dim; }

private:
    size_t index( const position_t& pos ) const {
        return (pos.x + 1) * m_stride + (pos.y + 1);
    }

    /**
     * @brief Define bordas como -1 para evitar que as formigas saiam do mapa
     */
    void cl_update( std::vector<pheronome_t>& map ) {
        for ( size_t j = 0; j < m_stride; ++j ) {
            map[j]                            = {{-1., -1.}}; // Topo
            map[j + m_stride * ( m_dim + 1 )] = {{-1., -1.}}; // Base
            map[j * m_stride]                 = {{-1., -1.}}; // Esquerda
            map[j * m_stride + m_dim + 1]     = {{-1., -1.}}; // Direita
        }
    }

    size_t m_dim, m_stride;
    double m_alpha, m_beta;
    std::vector< pheronome_t > m_map_of_pheronome, m_buffer_pheronome;
    position_t m_pos_nest, m_pos_food;
};

#endif