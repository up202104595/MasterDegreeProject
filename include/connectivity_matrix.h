// include/connectivity_matrix.h
#ifndef CONNECTIVITY_MATRIX_H
#define CONNECTIVITY_MATRIX_H

#include "tdma_types.h"

void connectivity_matrix_init(void);
void connectivity_matrix_set_topology(uint8_t matrix[MAX_NODES][MAX_NODES],
                                       node_id_t *node_ids,
                                       uint8_t num_nodes);
void connectivity_matrix_get(connectivity_matrix_t *output);
void connectivity_matrix_print(void);

#endif