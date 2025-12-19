// include/routing_manager.h
#ifndef ROUTING_MANAGER_H
#define ROUTING_MANAGER_H

#include <stdint.h>      // <--- ADICIONA (para uint64_t, uint32_t)
#include <stdbool.h>     // <--- ADICIONA (para bool, true, false)
#include <pthread.h>     // <--- ADICIONA (para pthread_mutex_t)
#include "tdma_types.h"  // <--- ADICIONA (para node_id_t, MAX_NODES)
#include "connectivity_matrix.h"
#include "spanning_tree.h"
#include "dijkstra.h"

// Estratégias de routing disponíveis
typedef enum {
    ROUTING_STRATEGY_DIJKSTRA,   // Shortest path (optimal)
    ROUTING_STRATEGY_MST,        // Spanning Tree (fallback rápido)
    ROUTING_STRATEGY_HYBRID      // Usa MST como fallback, Dijkstra como optimal
} routing_strategy_t;

// Estado de cada rota
typedef enum {
    PATH_STATE_OPTIMAL,      // Usando Dijkstra (melhor caminho)
    PATH_STATE_FALLBACK,     // Usando MST (após falha)
    PATH_STATE_RECOMPUTING,  // Em processo de recálculo
    PATH_STATE_UNREACHABLE   // Destino não alcançável
} path_state_t;

// Entrada da routing table
typedef struct {
    node_id_t destination;
    node_id_t next_hop;
    uint8_t distance;        // Número de hops
    path_state_t state;
    bool valid;
} routing_entry_t;

// Routing Manager principal
typedef struct {
    // Configuração
    node_id_t my_node_id;
    routing_strategy_t strategy;
    
    // Dados de topologia
    connectivity_matrix_t current_topology;
    spanning_tree_t mst;
    uint64_t topology_version;  // Incrementa a cada mudança
    
    // Routing tables
    routing_entry_t routing_table[MAX_NODES];
    dijkstra_result_t dijkstra_cache[MAX_NODES];  // Cache de Dijkstra
    
    // Sincronização
    pthread_mutex_t lock;
    bool needs_recomputation;
    
    // Estatísticas básicas
    uint32_t recomputations;
    uint32_t link_failures_detected;
    uint64_t last_update_time_ms;
    
    // ========== MÉTRICAS DE PERFORMANCE ==========
    uint64_t total_recompute_time_us;      // Tempo total em microsegundos
    uint64_t last_recompute_time_us;       // Último recompute
    uint64_t max_recompute_time_us;        // Pior caso
    uint64_t min_recompute_time_us;        // Melhor caso
    
    // Breakdown por estratégia
    uint64_t dijkstra_compute_time_us;
    uint64_t mst_compute_time_us;
    uint64_t table_update_time_us;
    
} routing_manager_t;

// ========================================
// API Principal
// ========================================

// Inicializa o Routing Manager
void routing_manager_init(routing_manager_t *rm, 
                         node_id_t my_id,
                         routing_strategy_t strategy);

// Atualiza topologia (detecta mudanças automaticamente)
bool routing_manager_update_topology(routing_manager_t *rm,
                                    connectivity_matrix_t *new_topology);

// Obtém next hop para um destino
node_id_t routing_manager_get_next_hop(routing_manager_t *rm, 
                                       node_id_t destination);

// Força recomputation (útil após link failure)
void routing_manager_force_recompute(routing_manager_t *rm);

// Verifica se rota mudou desde última atualização
bool routing_manager_route_changed(routing_manager_t *rm, 
                                  node_id_t destination);

// Imprime estado atual
void routing_manager_print_table(routing_manager_t *rm);
void routing_manager_print_stats(routing_manager_t *rm);

// Performance metrics (NOVO)
void routing_manager_print_performance(routing_manager_t *rm);
void routing_manager_export_metrics_csv(routing_manager_t *rm, const char *filename);

// Cleanup
void routing_manager_destroy(routing_manager_t *rm);

// ========================================
// Funções Internas (Helpers)
// ========================================

// Detecta se topologia mudou
bool topology_has_changed(connectivity_matrix_t *old, 
                         connectivity_matrix_t *new);

// Recomputa rotas (escolhe estratégia)
void recompute_routes(routing_manager_t *rm);

// Atualiza routing table com resultados de Dijkstra
void update_table_from_dijkstra(routing_manager_t *rm);

// Atualiza routing table com MST (fallback)
void update_table_from_mst(routing_manager_t *rm);

#endif // ROUTING_MANAGER_H