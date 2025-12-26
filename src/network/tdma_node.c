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
// Network Readiness Check (CORRIGIDO)
// ========================================

static bool check_network_ready(node_id_t my_id, int total_nodes) {
    char my_ip[32];  // ← CORRIGIDO: Buffer aumentado de 16 para 32
    snprintf(my_ip, sizeof(my_ip), "192.168.2.%d", 10 + my_id);
    
    // Cria socket de teste
    int test_sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (test_sock < 0) {
        return false;
    }
    
    // Tenta fazer bind no IP específico
    struct sockaddr_in bind_addr;
    memset(&bind_addr, 0, sizeof(bind_addr));
    bind_addr.sin_family = AF_INET;
    bind_addr.sin_port = 0;  // Porta aleatória
    
    if (inet_pton(AF_INET, my_ip, &bind_addr.sin_addr) <= 0) {
        close(test_sock);
        return false;
    }
    
    if (bind(test_sock, (struct sockaddr*)&bind_addr, sizeof(bind_addr)) < 0) {
        close(test_sock);
        return false;  // Interface ainda não está UP
    }
    
    // Tenta enviar para qualquer outro nó
    for (int target = 1; target <= total_nodes; target++) {
        if (target == my_id) continue;
        
        char dst_ip[32];  // ← CORRIGIDO: Buffer aumentado de 16 para 32
        snprintf(dst_ip, sizeof(dst_ip), "192.168.2.%d", 10 + target);
        
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
            // Conseguiu enviar! Rede está OK
            close(test_sock);
            return true;
        }
        
        if (errno != ENETUNREACH && errno != EHOSTUNREACH) {
            // Outro erro que não seja "network unreachable"
            close(test_sock);
            return true;  // Assume que a rede está OK
        }
    }
    
    close(test_sock);
    return false;  // Não conseguiu enviar para nenhum nó
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

    // ============================================
    // CRITICAL FIX: Wait for network interface
    // ============================================
    printf("[NODE %d] Waiting for network interface (veth%d)...\n", my_id, my_id);
    
    bool net_ready = false;
    for (int attempt = 1; attempt <= 15; attempt++) {  // 15 segundos máximo
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
        fprintf(stderr, "\n");
        fprintf(stderr, "[NODE %d] ❌ FATAL: Network interface failed to initialize!\n", my_id);
        fprintf(stderr, "[NODE %d] This usually means:\n", my_id);
        fprintf(stderr, "  1. Network namespace not created correctly\n");
        fprintf(stderr, "  2. Interface veth%d is not UP\n", my_id);
        fprintf(stderr, "  3. IP 192.168.2.%d not configured\n", 10 + my_id);
        fprintf(stderr, "\n");
        fprintf(stderr, "Debug commands:\n");
        fprintf(stderr, "  sudo ip netns exec node%d ip addr\n", my_id);
        fprintf(stderr, "  sudo ip netns exec node%d ip route\n", my_id);
        fprintf(stderr, "  sudo ip netns exec node%d ping -c 1 192.168.2.1%d\n", 
                my_id, (my_id % total_nodes) + 1);
        fprintf(stderr, "\n");
        return -1;
    }
    // ============================================
    
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
    uint64_t last_timeout_check_ms = current_time_ms();  // ← ADICIONAR
    
    while (node->running) {
        
        // ============================================
        // CHECK TIMEOUTS PERIODICALLY (NOVO!)
        // ============================================
        uint64_t now_ms = current_time_ms();
        if (now_ms - last_timeout_check_ms >= 1000) {  // A cada 1 segundo
            tdma_node_check_timeouts(node);
            last_timeout_check_ms = now_ms;
        }
        // ============================================
        
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
            
            // Process message
            tdma_node_process_message(node, &header, payload, len);
            
            // Update RA-TDMAs+ delays
            ra_tdmas_on_packet_received(&node->ra_sync, header.src,
                                       header.tx_timestamp_us, rx_time_us);
            
            // Update last seen
            if (header.src > 0 && header.src <= MAX_NODES) {
                node->last_seen_ms[header.src - 1] = current_time_ms();
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
            
        case MSG_DATA:
            data_streaming_receive(&node->streaming, 
                                 (uint8_t*)payload, 
                                 payload_len);
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
// Timeout Detection
// ========================================

void tdma_node_check_timeouts(tdma_node_t *node) {
    uint64_t now = current_time_ms();
    
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
        }
        else if (elapsed <= TIMEOUT_MS && !currently_connected) {
            printf("[NODE %d] ✅ RECOVERED: Node %d\n",
                   node->my_id, neighbor);
            
            tdma_node_update_connectivity(node, neighbor, true);
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
    
    routing_manager_print_table(&node->routing_mgr);
    udp_transport_print_stats(&node->transport);
    routing_manager_print_performance(&node->routing_mgr);
}

void tdma_node_destroy(tdma_node_t *node) {
    printf("[NODE %d] Destroying...\n", node->my_id);
    
    ip_routing_manager_destroy(&node->ip_routing_mgr);
    udp_transport_destroy(&node->transport);
    routing_manager_destroy(&node->routing_mgr);
    
    printf("[NODE %d] Destroyed\n", node->my_id);
}