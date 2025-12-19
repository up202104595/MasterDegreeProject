#ifndef TDMA_NODE_H
#define TDMA_NODE_H

#include <stdint.h>
#include <stdbool.h>
#include <pthread.h>
#include <time.h>           // <--- IMPORTANTE PARA O TEMPO
#include "tdma_types.h"
#include "routing_manager.h"
#include "udp_transport.h"

// Estado do nó
typedef enum {
    NODE_STATE_INIT,
    NODE_STATE_DISCOVERING,
    NODE_STATE_RUNNING,
    NODE_STATE_SHUTDOWN
} node_state_t;

// Estrutura principal do nó TDMA
typedef struct {
    node_id_t my_id;
    node_state_t state;
    
    // Componentes
    routing_manager_t routing_mgr;
    udp_transport_t transport;
    connectivity_matrix_t topology;
    
    // Monitoramento de Links (NOVO)
    uint64_t last_seen_ms[MAX_NODES]; // <--- FALTOU ISTO

    // Threads
    pthread_t heartbeat_thread;
    pthread_t receiver_thread;
    bool running;
    
    // Configuração
    int total_nodes;
    uint32_t heartbeat_interval_ms;
    
    // Estatísticas
    uint64_t heartbeats_sent;
    uint64_t heartbeats_received;
    uint64_t topology_updates;
    
} tdma_node_t;

// ========================================
// API do Nó TDMA
// ========================================

int tdma_node_init(tdma_node_t *node, 
                   node_id_t my_id, 
                   int total_nodes,
                   routing_strategy_t strategy);

int tdma_node_start(tdma_node_t *node);
void tdma_node_stop(tdma_node_t *node);
void tdma_node_print_status(tdma_node_t *node);
void tdma_node_destroy(tdma_node_t *node);

// A FUNÇÃO QUE FALTAVA PARA O MAIN.C FUNCIONAR:
void tdma_node_check_timeouts(tdma_node_t *node); 

#endif // TDMA_NODE_H