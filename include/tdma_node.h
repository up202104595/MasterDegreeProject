// include/tdma_node.h
#ifndef TDMA_NODE_H
#define TDMA_NODE_H

#include "tdma_types.h"
#include "udp_transport.h"
#include "ra_tdmas_sync.h"
#include "routing_manager.h"
#include "ip_routing_manager.h"
#include "data_streaming.h"
#include "tx_queue.h"  // ✅ IMPORTANTE
#include <pthread.h>
#include <stdbool.h>

#define NODE_TIMEOUT_ROUNDS 5  // ✅ NOVO: Timeout após 5 rounds sem heartbeat

typedef enum {
    NODE_STATE_INIT,
    NODE_STATE_DISCOVERING,
    NODE_STATE_RUNNING,
    NODE_STATE_SHUTDOWN
} node_state_t;

typedef struct {
    // Identity
    node_id_t my_id;
    int total_nodes;
    node_state_t state;
    bool running;
    
    // Networking
    udp_transport_t transport;
    connectivity_matrix_t topology;
    
    // Routing
    routing_manager_t routing_mgr;
    ip_routing_manager_t ip_routing_mgr;
    
    // Synchronization
    ra_tdmas_sync_t ra_sync;
    
    // Data streaming
    data_streaming_t streaming;
    
    // ✅ NOVO: TX Queue para relay
    tx_queue_t tx_queue;
    
    // Threads
    pthread_t heartbeat_thread;
    pthread_t receiver_thread;
    
    // Heartbeat tracking
    uint32_t heartbeat_interval_ms;
    uint64_t heartbeats_sent;
    uint64_t heartbeats_received;
    uint64_t topology_updates;
    
    // ✅ NOVO: Age-based timeout (em vez de timestamp)
    uint32_t node_age[MAX_NODES];
    
    // Slot management
    uint32_t packets_sent_in_slot;
    
} tdma_node_t;

// Node lifecycle
int tdma_node_init(tdma_node_t *node, node_id_t my_id,
                   int total_nodes, routing_strategy_t strategy);
int tdma_node_start(tdma_node_t *node);
void tdma_node_stop(tdma_node_t *node);
void tdma_node_destroy(tdma_node_t *node);

// Message processing
void tdma_node_process_message(tdma_node_t *node,
                              udp_header_t *header,
                              void *payload,
                              int payload_len);

// Topology management
void tdma_node_update_connectivity(tdma_node_t *node,
                                  node_id_t neighbor,
                                  bool is_alive);
void tdma_node_check_timeouts(tdma_node_t *node);

// Status
void tdma_node_print_status(tdma_node_t *node);

// Threads
void* tdma_node_heartbeat_thread(void *arg);
void* tdma_node_receiver_thread(void *arg);

#endif