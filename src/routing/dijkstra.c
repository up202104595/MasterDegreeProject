// src/routing/dijkstra.c
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <limits.h>
#include "dijkstra.h"

/**
 * Encontra índice do nó com menor distância não visitado
 */
static int find_min_distance_node(uint8_t distance[MAX_NODES],
                                   bool visited[MAX_NODES],
                                   int num_nodes) {
    uint8_t min = INFINITY_COST;
    int min_idx = -1;
    
    for (int i = 0; i < num_nodes; i++) {
        if (!visited[i] && distance[i] < min) {
            min = distance[i];
            min_idx = i;
        }
    }
    
    return min_idx;
}

/**
 * Constrói path from destination back to source
 */
static node_id_t get_next_hop(int src_idx, int dst_idx, 
                               int previous[MAX_NODES],
                               connectivity_matrix_t *topology) {
    if (dst_idx == src_idx) {
        return topology->node_ids[dst_idx];
    }
    
    // Trace back from destination to source
    int current = dst_idx;
    int prev = previous[current];
    
    // Keep going back until we find the node right after source
    while (prev != src_idx && prev != -1) {
        current = prev;
        prev = previous[current];
    }
    
    if (prev == -1) {
        return 0xFF;  // No path
    }
    
    // 'current' is the first hop after source
    return topology->node_ids[current];
}

int dijkstra_compute(node_id_t src,
                     connectivity_matrix_t *topology,
                     dijkstra_result_t results[MAX_NODES]) {
    
    if (!topology || !results) {
        return -1;
    }
    
    // Find source index in topology
    int src_idx = -1;
    for (int i = 0; i < topology->num_nodes; i++) {
        if (topology->node_ids[i] == src) {
            src_idx = i;
            break;
        }
    }
    
    if (src_idx == -1) {
        printf("[DIJKSTRA] Source node %d not found in topology\n", src);
        return -1;
    }
    
    // Initialize arrays
    uint8_t distance[MAX_NODES];
    int previous[MAX_NODES];
    bool visited[MAX_NODES];
    
    for (int i = 0; i < MAX_NODES; i++) {
        distance[i] = INFINITY_COST;
        previous[i] = -1;
        visited[i] = false;
    }
    
    distance[src_idx] = 0;
    
    // Main Dijkstra loop
    for (int count = 0; count < topology->num_nodes; count++) {
        // Find unvisited node with minimum distance
        int u = find_min_distance_node(distance, visited, topology->num_nodes);
        
        if (u == -1) {
            break;  // No more reachable nodes
        }
        
        visited[u] = true;
        
        // Update distances of neighbors
        for (int v = 0; v < topology->num_nodes; v++) {
            // Check if there's an edge and node not visited
            if (topology->matrix[u][v] && !visited[v]) {
                uint8_t alt = distance[u] + 1;  // hop count = 1
                
                if (alt < distance[v]) {
                    distance[v] = alt;
                    previous[v] = u;
                }
            }
        }
    }
    
    // Build results
    for (int i = 0; i < topology->num_nodes; i++) {
        node_id_t dst = topology->node_ids[i];
        
        results[i].destination = dst;
        results[i].distance = distance[i];
        results[i].reachable = (distance[i] != INFINITY_COST);
        
        if (i == src_idx) {
            results[i].next_hop = dst;  // Self
        } else if (results[i].reachable) {
            results[i].next_hop = get_next_hop(src_idx, i, previous, topology);
        } else {
            results[i].next_hop = 0xFF;  // Unreachable
        }
    }
    
    return 0;
}

void dijkstra_print_results(node_id_t src, 
                           dijkstra_result_t results[MAX_NODES],
                           int num_nodes) {
    printf("\n=== Dijkstra Results (Source: %d) ===\n", src);
    printf("Destination | Next Hop | Distance | Reachable\n");
    printf("------------|----------|----------|----------\n");
    
    for (int i = 0; i < num_nodes; i++) {
        if (results[i].destination == 0) continue;
        
        printf("    %3d     |   %3d    |    %2d    |    %s\n",
               results[i].destination,
               results[i].next_hop,
               results[i].distance,
               results[i].reachable ? "YES" : "NO");
    }
    printf("\n");
}

bool dijkstra_path_changed(dijkstra_result_t *old, dijkstra_result_t *new) {
    if (!old || !new) return false;
    
    return (old->next_hop != new->next_hop) || 
           (old->distance != new->distance) ||
           (old->reachable != new->reachable);
}