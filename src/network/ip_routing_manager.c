// src/network/ip_routing_manager.c
#include "ip_routing_manager.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>

// ========================================
// Helper Functions
// ========================================

static uint64_t get_current_time_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

void node_id_to_ip_str(node_id_t node_id, char *ip_str) {
    snprintf(ip_str, 16, "192.168.2.%d", 10 + node_id);
}

bool ip_routing_requires_sudo(void) {
    return (geteuid() != 0);
}

// ========================================
// IP Route Manipulation via System Commands
// ========================================

static int execute_route_command(const char *interface,
                                const char *dest_ip,
                                const char *gateway_ip,
                                uint32_t metric,
                                bool is_delete) {
    char cmd[512];
    
    if (is_delete) {
        // Delete route: ip route del <dest> via <gateway> dev <interface>
        snprintf(cmd, sizeof(cmd), 
                "ip route del %s via %s dev %s 2>/dev/null",
                dest_ip, gateway_ip, interface);
    } else {
        // Add route: ip route add <dest> via <gateway> dev <interface> metric <metric>
        snprintf(cmd, sizeof(cmd),
                "ip route add %s via %s dev %s metric %u 2>/dev/null",
                dest_ip, gateway_ip, interface, metric);
    }
    
    int ret = system(cmd);
    
    if (ret != 0 && !is_delete) {
        // Try to replace if add failed (route already exists)
        snprintf(cmd, sizeof(cmd),
                "ip route replace %s via %s dev %s metric %u 2>/dev/null",
                dest_ip, gateway_ip, interface, metric);
        ret = system(cmd);
    }
    
    return ret;
}

static int enable_ip_forwarding(void) {
    return system("echo 1 > /proc/sys/net/ipv4/ip_forward 2>/dev/null");
}

// ========================================
// Route Management
// ========================================

int ip_routing_manager_add_route(ip_routing_manager_t *mgr,
                                 node_id_t destination,
                                 node_id_t gateway,
                                 uint32_t metric) {
    pthread_mutex_lock(&mgr->lock);
    
    if (destination == mgr->my_node_id) {
        pthread_mutex_unlock(&mgr->lock);
        return 0;  // Don't route to self
    }
    
    char dest_ip[16], gateway_ip[16];
    node_id_to_ip_str(destination, dest_ip);
    node_id_to_ip_str(gateway, gateway_ip);
    
    // Find or create entry
    int idx = -1;
    for (int i = 0; i < mgr->num_routes; i++) {
        if (mgr->route_table[i].destination == destination) {
            idx = i;
            break;
        }
    }
    
    if (idx == -1) {
        if (mgr->num_routes >= MAX_ROUTES) {
            fprintf(stderr, "[IP-ROUTING] Route table full!\n");
            pthread_mutex_unlock(&mgr->lock);
            return -1;
        }
        idx = mgr->num_routes++;
    }
    
    // Check if route changed
    bool changed = !mgr->route_table[idx].valid ||
                   mgr->route_table[idx].gateway != gateway ||
                   mgr->route_table[idx].metric != metric;
    
    if (changed) {
        // Delete old route if exists
        if (mgr->route_table[idx].valid) {
            execute_route_command(mgr->interface_name,
                                dest_ip,
                                mgr->route_table[idx].gateway_ip,
                                mgr->route_table[idx].metric,
                                true);
        }
        
        // Add new route
        int ret = execute_route_command(mgr->interface_name,
                                       dest_ip, gateway_ip, metric, false);
        
        if (ret == 0) {
            mgr->route_table[idx].destination = destination;
            mgr->route_table[idx].gateway = gateway;
            strncpy(mgr->route_table[idx].dest_ip, dest_ip, sizeof(dest_ip));
            strncpy(mgr->route_table[idx].gateway_ip, gateway_ip, sizeof(gateway_ip));
            mgr->route_table[idx].metric = metric;
            mgr->route_table[idx].valid = true;
            mgr->route_table[idx].last_updated_ms = get_current_time_ms();
            
            if (idx == mgr->num_routes - 1) {
                mgr->route_adds++;
            } else {
                mgr->route_updates++;
            }
            
            printf("[IP-ROUTING] ✅ %s via %s (Node %d) metric %u\n",
                   dest_ip, gateway_ip, gateway, metric);
        } else {
            mgr->route_errors++;
            fprintf(stderr, "[IP-ROUTING] ❌ Failed to add route to %s\n", dest_ip);
        }
        
        pthread_mutex_unlock(&mgr->lock);
        return ret;
    }
    
    pthread_mutex_unlock(&mgr->lock);
    return 0;  // No change needed
}

int ip_routing_manager_delete_route(ip_routing_manager_t *mgr,
                                    node_id_t destination) {
    pthread_mutex_lock(&mgr->lock);
    
    for (int i = 0; i < mgr->num_routes; i++) {
        if (mgr->route_table[i].destination == destination &&
            mgr->route_table[i].valid) {
            
            execute_route_command(mgr->interface_name,
                                mgr->route_table[i].dest_ip,
                                mgr->route_table[i].gateway_ip,
                                mgr->route_table[i].metric,
                                true);
            
            mgr->route_table[i].valid = false;
            mgr->route_deletes++;
            
            printf("[IP-ROUTING] ❌ Deleted route to %s\n",
                   mgr->route_table[i].dest_ip);
            
            pthread_mutex_unlock(&mgr->lock);
            return 0;
        }
    }
    
    pthread_mutex_unlock(&mgr->lock);
    return -1;
}

int ip_routing_manager_flush_all(ip_routing_manager_t *mgr) {
    pthread_mutex_lock(&mgr->lock);
    
    printf("[IP-ROUTING] Flushing all routes...\n");
    int count = 0;
    
    for (int i = 0; i < mgr->num_routes; i++) {
        if (mgr->route_table[i].valid) {
            execute_route_command(mgr->interface_name,
                                mgr->route_table[i].dest_ip,
                                mgr->route_table[i].gateway_ip,
                                mgr->route_table[i].metric,
                                true);
            mgr->route_table[i].valid = false;
            count++;
        }
    }
    
    mgr->num_routes = 0;
    
    pthread_mutex_unlock(&mgr->lock);
    return count;
}

// ========================================
// Integration with Routing Manager
// ========================================

int ip_routing_manager_update_from_routing(ip_routing_manager_t *mgr,
                                          routing_manager_t *routing_mgr) {
    int updates = 0;
    
    printf("[IP-ROUTING] Updating from routing table (version %lu)...\n",
           routing_mgr->topology_version);
    
    for (int i = 0; i < routing_mgr->current_topology.num_nodes; i++) {
        routing_entry_t *entry = &routing_mgr->routing_table[i];
        
        if (entry->destination == mgr->my_node_id) continue;
        if (!entry->valid || entry->next_hop == 0) continue;
        
        uint32_t metric = entry->distance;
        
        if (ip_routing_manager_add_route(mgr, entry->destination,
                                        entry->next_hop, metric) == 0) {
            updates++;
        }
    }
    
    printf("[IP-ROUTING] Updated %d routes\n", updates);
    return updates;
}

// ========================================
// Initialization
// ========================================

int ip_routing_manager_init(ip_routing_manager_t *mgr,
                            node_id_t my_id,
                            const char *interface,
                            int total_nodes) {
    memset(mgr, 0, sizeof(ip_routing_manager_t));
    
    mgr->my_node_id = my_id;
    mgr->num_nodes = total_nodes;
    strncpy(mgr->interface_name, interface, IFNAMSIZ - 1);
    
    pthread_mutex_init(&mgr->lock, NULL);
    
    // Enable IP forwarding
    if (enable_ip_forwarding() != 0) {
        fprintf(stderr, "[IP-ROUTING] Warning: Could not enable IP forwarding\n");
    }
    
    printf("[IP-ROUTING] Initialized for node %d on %s\n", my_id, interface);
    
    if (ip_routing_requires_sudo()) {
        fprintf(stderr, "[IP-ROUTING] ⚠️  WARNING: Requires sudo!\n");
        return -1;
    }
    
    return 0;
}

void ip_routing_manager_destroy(ip_routing_manager_t *mgr) {
    printf("[IP-ROUTING] Destroying...\n");
    ip_routing_manager_flush_all(mgr);
    pthread_mutex_destroy(&mgr->lock);
}

// ========================================
// Display Functions
// ========================================

void ip_routing_manager_print_table(ip_routing_manager_t *mgr) {
    pthread_mutex_lock(&mgr->lock);
    
    printf("\n╔════════════════════════════════════════════════╗\n");
    printf("║  IP ROUTING TABLE (Node %d)                     \n", mgr->my_node_id);
    printf("╚════════════════════════════════════════════════╝\n\n");
    
    printf("Destination     | Gateway        | Metric | Status\n");
    printf("----------------|----------------|--------|--------\n");
    
    for (int i = 0; i < mgr->num_routes; i++) {
        if (!mgr->route_table[i].valid) continue;
        
        ip_route_entry_t *entry = &mgr->route_table[i];
        
        printf("%-15s | %-14s | %-6u | ACTIVE\n",
               entry->dest_ip,
               entry->gateway_ip,
               entry->metric);
    }
    printf("\n");
    
    pthread_mutex_unlock(&mgr->lock);
}

void ip_routing_manager_print_stats(ip_routing_manager_t *mgr) {
    pthread_mutex_lock(&mgr->lock);
    
    printf("\n=== IP Routing Manager Stats ===\n");
    printf("Adds:    %u\n", mgr->route_adds);
    printf("Updates: %u\n", mgr->route_updates);
    printf("Deletes: %u\n", mgr->route_deletes);
    printf("Errors:  %u\n", mgr->route_errors);
    printf("Active:  %d routes\n", mgr->num_routes);
    
    pthread_mutex_unlock(&mgr->lock);
}

void ip_routing_manager_print_kernel_routes(ip_routing_manager_t *mgr) {
    printf("\n=== Kernel Routing Table ===\n");
    
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "ip route show dev %s", mgr->interface_name);
    system(cmd);
    
    printf("\n");
}