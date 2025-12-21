#include "network/tdma_node.h"
#include "connectivity_matrix.h" 
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

#define TIMEOUT_MS 5000 

uint64_t current_time_ms() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

// ========================================
// Inicialização
// ========================================

int tdma_node_init(tdma_node_t *node, node_id_t my_id, 
                   int total_nodes, routing_strategy_t strategy) {
    memset(node, 0, sizeof(tdma_node_t));
    
    node->my_id = my_id;
    node->total_nodes = total_nodes;
    node->state = NODE_STATE_INIT;
    node->heartbeat_interval_ms = TDMA_ROUND_PERIOD_MS;
    node->running = false;
    
    uint64_t now = current_time_ms();
    for(int i = 0; i < MAX_NODES; i++) {
        node->last_seen_ms[i] = now;
    }
    
    printf("[NODE %d] Initializing...\n", my_id);
    
    if (udp_transport_init(&node->transport, my_id) < 0) {
        fprintf(stderr, "[NODE %d] Failed to init transport\n", my_id);
        return -1;
    }
    
    connectivity_matrix_init();
    routing_manager_init(&node->routing_mgr, my_id, strategy);
    
    node_id_t all_nodes[MAX_NODES];
    for (int i = 0; i < total_nodes; i++) {
        all_nodes[i] = i + 1;
    }
    
    if (ra_tdmas_init(&node->ra_sync, my_id, all_nodes, total_nodes) < 0) {
        fprintf(stderr, "[NODE %d] Failed to init RA-TDMAs+\n", my_id);
        return -1;
    }
    
    for (int i = 0; i < total_nodes; i++) {
        node->topology.node_ids[i] = i + 1;
        if (abs((int)my_id - (i+1)) == 1) {
            node->topology.matrix[my_id-1][i] = 1;
            node->topology.matrix[i][my_id-1] = 1;
        }
    }
    node->topology.num_nodes = total_nodes;
    
    spanning_tree_t mst;
    spanning_tree_compute(&node->topology, &mst);
    ra_tdmas_set_spanning_tree(&node->ra_sync, &mst);
    
    routing_manager_update_topology(&node->routing_mgr, &node->topology);
    
    printf("[NODE %d] Initialized successfully\n", my_id);
    return 0;
}

// ========================================
// Threads
// ========================================

void* tdma_node_heartbeat_thread(void *arg) {
    tdma_node_t *node = (tdma_node_t*)arg;
    printf("[NODE %d] Heartbeat thread started (RA-TDMAs+)\n", node->my_id);
    
    while (node->running) {
        // 1. SCHEDULER: Esperar pelo Slot
        while (!ra_tdmas_can_transmit(&node->ra_sync) && node->running) {
            usleep(100); 
        }
        
        if (!node->running) break;
        
        // --- DEBUG CRÍTICO: Descomenta para ver o slot a abrir ---
        printf("[TDMA %d] 🟢 Slot OPEN (Round %u). Sending...\n", 
              node->my_id, node->ra_sync.round_number);
        // ---------------------------------------------------------

        // 2. AJUSTAR RELÓGIO
        ra_tdmas_calculate_slot_adjustment(&node->ra_sync);
        
        // 3. ENVIAR
        uint64_t tx_time_us = ra_tdmas_get_current_time_us();
        uint8_t payload = 0xFF;
        
        int sent = udp_transport_broadcast(&node->transport, MSG_HEARTBEAT,
                                          &payload, 1, node->total_nodes, tx_time_us);
        
        if (sent > 0) {
            node->heartbeats_sent++;
            node->packets_sent_in_slot++;
        }
        
        // 4. DORMIR ATÉ AO FIM DA RONDA
        uint32_t wait_us = ra_tdmas_time_until_my_slot_us(&node->ra_sync);
        usleep(wait_us);
        
        ra_tdmas_on_round_end(&node->ra_sync);
        node->packets_sent_in_slot = 0;
    }
    
    printf("[NODE %d] Heartbeat thread stopped\n", node->my_id);
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
        
        if (len > 0) {
            uint64_t rx_time_us = ra_tdmas_get_current_time_us();
            tdma_node_process_message(node, &header, payload);
            
            ra_tdmas_on_packet_received(&node->ra_sync, header.src,
                                       header.tx_timestamp_us, rx_time_us);
            
            if (header.src > 0 && header.src <= MAX_NODES) {
                node->last_seen_ms[header.src - 1] = current_time_ms();
            }
            
        } else if (len == 0) {
            usleep(1000);
        }
    }
    return NULL;
}

void tdma_node_process_message(tdma_node_t *node, udp_header_t *header, void *payload) {
    (void)payload;
    switch (header->type) {
        case MSG_HEARTBEAT: node->heartbeats_received++; break;
        case MSG_TOPOLOGY_UPDATE: node->topology_updates++; break;
        default: break;
    }
}

void tdma_node_update_connectivity(tdma_node_t *node, node_id_t neighbor, bool is_alive) {
    int my_idx = node->my_id - 1;
    int neighbor_idx = neighbor - 1;
    if (neighbor_idx < 0 || neighbor_idx >= node->total_nodes) return;
    
    uint8_t old = node->topology.matrix[my_idx][neighbor_idx];
    uint8_t new_val = is_alive ? 1 : 0;
    
    if (old != new_val) {
        printf("[NODE %d] Link to node %d changed: %d -> %d\n", node->my_id, neighbor, old, new_val);
        node->topology.matrix[my_idx][neighbor_idx] = new_val;
        node->topology.matrix[neighbor_idx][my_idx] = new_val;
        
        connectivity_matrix_set_topology(node->topology.matrix, node->topology.node_ids, node->topology.num_nodes);
        spanning_tree_t mst;
        spanning_tree_compute(&node->topology, &mst);
        ra_tdmas_set_spanning_tree(&node->ra_sync, &mst);
        connectivity_matrix_get(&node->topology);
        routing_manager_update_topology(&node->routing_mgr, &node->topology);
    }
}

void tdma_node_check_timeouts(tdma_node_t *node) {
    uint64_t now = current_time_ms();
    for (int i = 0; i < node->total_nodes; i++) {
        node_id_t neighbor = i + 1;
        if (neighbor == node->my_id) continue;
        
        uint64_t last = node->last_seen_ms[i];
        if (now - last > TIMEOUT_MS && node->topology.matrix[node->my_id-1][i]) {
            printf("[NODE %d] TIMEOUT Node %d\n", node->my_id, neighbor);
            tdma_node_update_connectivity(node, neighbor, false);
        } else if (now - last <= TIMEOUT_MS && !node->topology.matrix[node->my_id-1][i]) {
            printf("[NODE %d] RECOVERED Node %d\n", node->my_id, neighbor);
            tdma_node_update_connectivity(node, neighbor, true);
        }
    }
}

int tdma_node_start(tdma_node_t *node) {
    printf("[NODE %d] Starting...\n", node->my_id);
    node->running = true;
    node->state = NODE_STATE_DISCOVERING;
    pthread_create(&node->heartbeat_thread, NULL, tdma_node_heartbeat_thread, node);
    pthread_create(&node->receiver_thread, NULL, tdma_node_receiver_thread, node);
    sleep(2);
    node->state = NODE_STATE_RUNNING;
    printf("[NODE %d] Running!\n", node->my_id);
    return 0;
}

void tdma_node_stop(tdma_node_t *node) {
    node->running = false;
    pthread_join(node->heartbeat_thread, NULL);
    pthread_join(node->receiver_thread, NULL);
}

void tdma_node_print_status(tdma_node_t *node) {
    printf("Node %d: Sent %lu, Recv %lu, Shift %ld us\n", 
           node->my_id, node->heartbeats_sent, node->heartbeats_received, 
           node->ra_sync.total_shift_applied_us);
    ra_tdmas_print_slot_boundaries(&node->ra_sync);
    routing_manager_print_table(&node->routing_mgr);
}

void tdma_node_destroy(tdma_node_t *node) {
    udp_transport_destroy(&node->transport);
    routing_manager_destroy(&node->routing_mgr);
}
