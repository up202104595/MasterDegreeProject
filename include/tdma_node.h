// include/tdma_node.h
#ifndef TDMA_NODE_H
#define TDMA_NODE_H

#include <stdint.h>
#include <stdbool.h>
#include <pthread.h>
#include "connectivity_matrix.h"
#include "routing_manager.h"
#include "ip_routing_manager.h"
#include "data_streaming.h"
#include "udp_transport.h"
#include "ra_tdmas_sync.h"

typedef enum {
    NODE_STATE_INIT = 0,
    NODE_STATE_DISCOVERING,
    NODE_STATE_RUNNING,
    NODE_STATE_SHUTDOWN
} node_state_t;

typedef struct {
    // Identity
    node_id_t my_id;
    int total_nodes;
    node_state_t state;
    
    // Topology
    connectivity_matrix_t topology;
    
    // Routing
    routing_manager_t routing_mgr;
    ip_routing_manager_t ip_routing_mgr;
    
    // Streaming
    data_streaming_t streaming;
    
    // Transport
    udp_transport_t transport;
    
    // RA-TDMAs+ Sync
    ra_tdmas_sync_t ra_sync;
    
    // Threads
    pthread_t heartbeat_thread;
    pthread_t receiver_thread;
    bool running;
    
    // Timing
    uint32_t heartbeat_interval_ms;
    uint64_t last_seen_ms[MAX_NODES];
    
    // Stats
    uint64_t heartbeats_sent;
    uint64_t heartbeats_received;
    uint64_t topology_updates;
    uint32_t packets_sent_in_slot;
    
} tdma_node_t;

// Lifecycle
int tdma_node_init(tdma_node_t *node, node_id_t my_id,
                   int total_nodes, routing_strategy_t strategy);
int tdma_node_start(tdma_node_t *node);
void tdma_node_stop(tdma_node_t *node);
void tdma_node_destroy(tdma_node_t *node);

// Operations
void tdma_node_process_message(tdma_node_t *node,
                              udp_header_t *header,
                              void *payload,
                              int payload_len);  // ← CORRIGIDO
void tdma_node_update_connectivity(tdma_node_t *node,
                                  node_id_t neighbor,
                                  bool is_alive);
void tdma_node_check_timeouts(tdma_node_t *node);

// Status
void tdma_node_print_status(tdma_node_t *node);

// Threads
void* tdma_node_heartbeat_thread(void *arg);
void* tdma_node_receiver_thread(void *arg);

#endif // TDMA_NODE_H