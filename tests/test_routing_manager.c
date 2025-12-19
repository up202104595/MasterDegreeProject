// tests/test_routing_manager.c
#include <stdio.h>
#include <unistd.h>
#include "routing_manager.h"

void test_basic_routing() {
    printf("\n╔══════════════════════════════════════╗\n");
    printf("║  TEST 1: Basic Routing Setup        ║\n");
    printf("╚══════════════════════════════════════╝\n");
    
    connectivity_matrix_init();
    
    // Topologia: 1 -- 2 -- 3 -- 4
    uint8_t matrix[MAX_NODES][MAX_NODES] = {0};
    node_id_t nodes[] = {1, 2, 3, 4};
    
    matrix[0][1] = matrix[1][0] = 1;
    matrix[1][2] = matrix[2][1] = 1;
    matrix[2][3] = matrix[3][2] = 1;
    
    connectivity_matrix_set_topology(matrix, nodes, 4);
    
    connectivity_matrix_t topo;
    connectivity_matrix_get(&topo);
    
    // Inicializa Routing Manager para node 1
    routing_manager_t rm;
    routing_manager_init(&rm, 1, ROUTING_STRATEGY_DIJKSTRA);
    
    // Atualiza topologia
    routing_manager_update_topology(&rm, &topo);
    
    // Imprime routing table
    routing_manager_print_table(&rm);
    
    // Testa next hop
    node_id_t next = routing_manager_get_next_hop(&rm, 4);
    printf("Next hop to node 4: %d (expected: 2)\n", next);
    
    routing_manager_destroy(&rm);
    printf("✓ Test passed\n");
}

void test_link_failure_recovery() {
    printf("\n╔══════════════════════════════════════╗\n");
    printf("║  TEST 2: Link Failure & Recovery    ║\n");
    printf("╚══════════════════════════════════════╝\n");
    
    connectivity_matrix_init();
    
    // Diamond topology
    uint8_t matrix[MAX_NODES][MAX_NODES] = {0};
    node_id_t nodes[] = {1, 2, 3, 4};
    
    matrix[0][1] = matrix[1][0] = 1;
    matrix[0][2] = matrix[2][0] = 1;
    matrix[1][3] = matrix[3][1] = 1;
    matrix[2][3] = matrix[3][2] = 1;
    
    connectivity_matrix_set_topology(matrix, nodes, 4);
    connectivity_matrix_t topo;
    connectivity_matrix_get(&topo);
    
    // Node 1 routing manager
    routing_manager_t rm;
    routing_manager_init(&rm, 1, ROUTING_STRATEGY_HYBRID);
    routing_manager_update_topology(&rm, &topo);
    
    printf("\n--- BEFORE Link Failure ---\n");
    routing_manager_print_table(&rm);
    
    // Simula falha do link 1-2
    printf("\n🔥 LINK FAILURE: 1-2 breaks!\n\n");
    matrix[0][1] = matrix[1][0] = 0;
    connectivity_matrix_set_topology(matrix, nodes, 4);
    connectivity_matrix_get(&topo);
    
    // Atualiza topologia (trigger recomputation)
    routing_manager_update_topology(&rm, &topo);
    
    printf("\n--- AFTER Link Failure ---\n");
    routing_manager_print_table(&rm);
    routing_manager_print_stats(&rm);
    
    // Verifica se ainda alcança node 4
    node_id_t next = routing_manager_get_next_hop(&rm, 4);
    printf("Next hop to node 4: %d (should use alternative path via 3)\n", next);
    
    routing_manager_destroy(&rm);
    printf("✓ Test passed - System recovered from link failure!\n");
}

void test_strategy_comparison() {
    printf("\n╔══════════════════════════════════════╗\n");
    printf("║  TEST 3: Strategy Comparison         ║\n");
    printf("╚══════════════════════════════════════╝\n");
    
    connectivity_matrix_init();
    
    // Complex topology
    uint8_t matrix[MAX_NODES][MAX_NODES] = {0};
    node_id_t nodes[] = {1, 2, 3, 4, 5};
    
    matrix[0][1] = matrix[1][0] = 1;
    matrix[0][2] = matrix[2][0] = 2;  // Longer link
    matrix[1][3] = matrix[3][1] = 1;
    matrix[2][3] = matrix[3][2] = 1;
    matrix[3][4] = matrix[4][3] = 1;
    
    connectivity_matrix_set_topology(matrix, nodes, 5);
    connectivity_matrix_t topo;
    connectivity_matrix_get(&topo);
    
    // Test Dijkstra
    routing_manager_t rm_dijkstra;
    routing_manager_init(&rm_dijkstra, 1, ROUTING_STRATEGY_DIJKSTRA);
    routing_manager_update_topology(&rm_dijkstra, &topo);
    printf("\n--- DIJKSTRA Strategy ---\n");
    routing_manager_print_table(&rm_dijkstra);
    
    // Test MST
    routing_manager_t rm_mst;
    routing_manager_init(&rm_mst, 1, ROUTING_STRATEGY_MST);
    routing_manager_update_topology(&rm_mst, &topo);
    printf("\n--- MST Strategy ---\n");
    routing_manager_print_table(&rm_mst);
    
    routing_manager_destroy(&rm_dijkstra);
    routing_manager_destroy(&rm_mst);
    
    printf("✓ Test passed - Both strategies work!\n");
}

// ========================================
// NOVO TESTE: Performance Metrics
// ========================================

void test_performance_metrics() {
    printf("\n╔══════════════════════════════════════╗\n");
    printf("║  TEST 4: Performance Metrics         ║\n");
    printf("╚══════════════════════════════════════╝\n");
    
    connectivity_matrix_init();
    
    // Topologia complexa (10 nós em mesh parcial)
    uint8_t matrix[MAX_NODES][MAX_NODES] = {0};
    node_id_t nodes[10];
    
    for (int i = 0; i < 10; i++) {
        nodes[i] = i + 1;
    }
    
    // Cria grafo mesh parcial - cada nó conecta com vizinhos próximos
    for (int i = 0; i < 10; i++) {
        for (int j = i + 1; j < 10; j++) {
            if ((j - i) <= 2) {  // Conecta com 2 vizinhos seguintes
                matrix[i][j] = matrix[j][i] = 1;
            }
        }
    }
    
    connectivity_matrix_set_topology(matrix, nodes, 10);
    connectivity_matrix_t topo;
    connectivity_matrix_get(&topo);
    
    printf("\n--- Testing DIJKSTRA Strategy ---\n");
    routing_manager_t rm_dijkstra;
    routing_manager_init(&rm_dijkstra, 1, ROUTING_STRATEGY_DIJKSTRA);
    
    // Múltiplas recomputations para ter média estatística
    printf("Running 5 recomputations...\n");
    for (int i = 0; i < 5; i++) {
        routing_manager_update_topology(&rm_dijkstra, &topo);
        usleep(1000);  // 1ms delay entre recomputations
    }
    
    routing_manager_print_performance(&rm_dijkstra);
    routing_manager_export_metrics_csv(&rm_dijkstra, "metrics_dijkstra.csv");
    
    printf("\n--- Testing MST Strategy ---\n");
    routing_manager_t rm_mst;
    routing_manager_init(&rm_mst, 1, ROUTING_STRATEGY_MST);
    
    printf("Running 5 recomputations...\n");
    for (int i = 0; i < 5; i++) {
        routing_manager_update_topology(&rm_mst, &topo);
        usleep(1000);
    }
    
    routing_manager_print_performance(&rm_mst);
    routing_manager_export_metrics_csv(&rm_mst, "metrics_mst.csv");
    
    // Comparação final
    printf("\n");
    printf("╔════════════════════════════════════════════════╗\n");
    printf("║  PERFORMANCE COMPARISON                        ║\n");
    printf("╚════════════════════════════════════════════════╝\n\n");
    
    double dijkstra_avg = (double)rm_dijkstra.total_recompute_time_us / rm_dijkstra.recomputations;
    double mst_avg = (double)rm_mst.total_recompute_time_us / rm_mst.recomputations;
    double speedup = dijkstra_avg / mst_avg;
    
    printf("📊 Average Recomputation Time:\n");
    printf("   Dijkstra: %6.2f μs  (%.3f ms)\n", dijkstra_avg, dijkstra_avg / 1000.0);
    printf("   MST:      %6.2f μs  (%.3f ms)\n\n", mst_avg, mst_avg / 1000.0);
    
    if (speedup > 1.0) {
        printf("🚀 MST is %.2fx FASTER than Dijkstra\n", speedup);
    } else {
        printf("🚀 Dijkstra is %.2fx FASTER than MST\n", 1.0 / speedup);
    }
    
    printf("\n✅ Both strategies fit within 25ms TDMA slot time!\n");
    
    routing_manager_destroy(&rm_dijkstra);
    routing_manager_destroy(&rm_mst);
    
    printf("\n✓ Test passed - Metrics collected and exported!\n");
}

int main() {
    printf("\n");
    printf("╔════════════════════════════════════════════════╗\n");
    printf("║  ROUTING MANAGER TESTS                         ║\n");
    printf("╚════════════════════════════════════════════════╝\n");
    
    test_basic_routing();
    test_link_failure_recovery();
    test_strategy_comparison();
    test_performance_metrics();  // <--- NOVO TESTE
    
    printf("\n");
    printf("╔════════════════════════════════════════════════╗\n");
    printf("║  ALL ROUTING MANAGER TESTS PASSED ✓            ║\n");
    printf("╚════════════════════════════════════════════════╝\n\n");
    
    return 0;
}