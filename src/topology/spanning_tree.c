// src/topology/spanning_tree.c
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include "tdma_types.h"
#include "spanning_tree.h"

void spanning_tree_compute(connectivity_matrix_t *topo, spanning_tree_t *tree) {
    // Implementação de Prim (igual à da Ana)
    
    memset(tree->tree, 0, sizeof(tree->tree));
    tree->num_nodes = topo->num_nodes;
    memcpy(tree->node_ids, topo->node_ids, sizeof(tree->node_ids));
    
    if (topo->num_nodes == 0) return;
    
    bool in_tree[MAX_NODES] = {false};
    int parent[MAX_NODES];
    int key[MAX_NODES];
    
    // Initialize
    for (int i = 0; i < topo->num_nodes; i++) {
        key[i] = INT_MAX;
        parent[i] = -1;
    }
    
    // Start from first node
    key[0] = 0;
    
    // Prim's algorithm
    for (int count = 0; count < topo->num_nodes; count++) {
        // Find minimum key node not in tree
        int min_key = INT_MAX;
        int u = -1;
        
        for (int v = 0; v < topo->num_nodes; v++) {
            if (!in_tree[v] && key[v] < min_key) {
                min_key = key[v];
                u = v;
            }
        }
        
        if (u == -1) break;  // No more reachable nodes
        
        in_tree[u] = true;
        
        // Add edge to tree
        if (parent[u] != -1) {
            tree->tree[parent[u]][u] = 1;
            tree->tree[u][parent[u]] = 1;
        }
        
        // Update keys of adjacent nodes
        for (int v = 0; v < topo->num_nodes; v++) {
            if (topo->matrix[u][v] && !in_tree[v] && topo->matrix[u][v] < key[v]) {
                parent[v] = u;
                key[v] = topo->matrix[u][v];
            }
        }
    }
    
    printf("[SPANNING TREE] Computed for %d nodes\n", tree->num_nodes);
}

void spanning_tree_print(spanning_tree_t *tree) {
    printf("\n=== Spanning Tree ===\n");
    printf("    ");
    for (int i = 0; i < tree->num_nodes; i++) {
        printf("%3d ", tree->node_ids[i]);
    }
    printf("\n");
    
    for (int i = 0; i < tree->num_nodes; i++) {
        printf("%3d ", tree->node_ids[i]);
        for (int j = 0; j < tree->num_nodes; j++) {
            printf("%3d ", tree->tree[i][j]);
        }
        printf("\n");
    }
    printf("\n");
}