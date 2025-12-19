// src/topology/connectivity_matrix.c
#include <stdio.h>
#include <string.h>
#include <time.h>
#include "tdma_types.h"
#include <pthread.h>             // <--- Necessário para pthread_mutex_t
#include "connectivity_matrix.h" // <--- ADICIONE ISTO (para ligar ao seu .

// Global connectivity matrix (simula shared memory)
static connectivity_matrix_t global_topology;

void connectivity_matrix_init(void) {
    memset(&global_topology, 0, sizeof(global_topology));
    pthread_mutex_init(&global_topology.lock, NULL);
    
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    global_topology.timestamp = now.tv_sec * 1000 + now.tv_nsec / 1000000;
}

void connectivity_matrix_set_topology(uint8_t matrix[MAX_NODES][MAX_NODES],
                                       node_id_t *node_ids,
                                       uint8_t num_nodes) {
    pthread_mutex_lock(&global_topology.lock);
    
    memcpy(global_topology.matrix, matrix, sizeof(global_topology.matrix));
    memcpy(global_topology.node_ids, node_ids, num_nodes * sizeof(node_id_t));
    global_topology.num_nodes = num_nodes;
    
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    global_topology.timestamp = now.tv_sec * 1000 + now.tv_nsec / 1000000;
    
    pthread_mutex_unlock(&global_topology.lock);
    
    printf("[TOPOLOGY] Updated: %d nodes\n", num_nodes);
}

void connectivity_matrix_get(connectivity_matrix_t *output) {
    pthread_mutex_lock(&global_topology.lock);
    memcpy(output, &global_topology, sizeof(connectivity_matrix_t));
    pthread_mutex_unlock(&global_topology.lock);
}

void connectivity_matrix_print(void) {
    pthread_mutex_lock(&global_topology.lock);
    
    printf("\n=== Connectivity Matrix ===\n");
    printf("Nodes: %d [", global_topology.num_nodes);
    for (int i = 0; i < global_topology.num_nodes; i++) {
        printf("%d%s", global_topology.node_ids[i], 
               i < global_topology.num_nodes - 1 ? ", " : "");
    }
    printf("]\n\n");
    
    printf("    ");
    for (int i = 0; i < global_topology.num_nodes; i++) {
        printf("%3d ", global_topology.node_ids[i]);
    }
    printf("\n");
    
    for (int i = 0; i < global_topology.num_nodes; i++) {
        printf("%3d ", global_topology.node_ids[i]);
        for (int j = 0; j < global_topology.num_nodes; j++) {
            printf("%3d ", global_topology.matrix[i][j]);
        }
        printf("\n");
    }
    printf("\n");
    
    pthread_mutex_unlock(&global_topology.lock);
}