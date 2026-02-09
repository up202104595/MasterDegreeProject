// src/network/tdma_node.c
#include "tdma_node.h"
#include "connectivity_matrix.h"
#include "ip_routing_manager.h"
#include "data_streaming.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <math.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define TIMEOUT_MS 5000
#define INITIAL_SETTLE_TIME_SEC 10

uint64_t current_time_ms() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

// ========================================
// Network Readiness Check (CORRIGIDO - SEM +10)
// ========================================

static bool check_network_ready(node_id_t my_id, int total_nodes) {
    char my_ip[32];
    snprintf(my_ip, sizeof(my_ip), "192.168.2.%d", my_id);  // ✅ SEM +10!
    
    int test_sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (test_sock < 0) {
        return false;
    }
    
    struct sockaddr_in bind_addr;
    memset(&bind_addr, 0, sizeof(bind_addr));
    bind_addr.sin_family = AF_INET;
    bind_addr.sin_port = 0;
    
    if (inet_pton(AF_INET, my_ip, &bind_addr.sin_addr) <= 0) {
        close(test_sock);
        return false;
    }
    
    if (bind(test_sock, (struct sockaddr*)&bind_addr, sizeof(bind_addr)) < 0) {
        close(test_sock);
        return false;
    }
    
    for (int target = 1; target <= total_nodes; target++) {
        if (target == my_id) continue;
        
        char dst_ip[32];
        snprintf(dst_ip, sizeof(dst_ip), "192.168.2.%d", target);  // ✅ SEM +10!
        
        struct sockaddr_in dst_addr;
        memset(&dst_addr, 0, sizeof(dst_addr));
        dst_addr.sin_family = AF_INET;
        dst_addr.sin_port = htons(9999);
        
        if (inet_pton(AF_INET, dst_ip, &dst_addr.sin_addr) <= 0) {
            continue;
        }
        
        char test_msg[] = "test";
        ssize_t sent = sendto(test_sock, test_msg, sizeof(test_msg), 0,
                            (struct sockaddr*)&dst_addr, sizeof(dst_addr));
        
        if (sent > 0) {
            close(test_sock);
            return true;
        }
        
        if (errno != ENETUNREACH && errno != EHOSTUNREACH) {
            close(test_sock);
            return true;
        }
    }
    
    close(test_sock);
    return false;
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
    
    // ✅ NOVO! Age-based timeout
    for(int i = 0; i < MAX_NODES; i++) {
        node->node_age[i] = 0;
    }
    
    printf("[NODE %d] Initializing...\n", my_id);

    printf("[NODE %d] Waiting for network interface (veth%d)...\n", my_id, my_id);
    
    bool net_ready = false;
    for (int attempt = 1; attempt <= 15; attempt++) {
        if (check_network_ready(my_id, total_nodes)) {
            net_ready = true;
            printf("[NODE %d] ✅ Network is ready (attempt %d)\n", my_id, attempt);
            break;
        }
        
        if (attempt == 1) {
            printf("[NODE %d] Network not ready yet, waiting...\n", my_id);
        }
        
        if (attempt % 3 == 0) {
            printf("[NODE %d] Still waiting... (%d/15)\n", my_id, attempt);
        }
        
        sleep(1);
    }

    if (!net_ready) {
        fprintf(stderr, "[NODE %d] ❌ FATAL: Network interface failed!\n", my_id);
        return -1;
    }
    
    // Init transport
    if (udp_transport_init(&node->transport, my_id) < 0) {
        fprintf(stderr, "[NODE %d] Failed to init transport\n", my_id);
        return -1;
    }
    
    // Init connectivity matrix
    connectivity_matrix_init();
    
    // Init routing manager
    routing_manager_init(&node->routing_mgr, my_id, strategy);
    
    // Init IP Routing Manager
    char interface[16];
    snprintf(interface, sizeof(interface), "veth%d", my_id);
    
    if (ip_routing_manager_init(&node->ip_routing_mgr, my_id, 
                                interface, total_nodes) < 0) {
        fprintf(stderr, "[NODE %d] ERROR: IP routing init failed\n", my_id);
        return -1;
    }
    
    // Init Data Streaming
    if (data_streaming_init(&node->streaming, my_id, &node->transport) < 0) {
        fprintf(stderr, "[NODE %d] ERROR: Streaming init failed\n", my_id);
        return -1;
    }
    
    // ✅ NOVO! Init TX Queue
    tx_queue_init(&node->tx_queue);
    
    // RA-TDMAs+ Init
    node_id_t all_nodes[MAX_NODES];
    for (int i = 0; i < total_nodes; i++) {
        all_nodes[i] = i + 1;
    }
    
    if (ra_tdmas_init(&node->ra_sync, my_id, all_nodes, total_nodes) < 0) {
        fprintf(stderr, "[NODE %d] Failed to init RA-TDMAs+\n", my_id);
        return -1;
    }
    
    // Initial topology (FULL MESH)
    for (int i = 0; i < total_nodes; i++) {
        node->topology.node_ids[i] = i + 1;
        
        for (int j = 0; j < total_nodes; j++) {
            if (i != j) {
                node->topology.matrix[i][j] = 1;
            } else {
                node->topology.matrix[i][j] = 0;
            }
        }
    }
    node->topology.num_nodes = total_nodes;
    
    printf("[NODE %d] Initial topology: FULL MESH\n", my_id);
    
    // Compute MST
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
    printf("[NODE %d] Heartbeat thread started\n", node->my_id);
    
    uint64_t last_routing_version = 0;
    
    while (node->running) {
        
        // ✅ NOVO! Age-based timeout check
        tdma_node_check_timeouts(node);
        
        // Update IP routing when topology changes
        if (node->state == NODE_STATE_RUNNING) {
            uint64_t current_version = node->routing_mgr.topology_version;
            
            if (current_version != last_routing_version) {
                printf("[NODE %d] 🔄 Routing changed (v%lu → v%lu)\n",
                       node->my_id, last_routing_version, current_version);
                
                ip_routing_manager_update_from_routing(&node->ip_routing_mgr,
                                                      &node->routing_mgr);
                last_routing_version = current_version;
            }
        }
        
        // Wait for my slot
        while (!ra_tdmas_can_transmit(&node->ra_sync) && node->running) {
            usleep(100);
        }
        
        if (!node->running) break;
        
        // Calculate slot adjustment
        ra_tdmas_calculate_slot_adjustment(&node->ra_sync);
        
        // Send heartbeat
        uint64_t tx_time_us = ra_tdmas_get_current_time_us();
        uint8_t payload = 0xFF;
        
        int sent = udp_transport_broadcast(&node->transport, MSG_HEARTBEAT,
                                          &payload, 1, node->total_nodes, 
                                          tx_time_us);
        
        if (sent > 0) {
            node->heartbeats_sent++;
            node->packets_sent_in_slot++;
        }
        
        // ✅ NOVO! Send data from TX queue (with lookahead)
        while (node->running && !tx_queue_is_empty(&node->tx_queue)) {
            uint64_t now_us = ra_tdmas_get_current_time_us();
            uint64_t slot_end_us = now_us + ra_tdmas_time_until_my_slot_us(&node->ra_sync);
            
            // Lookahead: enough time for packet?
            if ((now_us + 2000) > slot_end_us) {
                break;  // No time left
            }
            
            uint8_t data_payload[MAX_PACKET_SIZE];
            uint32_t dst_ip;
            int data_len = tx_queue_dequeue(&node->tx_queue, &dst_ip,
                                          data_payload, sizeof(data_payload));
            
            if (data_len <= 0) break;
            
            udp_transport_send(&node->transport, dst_ip, MSG_DATA,
                             data_payload, data_len, ra_tdmas_get_current_time_us());
        }
        
        // Wait for round end
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
            
            tdma_node_process_message(node, &header, payload, len);
            
            ra_tdmas_on_packet_received(&node->ra_sync, header.src,
                                       header.tx_timestamp_us, rx_time_us);
            
            // ✅ NOVO! Reset age
            if (header.src > 0 && header.src <= MAX_NODES) {
                node->node_age[header.src - 1] = 0;
            }
            
        } else if (len == 0) {
            usleep(1000);
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
                              void *payload,
                              int payload_len) {
    
    switch (header->type) {
        case MSG_HEARTBEAT:
            node->heartbeats_received++;
            break;
            
        case MSG_TOPOLOGY_UPDATE:
            node->topology_updates++;
            break;
            
        case MSG_DATA: {
            // ============================================
            // MULTI-HOP RELAY LOGIC
            // ============================================
            
            if (payload_len < sizeof(stream_header_t)) {
                fprintf(stderr, "[NODE %d] Invalid data packet size: %d bytes\n", 
                       node->my_id, payload_len);
                break;
            }
            
            stream_header_t *shdr = (stream_header_t*)payload;
            node_id_t final_destination = shdr->destination;
            node_id_t source = shdr->source;
            
            if (final_destination == node->my_id) {
                // ✅ É PARA MIM - Entregar à aplicação
                printf("[NODE %d] 📥 Receiving data from Node %d (stream %u, seq %u/%u)\n",
                       node->my_id, source, shdr->stream_id, 
                       shdr->sequence_number + 1, shdr->total_chunks);
                
                data_streaming_receive(&node->streaming, 
                                     (uint8_t*)payload, 
                                     payload_len);
            } else {
                // 🔄 NÃO É PARA MIM - RELAY!
                printf("[NODE %d] 🔄 Relaying: src=%d → dst=%d (stream %u, seq %u/%u)\n",
                       node->my_id, source, final_destination,
                       shdr->stream_id, shdr->sequence_number + 1, shdr->total_chunks);
                
                // Obter next hop da routing table
                node_id_t next_hop = routing_manager_get_next_hop(
                    &node->routing_mgr, final_destination);
                
                if (next_hop == 255) {
                    printf("[NODE %d] ⚠️  No route to destination %d, dropping packet\n",
                           node->my_id, final_destination);
                    break;
                }
                
                // Calcular IP do next hop
                uint32_t next_hop_ip = 0xC0A80200 | next_hop;  // 192.168.2.X
                
                printf("[NODE %d] 📤 Forwarding to next_hop=%d (192.168.2.%d)\n",
                       node->my_id, next_hop, next_hop);
                
                // Enfileira para transmissão no próximo slot TDMA
                bool queued = tx_queue_enqueue(&node->tx_queue, next_hop_ip,
                                              (uint8_t*)payload, payload_len);
                
                if (queued) {
                    node->streaming.packets_relayed++;
                    
                    if ((shdr->sequence_number + 1) % 10 == 0) {
                        printf("[NODE %d] 📊 Relayed %u packets total\n",
                               node->my_id, node->streaming.packets_relayed);
                    }
                } else {
                    printf("[NODE %d] ⚠️  TX queue full, packet dropped!\n", 
                           node->my_id);
                }
            }
            
            break;
        }
            
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
        
        connectivity_matrix_set_topology(node->topology.matrix,
                                        node->topology.node_ids,
                                        node->topology.num_nodes);
        
        spanning_tree_t mst;
        spanning_tree_compute(&node->topology, &mst);
        ra_tdmas_set_spanning_tree(&node->ra_sync, &mst);
        
        connectivity_matrix_get(&node->topology);
        routing_manager_update_topology(&node->routing_mgr, &node->topology);
        
        ip_routing_manager_update_from_routing(&node->ip_routing_mgr,
                                              &node->routing_mgr);
    }
}

// ========================================
// ✅ NOVO! Age-Based Timeout
// ========================================

void tdma_node_check_timeouts(tdma_node_t *node) {
    for (int i = 0; i < node->total_nodes; i++) {
        node_id_t neighbor = i + 1;
        if (neighbor == node->my_id) continue;
        
        node->node_age[i]++;
        
        if (node->node_age[i] > NODE_TIMEOUT_ROUNDS) {
            int my_idx = node->my_id - 1;
            
            if (node->topology.matrix[my_idx][i] == 1) {
                printf("[NODE %d] ⏱️  TIMEOUT: Node %d (age=%d rounds)\n",
                       node->my_id, neighbor, node->node_age[i]);
                
                tdma_node_update_connectivity(node, neighbor, false);
            }
        }
    }
}

// ========================================
// Control
// ========================================

int tdma_node_start(tdma_node_t *node) {
    printf("[NODE %d] Starting...\n", node->my_id);
    
    node->running = true;
    node->state = NODE_STATE_DISCOVERING;
    
    if (pthread_create(&node->heartbeat_thread, NULL,
                      tdma_node_heartbeat_thread, node) != 0) {
        perror("pthread_create heartbeat");
        return -1;
    }
    
    if (pthread_create(&node->receiver_thread, NULL,
                      tdma_node_receiver_thread, node) != 0) {
        perror("pthread_create receiver");
        return -1;
    }
    
    printf("[NODE %d] Topology discovery (%d seconds)...\n",
           node->my_id, INITIAL_SETTLE_TIME_SEC);
    
    int steps = INITIAL_SETTLE_TIME_SEC * 10;
    for (int i = 0; i < steps; i++) {
        usleep(100000);
        tdma_node_check_timeouts(node);
    }
    
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
    printf("\n╔════════════════════════════════════════════════╗\n");
    printf("║  NODE %d STATUS                                 \n", node->my_id);
    printf("╚════════════════════════════════════════════════╝\n\n");
    
    const char *state_str[] = {"INIT", "DISCOVERING", "RUNNING", "SHUTDOWN"};
    printf("State:           %s\n", state_str[node->state]);
    printf("Synchronized:    %s\n", 
           node->ra_sync.is_synchronized ? "YES" : "NO");
    printf("Heartbeats sent: %lu\n", node->heartbeats_sent);
    printf("Heartbeats recv: %lu\n", node->heartbeats_received);
    
    // ✅ NOVO! TX Queue stats
    tx_queue_print_stats(&node->tx_queue);
    
    routing_manager_print_table(&node->routing_mgr);
    udp_transport_print_stats(&node->transport);
    routing_manager_print_performance(&node->routing_mgr);
}

void tdma_node_destroy(tdma_node_t *node) {
    printf("[NODE %d] Destroying...\n", node->my_id);
    
    ip_routing_manager_destroy(&node->ip_routing_mgr);
    udp_transport_destroy(&node->transport);
    routing_manager_destroy(&node->routing_mgr);
    tx_queue_destroy(&node->tx_queue);  // ✅ NOVO!
    
    printf("[NODE %d] Destroyed\n", node->my_id);
}