// include/tdma_node.h
#ifndef TDMA_NODE_H
#define TDMA_NODE_H

#include <stdint.h>
#include <stdbool.h>
#include <pthread.h>
#include <time.h>
#include "tdma_types.h"
#include "routing_manager.h"
#include "udp_transport.h"
#include "ra_tdmas_sync.h"  // <--- CERTIFICA-TE QUE ESTÁ AQUI!

typedef enum {
    NODE_STATE_INIT,
    NODE_STATE_DISCOVERING,
    NODE_STATE_RUNNING,
    NODE_STATE_SHUTDOWN
} node_state_t;

typedef struct {
    node_id_t my_id;
    node_state_t state;
    
    routing_manager_t routing_mgr;
    udp_transport_t transport;
    connectivity_matrix_t topology;
    ra_tdmas_sync_t ra_sync;  // <--- RA-TDMAs+ sync
    
    uint64_t last_seen_ms[MAX_NODES];
    
    pthread_t heartbeat_thread;
    pthread_t receiver_thread;
    bool running;
    
    int total_nodes;
    uint32_t heartbeat_interval_ms;
    
    uint64_t heartbeats_sent;
    uint64_t heartbeats_received;
    uint64_t topology_updates;
    uint64_t packets_sent_in_slot;  // <--- ADICIONA ISTO!
} tdma_node_t;

// API
int tdma_node_init(tdma_node_t *node, node_id_t my_id, 
                   int total_nodes, routing_strategy_t strategy);
int tdma_node_start(tdma_node_t *node);
void tdma_node_stop(tdma_node_t *node);
void tdma_node_check_timeouts(tdma_node_t *node);
void tdma_node_print_status(tdma_node_t *node);
void tdma_node_destroy(tdma_node_t *node);

// Internal functions (declarações para evitar warnings)
void* tdma_node_heartbeat_thread(void *arg);
void* tdma_node_receiver_thread(void *arg);
void tdma_node_process_message(tdma_node_t *node, 
                              udp_header_t *header,
                              void *payload);
void tdma_node_update_connectivity(tdma_node_t *node, 
                                  node_id_t neighbor,
                                  bool is_alive);

#endif // TDMA_NODE_H