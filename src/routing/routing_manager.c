// src/routing/routing_manager.c
#include "routing_manager.h"
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>  // <--- ADICIONADO para microsegundos
#include <limits.h>    // <--- ADICIONADO para UINT64_MAX

// ========================================
// Funções de Timing (NOVAS)
// ========================================

static uint64_t get_current_time_us(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint64_t)tv.tv_sec * 1000000 + tv.tv_usec;
}

static uint64_t get_current_time_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

// ========================================
// Funções Auxiliares
// ========================================

static int find_node_index(connectivity_matrix_t *topo, node_id_t node) {
    for (int i = 0; i < topo->num_nodes; i++) {
        if (topo->node_ids[i] == node) return i;
    }
    return -1;
}

// ========================================
// Detecção de Mudanças
// ========================================

bool topology_has_changed(connectivity_matrix_t *old, 
                         connectivity_matrix_t *new) {
    // Compara número de nós
    if (old->num_nodes != new->num_nodes) return true;
    
    // Compara matriz de conectividade
    for (int i = 0; i < old->num_nodes; i++) {
        for (int j = 0; j < old->num_nodes; j++) {
            if (old->matrix[i][j] != new->matrix[i][j]) {
                printf("[ROUTING] Topology change detected: link %d-%d changed\n",
                       old->node_ids[i], old->node_ids[j]);
                return true;
            }
        }
    }
    
    return false;
}

// ========================================
// Recomputation Logic
// ========================================

void update_table_from_dijkstra(routing_manager_t *rm) {
    // Roda Dijkstra para todos os destinos
    dijkstra_compute(rm->my_node_id, &rm->current_topology, rm->dijkstra_cache);
    
    // Atualiza routing table
    for (int i = 0; i < rm->current_topology.num_nodes; i++) {
        node_id_t dest = rm->dijkstra_cache[i].destination;
        
        if (dest == rm->my_node_id) continue;  // Skip self
        
        int idx = find_node_index(&rm->current_topology, dest);
        if (idx == -1) continue;
        
        rm->routing_table[idx].destination = dest;
        rm->routing_table[idx].next_hop = rm->dijkstra_cache[i].next_hop;
        rm->routing_table[idx].distance = rm->dijkstra_cache[i].distance;
        rm->routing_table[idx].valid = rm->dijkstra_cache[i].reachable;
        rm->routing_table[idx].state = rm->dijkstra_cache[i].reachable ? 
                                       PATH_STATE_OPTIMAL : PATH_STATE_UNREACHABLE;
    }
}

void update_table_from_mst(routing_manager_t *rm) {
    // Computa MST
    spanning_tree_compute(&rm->current_topology, &rm->mst);
    
    // BFS na MST para encontrar next hops
    int my_idx = find_node_index(&rm->current_topology, rm->my_node_id);
    if (my_idx == -1) return;
    
    bool visited[MAX_NODES] = {false};
    int parent[MAX_NODES];
    int queue[MAX_NODES];
    int front = 0, rear = 0;
    
    for (int i = 0; i < MAX_NODES; i++) parent[i] = -1;
    
    visited[my_idx] = true;
    queue[rear++] = my_idx;
    
    while (front < rear) {
        int u = queue[front++];
        
        for (int v = 0; v < rm->mst.num_nodes; v++) {
            if (rm->mst.tree[u][v] && !visited[v]) {
                visited[v] = true;
                parent[v] = u;
                queue[rear++] = v;
            }
        }
    }
    
    // Atualiza routing table
    for (int i = 0; i < rm->current_topology.num_nodes; i++) {
        if (i == my_idx) continue;
        
        // Encontra next hop (primeiro passo no caminho)
        int curr = i;
        int prev = parent[i];
        
        if (prev == -1) {
            // Unreachable
            rm->routing_table[i].valid = false;
            rm->routing_table[i].state = PATH_STATE_UNREACHABLE;
            continue;
        }
        
        while (parent[prev] != my_idx && prev != my_idx) {
            curr = prev;
            prev = parent[prev];
        }
        
        rm->routing_table[i].destination = rm->current_topology.node_ids[i];
        rm->routing_table[i].next_hop = rm->current_topology.node_ids[curr];
        rm->routing_table[i].valid = true;
        rm->routing_table[i].state = PATH_STATE_FALLBACK;
        
        // Calcula distância (número de hops)
        int hops = 0;
        int node = i;
        while (parent[node] != -1) {
            hops++;
            node = parent[node];
        }
        rm->routing_table[i].distance = hops;
    }
}

void recompute_routes(routing_manager_t *rm) {
    uint64_t start_total = get_current_time_us();  // <--- TIMING COMEÇA
    
    printf("[ROUTING] Recomputing routes (strategy: %d)...\n", rm->strategy);
    
    uint64_t start_algo = 0;
    
    switch (rm->strategy) {
        case ROUTING_STRATEGY_DIJKSTRA:
            start_algo = get_current_time_us();
            update_table_from_dijkstra(rm);
            rm->dijkstra_compute_time_us = get_current_time_us() - start_algo;
            break;
            
        case ROUTING_STRATEGY_MST:
            start_algo = get_current_time_us();
            update_table_from_mst(rm);
            rm->mst_compute_time_us = get_current_time_us() - start_algo;
            break;
            
        case ROUTING_STRATEGY_HYBRID:
            // Primeiro tenta Dijkstra (optimal)
            start_algo = get_current_time_us();
            update_table_from_dijkstra(rm);
            rm->dijkstra_compute_time_us = get_current_time_us() - start_algo;
            
            // Se alguma rota falhou, usa MST como fallback
            for (int i = 0; i < rm->current_topology.num_nodes; i++) {
                if (!rm->routing_table[i].valid) {
                    printf("[ROUTING] Using MST fallback for unreachable nodes\n");
                    start_algo = get_current_time_us();
                    update_table_from_mst(rm);
                    rm->mst_compute_time_us = get_current_time_us() - start_algo;
                    break;
                }
            }
            break;
    }
    
    uint64_t end_total = get_current_time_us();  // <--- TIMING TERMINA
    uint64_t elapsed = end_total - start_total;
    
    // Atualiza estatísticas
    rm->recomputations++;
    rm->last_recompute_time_us = elapsed;
    rm->total_recompute_time_us += elapsed;
    rm->last_update_time_ms = get_current_time_ms();
    rm->needs_recomputation = false;
    
    // Atualiza min/max
    if (elapsed < rm->min_recompute_time_us) {
        rm->min_recompute_time_us = elapsed;
    }
    if (elapsed > rm->max_recompute_time_us) {
        rm->max_recompute_time_us = elapsed;
    }
    
    printf("[ROUTING] Recomputation complete (version %lu) - %lu μs\n", 
           rm->topology_version, elapsed);
}

// ========================================
// API Pública
// ========================================

void routing_manager_init(routing_manager_t *rm, 
                         node_id_t my_id,
                         routing_strategy_t strategy) {
    memset(rm, 0, sizeof(routing_manager_t));
    
    rm->my_node_id = my_id;
    rm->strategy = strategy;
    rm->topology_version = 0;
    rm->needs_recomputation = false;
    
    // Inicializa métricas de performance
    rm->min_recompute_time_us = UINT64_MAX;
    
    pthread_mutex_init(&rm->lock, NULL);
    
    printf("[ROUTING] Manager initialized for node %d (strategy: %d)\n", 
           my_id, strategy);
}

bool routing_manager_update_topology(routing_manager_t *rm,
                                    connectivity_matrix_t *new_topology) {
    pthread_mutex_lock(&rm->lock);
    
    bool changed = topology_has_changed(&rm->current_topology, new_topology);
    
    if (changed) {
        printf("[ROUTING] Topology version %lu → %lu\n", 
               rm->topology_version, rm->topology_version + 1);
        
        // Copia nova topologia
        memcpy(&rm->current_topology, new_topology, sizeof(connectivity_matrix_t));
        rm->topology_version++;
        rm->needs_recomputation = true;
        rm->link_failures_detected++;
        
        // Recomputa rotas
        recompute_routes(rm);
    }
    
    pthread_mutex_unlock(&rm->lock);
    return changed;
}

node_id_t routing_manager_get_next_hop(routing_manager_t *rm, 
                                       node_id_t destination) {
    pthread_mutex_lock(&rm->lock);
    
    int idx = find_node_index(&rm->current_topology, destination);
    node_id_t next_hop = 255;  // Invalid
    
    if (idx != -1 && rm->routing_table[idx].valid) {
        next_hop = rm->routing_table[idx].next_hop;
    }
    
    pthread_mutex_unlock(&rm->lock);
    return next_hop;
}

void routing_manager_force_recompute(routing_manager_t *rm) {
    pthread_mutex_lock(&rm->lock);
    rm->needs_recomputation = true;
    recompute_routes(rm);
    pthread_mutex_unlock(&rm->lock);
}

bool routing_manager_route_changed(routing_manager_t *rm, 
                                  node_id_t destination) {
    // TODO: Implementar cache de rotas anteriores
    (void)rm;
    (void)destination;
    return false;
}

void routing_manager_print_table(routing_manager_t *rm) {
    pthread_mutex_lock(&rm->lock);
    
    printf("\n=== Routing Table (Node %d) ===\n", rm->my_node_id);
    printf("Version: %lu | Strategy: %d\n", rm->topology_version, rm->strategy);
    printf("Destination | Next Hop | Distance | State\n");
    printf("------------|----------|----------|----------\n");
    
    for (int i = 0; i < rm->current_topology.num_nodes; i++) {
        if (rm->routing_table[i].destination == 0) continue;
        if (rm->routing_table[i].destination == rm->my_node_id) continue;
        
        const char *state_str[] = {"OPTIMAL", "FALLBACK", "RECOMPUTING", "UNREACHABLE"};
        
        printf("    %3d     |    %3d   |    %3d   | %s\n",
               rm->routing_table[i].destination,
               rm->routing_table[i].next_hop,
               rm->routing_table[i].distance,
               state_str[rm->routing_table[i].state]);
    }
    printf("\n");
    
    pthread_mutex_unlock(&rm->lock);
}

void routing_manager_print_stats(routing_manager_t *rm) {
    pthread_mutex_lock(&rm->lock);
    
    printf("\n=== Routing Manager Stats ===\n");
    printf("Node ID:           %d\n", rm->my_node_id);
    printf("Topology Version:  %lu\n", rm->topology_version);
    printf("Recomputations:    %u\n", rm->recomputations);
    printf("Link Failures:     %u\n", rm->link_failures_detected);
    printf("Last Update:       %lu ms ago\n", 
           get_current_time_ms() - rm->last_update_time_ms);
    printf("\n");
    
    pthread_mutex_unlock(&rm->lock);
}

// ========================================
// NOVAS FUNÇÕES DE PERFORMANCE
// ========================================

void routing_manager_print_performance(routing_manager_t *rm) {
    pthread_mutex_lock(&rm->lock);
    
    printf("\n╔════════════════════════════════════════════════╗\n");
    printf("║  ROUTING MANAGER PERFORMANCE METRICS           ║\n");
    printf("╚════════════════════════════════════════════════╝\n\n");
    
    printf("📊 General Statistics:\n");
    printf("   Node ID:              %d\n", rm->my_node_id);
    printf("   Strategy:             %s\n", 
           rm->strategy == 0 ? "DIJKSTRA" : 
           rm->strategy == 1 ? "MST" : "HYBRID");
    printf("   Topology Version:     %lu\n", rm->topology_version);
    printf("   Total Recomputations: %u\n", rm->recomputations);
    printf("   Link Failures:        %u\n\n", rm->link_failures_detected);
    
    if (rm->recomputations > 0) {
        printf("⏱️  Recomputation Timing (microseconds):\n");
        printf("   Last:    %6lu μs  (%.3f ms)\n", 
               rm->last_recompute_time_us,
               rm->last_recompute_time_us / 1000.0);
        printf("   Average: %6lu μs  (%.3f ms)\n", 
               rm->total_recompute_time_us / rm->recomputations,
               (rm->total_recompute_time_us / rm->recomputations) / 1000.0);
        printf("   Min:     %6lu μs  (%.3f ms)\n", 
               rm->min_recompute_time_us,
               rm->min_recompute_time_us / 1000.0);
        printf("   Max:     %6lu μs  (%.3f ms)\n\n", 
               rm->max_recompute_time_us,
               rm->max_recompute_time_us / 1000.0);
        
        printf("🔍 Algorithm Breakdown:\n");
        if (rm->dijkstra_compute_time_us > 0) {
            printf("   Dijkstra:     %6lu μs  (%.3f ms)\n", 
                   rm->dijkstra_compute_time_us,
                   rm->dijkstra_compute_time_us / 1000.0);
        }
        if (rm->mst_compute_time_us > 0) {
            printf("   MST:          %6lu μs  (%.3f ms)\n", 
                   rm->mst_compute_time_us,
                   rm->mst_compute_time_us / 1000.0);
        }
        
        printf("\n✅ Timing Analysis:\n");
        double avg_ms = (rm->total_recompute_time_us / rm->recomputations) / 1000.0;
        if (avg_ms < 1.0) {
            printf("   Status: EXCELLENT (< 1ms average)\n");
        } else if (avg_ms < 5.0) {
            printf("   Status: GOOD (< 5ms average)\n");
        } else if (avg_ms < 25.0) {
            printf("   Status: ACCEPTABLE (< 25ms slot time)\n");
        } else {
            printf("   Status: ⚠️  WARNING (> 25ms slot time!)\n");
        }
        
        // Cálculo de throughput
        double slot_time_ms = 25.0;  // TDMA slot time
        double overhead_pct = (avg_ms / slot_time_ms) * 100.0;
        printf("   Slot overhead: %.2f%% of 25ms slot\n", overhead_pct);
    }
    
    printf("\n");
    pthread_mutex_unlock(&rm->lock);
}

void routing_manager_export_metrics_csv(routing_manager_t *rm, const char *filename) {
    pthread_mutex_lock(&rm->lock);
    
    FILE *fp = fopen(filename, "w");
    if (!fp) {
        printf("[ERROR] Cannot open %s for writing\n", filename);
        pthread_mutex_unlock(&rm->lock);
        return;
    }
    
    // Header
    fprintf(fp, "node_id,strategy,topology_version,recomputations,link_failures,");
    fprintf(fp, "avg_time_us,min_time_us,max_time_us,last_time_us,");
    fprintf(fp, "dijkstra_time_us,mst_time_us,slot_overhead_pct\n");
    
    // Data
    double avg_us = rm->recomputations > 0 ? 
                    (double)rm->total_recompute_time_us / rm->recomputations : 0;
    double slot_overhead = (avg_us / 1000.0) / 25.0 * 100.0;
    
    fprintf(fp, "%d,%d,%lu,%u,%u,%.2f,%lu,%lu,%lu,%lu,%lu,%.2f\n",
            rm->my_node_id,
            rm->strategy,
            rm->topology_version,
            rm->recomputations,
            rm->link_failures_detected,
            avg_us,
            rm->min_recompute_time_us,
            rm->max_recompute_time_us,
            rm->last_recompute_time_us,
            rm->dijkstra_compute_time_us,
            rm->mst_compute_time_us,
            slot_overhead);
    
    fclose(fp);
    printf("[EXPORT] Metrics saved to %s\n", filename);
    
    pthread_mutex_unlock(&rm->lock);
}

void routing_manager_destroy(routing_manager_t *rm) {
    pthread_mutex_destroy(&rm->lock);
    printf("[ROUTING] Manager destroyed for node %d\n", rm->my_node_id);
}