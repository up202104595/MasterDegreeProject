// src/network/tdma_node.c
#include "tdma_node.h"
#include "connectivity_matrix.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

#define TIMEOUT_MS 5000  // 5 segundos sem heartbeat = nó morto

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
    
    // Init transport
    if (udp_transport_init(&node->transport, my_id) < 0) {
        fprintf(stderr, "[NODE %d] Failed to init transport\n", my_id);
        return -1;
    }
    
    // Init connectivity matrix
    connectivity_matrix_init();
    
    // Init routing manager
    routing_manager_init(&node->routing_mgr, my_id, strategy);
    
    // RA-TDMAs+ Init
    node_id_t all_nodes[MAX_NODES];
    for (int i = 0; i < total_nodes; i++) {
        all_nodes[i] = i + 1;
    }
    
    if (ra_tdmas_init(&node->ra_sync, my_id, all_nodes, total_nodes) < 0) {
        fprintf(stderr, "[NODE %d] Failed to init RA-TDMAs+\n", my_id);
        return -1;
    }
    
    // Initial topology (assume linear for bootstrap)
    for (int i = 0; i < total_nodes; i++) {
        node->topology.node_ids[i] = i + 1;
        // Connect neighbors (linear topology initially)
        if (abs((int)my_id - (i+1)) == 1) {
            node->topology.matrix[my_id-1][i] = 1;
            node->topology.matrix[i][my_id-1] = 1;
        }
    }
    node->topology.num_nodes = total_nodes;
    
    // Compute MST and set it for RA-TDMAs+
    spanning_tree_t mst;
    spanning_tree_compute(&node->topology, &mst);
    ra_tdmas_set_spanning_tree(&node->ra_sync, &mst);
    
    // Update routing
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
        // Wait for my slot
        while (!ra_tdmas_can_transmit(&node->ra_sync) && node->running) {
            usleep(100);  // Check every 100μs
        }
        
        if (!node->running) break;
        
        // Calculate slot adjustment BEFORE transmitting
        ra_tdmas_calculate_slot_adjustment(&node->ra_sync);
        
        // Send heartbeat with timestamp
        uint64_t tx_time_us = ra_tdmas_get_current_time_us();
        uint8_t payload = 0xFF;
        
        int sent = udp_transport_broadcast(&node->transport, MSG_HEARTBEAT,
                                          &payload, 1, node->total_nodes, tx_time_us);
        
        if (sent > 0) {
            node->heartbeats_sent++;
            node->packets_sent_in_slot++;
        }
        
        // Wait for round end
        uint32_t wait_us = ra_tdmas_time_until_my_slot_us(&node->ra_sync);
        usleep(wait_us);
        
        ra_tdmas_on_round_end(&node->ra_sync);
        node->packets_sent_in_slot = 0;  // Reset counter
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
            
            // Process message
            tdma_node_process_message(node, &header, payload);
            
            // Update RA-TDMAs+ delays
            ra_tdmas_on_packet_received(&node->ra_sync, header.src,
                                       header.tx_timestamp_us, rx_time_us);
            
            // Update last seen
            if (header.src > 0 && header.src <= MAX_NODES) {
                node->last_seen_ms[header.src - 1] = current_time_ms();
            }
            
        } else if (len == 0) {
            usleep(1000);  // No data, sleep 1ms
        }
    }
    
    printf("[NODE %d] Receiver thread stopped\n", node->my_id);
    return NULL;
}

// ========================================
// Message Processing
// ========================================

void tdma_node_process_message(tdma_node_t *node, 
                              udp_header_t *header,
                              void *payload) {
    (void)payload;  // Unused for now
    
    switch (header->type) {
        case MSG_HEARTBEAT:
            node->heartbeats_received++;
            break;
            
        case MSG_TOPOLOGY_UPDATE:
            node->topology_updates++;
            break;
            
        case MSG_DATA:
            // Handle data packets
            break;
            
        default:
            break;
    }
}

void tdma_node_update_connectivity(tdma_node_t *node, 
                                  node_id_t neighbor,
                                  bool is_alive) {
    int my_idx = node->my_id - 1;
    int neighbor_idx = neighbor - 1;
    
    if (neighbor_idx < 0 || neighbor_idx >= node->total_nodes) {
        return;
    }
    
    uint8_t old_value = node->topology.matrix[my_idx][neighbor_idx];
    uint8_t new_value = is_alive ? 1 : 0;
    
    if (old_value != new_value) {
        printf("[NODE %d] Link to node %d changed: %d → %d\n",
               node->my_id, neighbor, old_value, new_value);
        
        node->topology.matrix[my_idx][neighbor_idx] = new_value;
        node->topology.matrix[neighbor_idx][my_idx] = new_value;
        
        // Update global connectivity matrix
        connectivity_matrix_set_topology(node->topology.matrix, 
                                        node->topology.node_ids,
                                        node->topology.num_nodes);
        
        // Recompute MST
        spanning_tree_t mst;
        spanning_tree_compute(&node->topology, &mst);
        ra_tdmas_set_spanning_tree(&node->ra_sync, &mst);
        
        // Update routing
        connectivity_matrix_get(&node->topology);
        routing_manager_update_topology(&node->routing_mgr, &node->topology);
    }
}

// ========================================
// Timeout Detection
// ========================================

void tdma_node_check_timeouts(tdma_node_t *node) {
    uint64_t now = current_time_ms();
    bool topology_changed = false;
    
    for (int i = 0; i < node->total_nodes; i++) {
        node_id_t neighbor = i + 1;
        if (neighbor == node->my_id) continue;
        
        uint64_t last_seen = node->last_seen_ms[i];
        uint64_t elapsed = now - last_seen;
        
        int my_idx = node->my_id - 1;
        bool currently_connected = (node->topology.matrix[my_idx][i] != 0);
        
        if (elapsed > TIMEOUT_MS && currently_connected) {
            printf("[NODE %d] ⚠️  TIMEOUT: Node %d (last seen %lu ms ago)\n",
                   node->my_id, neighbor, elapsed);
            
            tdma_node_update_connectivity(node, neighbor, false);
            topology_changed = true;
        }
        else if (elapsed <= TIMEOUT_MS && !currently_connected) {
            printf("[NODE %d] ✅ RECOVERED: Node %d is back online\n",
                   node->my_id, neighbor);
            
            tdma_node_update_connectivity(node, neighbor, true);
            topology_changed = true;
        }
    }
    
    if (topology_changed) {
        printf("[NODE %d] Topology changed due to timeouts\n", node->my_id);
    }
}

// ========================================
// Control
// ========================================

int tdma_node_start(tdma_node_t *node) {
    printf("[NODE %d] Starting...\n", node->my_id);
    
    node->running = true;
    node->state = NODE_STATE_DISCOVERING;
    
    // Start heartbeat thread
    if (pthread_create(&node->heartbeat_thread, NULL, 
                      tdma_node_heartbeat_thread, node) != 0) {
        perror("pthread_create heartbeat");
        return -1;
    }
    
    // Start receiver thread
    if (pthread_create(&node->receiver_thread, NULL,
                      tdma_node_receiver_thread, node) != 0) {
        perror("pthread_create receiver");
        return -1;
    }
    
    sleep(2);  // Wait for initial discovery
    node->state = NODE_STATE_RUNNING;
    
    printf("[NODE %d] Running!\n", node->my_id);
    return 0;
}

void tdma_node_stop(tdma_node_t *node) {
    printf("[NODE %d] Stopping...\n", node->my_id);
    
    node->running = false;
    node->state = NODE_STATE_SHUTDOWN;
    
    pthread_join(node->heartbeat_thread, NULL);
    pthread_join(node->receiver_thread, NULL);
    
    printf("[NODE %d] Stopped\n", node->my_id);
}

void tdma_node_print_status(tdma_node_t *node) {
    printf("\n");
    printf("╔════════════════════════════════════════════════╗\n");
    printf("║  NODE %d STATUS - RA-TDMAs+ ENABLED            \n", node->my_id);
    printf("╚════════════════════════════════════════════════╝\n\n");
    
    const char *state_str[] = {"INIT", "DISCOVERING", "RUNNING", "SHUTDOWN"};
    printf("State:              %s\n", state_str[node->state]);
    printf("Synchronized:       %s\n", node->ra_sync.is_synchronized ? "YES" : "NO");
    printf("Round:              %u\n", node->ra_sync.round_number);
    printf("Heartbeats sent:    %lu\n", node->heartbeats_sent);
    printf("Heartbeats recv:    %lu\n", node->heartbeats_received);
    printf("Slot adjustments:   %lu\n", node->ra_sync.slot_adjustments);
    printf("Total shift:        %ld μs\n", node->ra_sync.total_shift_applied_us);
    
    // Slot boundaries
    ra_tdmas_print_slot_boundaries(&node->ra_sync);
    
    // Delays
    ra_tdmas_print_delays(&node->ra_sync);
    
    // Routing table
    routing_manager_print_table(&node->routing_mgr);
    
    // Transport stats
    udp_transport_print_stats(&node->transport);
    
    // Performance
    routing_manager_print_performance(&node->routing_mgr);
}

void tdma_node_destroy(tdma_node_t *node) {
    printf("[NODE %d] Destroying...\n", node->my_id);
    
    udp_transport_destroy(&node->transport);
    routing_manager_destroy(&node->routing_mgr);
    
    printf("[NODE %d] Destroyed\n", node->my_id);
}