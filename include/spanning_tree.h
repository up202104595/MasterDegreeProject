#ifndef SPANNING_TREE_H
#define SPANNING_TREE_H

#include "tdma_types.h"

// Calcula a Spanning Tree (MST) baseada na matriz de conectividade
void spanning_tree_compute(connectivity_matrix_t *topo, spanning_tree_t *tree);

// Imprime a árvore para debug
void spanning_tree_print(spanning_tree_t *tree);

#endif