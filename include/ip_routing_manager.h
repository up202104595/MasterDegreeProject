// include/ip_routing_manager.h
#ifndef IP_ROUTING_MANAGER_H
#define IP_ROUTING_MANAGER_H

#include <stdint.h>
#include <stdbool.h>
#include <pthread.h>
#include "routing_manager.h"

#define MAX_ROUTES 32
#define IFNAMSIZ 16

typedef struct {
    node_id_t destination;
    node_id_t gateway;
    char dest_ip[16];
    char gateway_ip[16];
    uint32_t metric;
    bool valid;
    uint64_t last_updated_ms;
} ip_route_entry_t;

typedef struct {
    node_id_t my_node_id;
    int num_nodes;
    char interface_name[IFNAMSIZ];
    
    // Route table
    ip_route_entry_t route_table[MAX_ROUTES];
    int num_routes;
    
    // Stats
    uint32_t route_adds;
    uint32_t route_deletes;
    uint32_t route_updates;
    uint32_t route_errors;
    
    pthread_mutex_t lock;
} ip_routing_manager_t;

// Init/Destroy
int ip_routing_manager_init(ip_routing_manager_t *mgr, 
                            node_id_t my_id,
                            const char *interface,
                            int total_nodes);
void ip_routing_manager_destroy(ip_routing_manager_t *mgr);

// Route Management
int ip_routing_manager_add_route(ip_routing_manager_t *mgr,
                                 node_id_t destination,
                                 node_id_t gateway,
                                 uint32_t metric);
int ip_routing_manager_delete_route(ip_routing_manager_t *mgr,
                                    node_id_t destination);
int ip_routing_manager_flush_all(ip_routing_manager_t *mgr);

// Integration with Routing Manager
int ip_routing_manager_update_from_routing(ip_routing_manager_t *mgr,
                                          routing_manager_t *routing_mgr);

// Display
void ip_routing_manager_print_table(ip_routing_manager_t *mgr);
void ip_routing_manager_print_stats(ip_routing_manager_t *mgr);
void ip_routing_manager_print_kernel_routes(ip_routing_manager_t *mgr);

// Utilities
bool ip_routing_requires_sudo(void);
void node_id_to_ip_str(node_id_t node_id, char *ip_str);

#endif // IP_ROUTING_MANAGER_H