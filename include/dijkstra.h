// include/dijkstra.h
#ifndef DIJKSTRA_H
#define DIJKSTRA_H

#include "tdma_types.h"

#define INFINITY_COST 255

/**
 * Estrutura para resultado do Dijkstra
 */
typedef struct {
    node_id_t destination;
    node_id_t next_hop;        // Próximo hop no caminho
    uint8_t distance;          // Número de hops
    bool reachable;
} dijkstra_result_t;

/**
 * Executa Dijkstra a partir de um source node
 * 
 * @param src Source node ID
 * @param topology Connectivity matrix
 * @param results Array de resultados [MAX_NODES]
 * @return 0 em sucesso, -1 em erro
 */
int dijkstra_compute(node_id_t src,
                     connectivity_matrix_t *topology,
                     dijkstra_result_t results[MAX_NODES]);

/**
 * Imprime resultados do Dijkstra
 */
void dijkstra_print_results(node_id_t src, 
                           dijkstra_result_t results[MAX_NODES],
                           int num_nodes);

/**
 * Compara dois paths para verificar se mudaram
 * @return true se paths são diferentes
 */
bool dijkstra_path_changed(dijkstra_result_t *old, dijkstra_result_t *new);

#endif