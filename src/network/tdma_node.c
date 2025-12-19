#include "tdma_node.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

// ========================================
// Função Auxiliar de Tempo (NOVO)
// ========================================
uint64_t current_time_ms() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

// ========================================
// Inicialização
// ========================================
int tdma_node_init(tdma_node_t *node, 
                   node_id_t my_id, 
                   int total_nodes,
                   routing_strategy_t strategy) {
    memset(node, 0, sizeof(tdma_node_t));
    
    node->my_id = my_id;
    node->total_nodes = total_nodes;
    node->state = NODE_STATE_INIT;
    node->heartbeat_interval_ms = 1000;
    node->running = false;
    
    // Inicializa tempos (NOVO)
    uint64_t now = current_time_ms();
    for(int i=0; i<MAX_NODES; i++) node->last_seen_ms[i] = now;
    
    printf("[NODE %d] Initializing...\n", my_id);
    
    if (udp_transport_init(&node->transport, my_id) < 0) {
        fprintf(stderr, "[NODE %d] Failed to init transport\n", my_id);
        return -1;
    }
    
    connectivity_matrix_init();
    routing_manager_init(&node->routing_mgr, my_id, strategy);
    
    // Topologia inicial: Assume vizinhos diretos (1-2, 2-3, etc)
    // Isso ajuda a rede a começar conectada antes dos heartbeats
    node_id_t nodes[MAX_NODES];
    for (int i = 0; i < total_nodes; i++) {
        nodes[i] = i + 1;
        // Se for vizinho sequencial, marca como 1
        if (abs((int)my_id - (i+1)) == 1) {
             node->topology.matrix[my_id-1][i] = 1;
             node->topology.matrix[i][my_id-1] = 1;
        }
    }
    node->topology.num_nodes = total_nodes;
    memcpy(node->topology.node_ids, nodes, total_nodes * sizeof(node_id_t));
    
    // Carga inicial
    routing_manager_update_topology(&node->routing_mgr, &node->topology);
    
    printf("[NODE %d] Initialized successfully\n", my_id);
    return 0;
}

// ========================================
// Threads
// ========================================

void* tdma_node_heartbeat_thread(void *arg) {
    tdma_node_t *node = (tdma_node_t*)arg;
    printf("[NODE %d] Heartbeat thread started\n", node->my_id);
    
    while (node->running) {
        uint8_t payload = 0xFF;
        int sent = udp_transport_broadcast(&node->transport, 
                                          MSG_HEARTBEAT,
                                          &payload, 1,
                                          node->total_nodes);
        node->heartbeats_sent++;
        // printf("[NODE %d] Sent heartbeat to %d nodes\n", node->my_id, sent); // Comentei para limpar log
        usleep(node->heartbeat_interval_ms * 1000);
    }
    return NULL;
}

void* tdma_node_receiver_thread(void *arg) {
    tdma_node_t *node = (tdma_node_t*)arg;
    printf("[NODE %d] Receiver thread started\n", node->my_id);
    
    while (node->running) {
        udp_header_t header;
        uint8_t payload[MAX_PACKET_SIZE];
        
        int len = udp_transport_receive(&node->transport, &header, 
                                       payload, sizeof(payload), false);
        
        if (len >= 0) {
            // ATUALIZAÇÃO DO TIMEOUT (IMPORTANTE)
            if (header.src < MAX_NODES) {
                node->last_seen_ms[header.src] = current_time_ms();
            }

            // Processa lógica normal
            if (header.type == MSG_HEARTBEAT) {
                 node->heartbeats_received++;
                 // Se o link estava DOWN, traz de volta para UP
                 int my_idx = node->my_id - 1;
                 int src_idx = header.src - 1;
                 
                 if (node->topology.matrix[my_idx][src_idx] == 0) {
                     printf("[NODE %d] Link recovered: %d is back online!\n", node->my_id, header.src);
                     node->topology.matrix[my_idx][src_idx] = 1;
                     node->topology.matrix[src_idx][my_idx] = 1;
                     
                     // Sincroniza topologia global e recalcula rota
                     connectivity_matrix_set_topology(node->topology.matrix, 
                                                    node->topology.node_ids, 
                                                    node->topology.num_nodes);
                     routing_manager_update_topology(&node->routing_mgr, &node->topology);
                 }
            }
        }
        usleep(1000); 
    }
    return NULL;
}

// ========================================
// Verificação de Timeouts (A FUNÇÃO QUE FALTAVA)
// ========================================
void tdma_node_check_timeouts(tdma_node_t *node) {
    uint64_t now = current_time_ms();
    uint64_t timeout_threshold = 3500; // 3.5 segundos

    for (int i = 1; i <= node->total_nodes; i++) {
        if (i == node->my_id) continue;

        if ((now - node->last_seen_ms[i]) > timeout_threshold) {
            int my_idx = node->my_id - 1;
            int other_idx = i - 1;

            // Se o link ainda está marcado como UP (1), muda para DOWN (0)
            if (node->topology.matrix[my_idx][other_idx] == 1) {
                printf("[NODE %d] ⚠️ TIMEOUT: Node %d disappeared. Link Broken!\n", node->my_id, i);
                
                node->topology.matrix[my_idx][other_idx] = 0;
                node->topology.matrix[other_idx][my_idx] = 0;

                // Atualiza Routing Manager
                connectivity_matrix_set_topology(node->topology.matrix, 
                                               node->topology.node_ids, 
                                               node->topology.num_nodes);
                routing_manager_update_topology(&node->routing_mgr, &node->topology);
                
                routing_manager_print_table(&node->routing_mgr);
            }
        }
    }
}

// ========================================
// Controle do Nó
// ========================================

int tdma_node_start(tdma_node_t *node) {
    node->running = true;
    node->state = NODE_STATE_DISCOVERING;
    
    pthread_create(&node->heartbeat_thread, NULL, tdma_node_heartbeat_thread, node);
    pthread_create(&node->receiver_thread, NULL, tdma_node_receiver_thread, node);
    
    sleep(1);
    node->state = NODE_STATE_RUNNING;
    return 0;
}

void tdma_node_stop(tdma_node_t *node) {
    node->running = false;
    // (Simplificado: em prod usar pthread_join)
    udp_transport_destroy(&node->transport);
    routing_manager_destroy(&node->routing_mgr);
}

void tdma_node_print_status(tdma_node_t *node) {
    // Implementação simples
    routing_manager_print_table(&node->routing_mgr);
}

void tdma_node_destroy(tdma_node_t *node) {
    tdma_node_stop(node);
}