// include/tdma_types.h
#ifndef TDMA_TYPES_H
#define TDMA_TYPES_H

#include <stdint.h>
#include <stdbool.h>
#include <pthread.h>

#define MAX_NODES 20
#define MAC_BYTES 6

// Node ID type
typedef uint8_t node_id_t;

// MAC address
typedef struct {
    uint8_t bytes[MAC_BYTES];
} mac_addr_t;

// Network topology (connectivity matrix)
typedef struct {
    uint8_t matrix[MAX_NODES][MAX_NODES];  // Binary: 1=connected, 0=not
    node_id_t node_ids[MAX_NODES];         // Active node IDs
    uint8_t num_nodes;                     // Number of active nodes
    uint64_t timestamp;                    // Last update time
    pthread_mutex_t lock;
} connectivity_matrix_t;

// Spanning tree (for synchronization)
typedef struct {
    uint8_t tree[MAX_NODES][MAX_NODES];    // MST representation
    node_id_t node_ids[MAX_NODES];
    uint8_t num_nodes;
    pthread_mutex_t lock;
} spanning_tree_t;

// Route entry
typedef struct {
    node_id_t destination;
    node_id_t next_hop;
    mac_addr_t next_hop_mac;
    uint8_t hop_count;
    bool valid;
    uint64_t timestamp;
} route_entry_t;

// Routing table
typedef struct {
    route_entry_t routes[MAX_NODES];
    pthread_mutex_t lock;
} routing_table_t;

#endif