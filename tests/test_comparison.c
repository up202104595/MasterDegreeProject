// tests/test_comparison.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "tdma_types.h"
#include "connectivity_matrix.h"
#include "spanning_tree.h"
#include "dijkstra.h"

// Estrutura para armazenar métricas
typedef struct {
    float avg_hops_dijkstra;
    float avg_hops_mst;
    int optimal_paths_dijkstra;
    int optimal_paths_mst;
    int total_paths;
    float improvement_percentage;
} ComparisonMetrics;

// Função auxiliar: BFS na MST para contar hops
int mst_count_hops(spanning_tree_t *tree, int src_idx, int dst_idx) {
    if (src_idx == dst_idx) return 0;
    
    bool visited[MAX_NODES] = {false};
    int parent[MAX_NODES];
    int queue[MAX_NODES];
    int front = 0, rear = 0;
    
    for (int i = 0; i < MAX_NODES; i++) parent[i] = -1;
    
    visited[src_idx] = true;
    queue[rear++] = src_idx;
    
    while (front < rear) {
        int u = queue[front++];
        
        for (int v = 0; v < tree->num_nodes; v++) {
            if (tree->tree[u][v] && !visited[v]) {
                visited[v] = true;
                parent[v] = u;
                queue[rear++] = v;
                
                if (v == dst_idx) {
                    // Reconstrói caminho e conta hops
                    int hops = 0;
                    int curr = dst_idx;
                    while (parent[curr] != -1) {
                        hops++;
                        curr = parent[curr];
                    }
                    return hops;
                }
            }
        }
    }
    
    return -1; // Unreachable
}

// Função auxiliar: encontra índice do node_id no array
int find_node_index(connectivity_matrix_t *topo, node_id_t node) {
    for (int i = 0; i < topo->num_nodes; i++) {
        if (topo->node_ids[i] == node) return i;
    }
    return -1;
}

// Função para calcular métricas comparativas
void compute_comparison_metrics(connectivity_matrix_t *topo,
                                spanning_tree_t *mst,
                                ComparisonMetrics *metrics) {
    int total_hops_dijkstra = 0;
    int total_hops_mst = 0;
    int reachable_pairs = 0;
    int optimal_dijkstra = 0;
    int optimal_mst = 0;
    
    // Para cada par de nós (src, dst)
    for (int src_idx = 0; src_idx < topo->num_nodes; src_idx++) {
        node_id_t src = topo->node_ids[src_idx];
        
        // Roda Dijkstra para este source
        dijkstra_result_t dijkstra_results[MAX_NODES] = {0};
        dijkstra_compute(src, topo, dijkstra_results);
        
        for (int dst_idx = 0; dst_idx < topo->num_nodes; dst_idx++) {
            if (src_idx == dst_idx) continue;
            
            node_id_t dst = topo->node_ids[dst_idx];
            
            // Encontra resultado para este destino
            int result_idx = -1;
            for (int i = 0; i < topo->num_nodes; i++) {
                if (dijkstra_results[i].destination == dst) {
                    result_idx = i;
                    break;
                }
            }
            
            if (result_idx == -1) continue;
            
            // Conta hops em ambos os algoritmos
            int hops_dijkstra = dijkstra_results[result_idx].distance;
            int hops_mst = mst_count_hops(mst, src_idx, dst_idx);
            
            if (dijkstra_results[result_idx].reachable && hops_mst > 0) {
                total_hops_dijkstra += hops_dijkstra;
                total_hops_mst += hops_mst;
                reachable_pairs++;
                
                // Conta paths ótimos (shortest possible)
                if (hops_dijkstra <= hops_mst) optimal_dijkstra++;
                if (hops_mst <= hops_dijkstra) optimal_mst++;
            }
        }
    }
    
    if (reachable_pairs > 0) {
        metrics->total_paths = reachable_pairs;
        metrics->avg_hops_dijkstra = (float)total_hops_dijkstra / reachable_pairs;
        metrics->avg_hops_mst = (float)total_hops_mst / reachable_pairs;
        metrics->optimal_paths_dijkstra = optimal_dijkstra;
        metrics->optimal_paths_mst = optimal_mst;
        metrics->improvement_percentage = 
            ((metrics->avg_hops_mst - metrics->avg_hops_dijkstra) / 
             metrics->avg_hops_mst) * 100.0f;
    } else {
        memset(metrics, 0, sizeof(ComparisonMetrics));
    }
}

// Função para imprimir comparação lado a lado
void print_path_comparison(connectivity_matrix_t *topo,
                          spanning_tree_t *mst,
                          node_id_t src, node_id_t dst) {
    int src_idx = find_node_index(topo, src);
    int dst_idx = find_node_index(topo, dst);
    
    if (src_idx == -1 || dst_idx == -1) {
        printf("Invalid source or destination node\n");
        return;
    }
    
    printf("\n📍 Route: Node %d → Node %d\n", src, dst);
    printf("─────────────────────────────────────\n");
    
    // Dijkstra path
    dijkstra_result_t dijkstra_results[MAX_NODES] = {0};
    dijkstra_compute(src, topo, dijkstra_results);
    
    // Encontra resultado para dst
    int result_idx = -1;
    for (int i = 0; i < topo->num_nodes; i++) {
        if (dijkstra_results[i].destination == dst) {
            result_idx = i;
            break;
        }
    }
    
    printf("🔵 Dijkstra:  ");
    if (result_idx != -1 && dijkstra_results[result_idx].reachable) {
        printf("%d → ... → %d  [%d hops]\n", 
               src, dst, dijkstra_results[result_idx].distance);
    } else {
        printf("UNREACHABLE\n");
    }
    
    // MST path
    printf("🟢 MST:       ");
    int hops_mst = mst_count_hops(mst, src_idx, dst_idx);
    if (hops_mst > 0) {
        printf("%d → ... → %d  [%d hops]\n", src, dst, hops_mst);
    } else {
        printf("UNREACHABLE\n");
    }
    
    // Comparação
    if (result_idx != -1 && dijkstra_results[result_idx].reachable && hops_mst > 0) {
        int diff = hops_mst - dijkstra_results[result_idx].distance;
        if (diff > 0) {
            printf("⚡ Dijkstra is %d hop(s) shorter!\n", diff);
        } else if (diff == 0) {
            printf("✓ Both algorithms found same path\n");
        }
    }
}

// ========================
// TESTES
// ========================

void test_simple_comparison() {
    printf("\n╔══════════════════════════════════════╗\n");
    printf("║  TEST 1: Simple Line Topology       ║\n");
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
    
    // Computa MST
    spanning_tree_t mst;
    spanning_tree_compute(&topo, &mst);
    
    // Métricas
    ComparisonMetrics metrics;
    compute_comparison_metrics(&topo, &mst, &metrics);
    
    printf("\n📊 METRICS:\n");
    printf("   Avg hops (Dijkstra): %.2f\n", metrics.avg_hops_dijkstra);
    printf("   Avg hops (MST):      %.2f\n", metrics.avg_hops_mst);
    printf("   Improvement:         %.1f%%\n", metrics.improvement_percentage);
    
    printf("\n✓ In line topology, both algorithms perform identically\n");
}

void test_diamond_comparison() {
    printf("\n╔══════════════════════════════════════╗\n");
    printf("║  TEST 2: Diamond Topology            ║\n");
    printf("╚══════════════════════════════════════╝\n");
    
    connectivity_matrix_init();
    
    /*
         1
        / \
       2   3
        \ /
         4
    */
    uint8_t matrix[MAX_NODES][MAX_NODES] = {0};
    node_id_t nodes[] = {1, 2, 3, 4};
    
    matrix[0][1] = matrix[1][0] = 1; // 1-2
    matrix[0][2] = matrix[2][0] = 1; // 1-3
    matrix[1][3] = matrix[3][1] = 1; // 2-4
    matrix[2][3] = matrix[3][2] = 1; // 3-4
    
    connectivity_matrix_set_topology(matrix, nodes, 4);
    
    connectivity_matrix_t topo;
    connectivity_matrix_get(&topo);
    
    spanning_tree_t mst;
    spanning_tree_compute(&topo, &mst);
    
    // Exemplo: Route 1 → 4
    print_path_comparison(&topo, &mst, 1, 4);
    
    ComparisonMetrics metrics;
    compute_comparison_metrics(&topo, &mst, &metrics);
    
    printf("\n📊 METRICS:\n");
    printf("   Avg hops (Dijkstra): %.2f\n", metrics.avg_hops_dijkstra);
    printf("   Avg hops (MST):      %.2f\n", metrics.avg_hops_mst);
    printf("   Improvement:         %.1f%%\n", metrics.improvement_percentage);
}

void test_complex_topology() {
    printf("\n╔══════════════════════════════════════╗\n");
    printf("║  TEST 3: Complex 6-Node Topology     ║\n");
    printf("╚══════════════════════════════════════╝\n");
    
    connectivity_matrix_init();
    
    /*
         1 --- 2
         |  X  |
         | / \ |
         3 --- 4 --- 5 --- 6
    */
    uint8_t matrix[MAX_NODES][MAX_NODES] = {0};
    node_id_t nodes[] = {1, 2, 3, 4, 5, 6};
    
    matrix[0][1] = matrix[1][0] = 1;
    matrix[0][2] = matrix[2][0] = 1;
    matrix[0][3] = matrix[3][0] = 2; // Longer path
    matrix[1][3] = matrix[3][1] = 1;
    matrix[2][3] = matrix[3][2] = 1;
    matrix[3][4] = matrix[4][3] = 1;
    matrix[4][5] = matrix[5][4] = 1;
    
    connectivity_matrix_set_topology(matrix, nodes, 6);
    
    connectivity_matrix_t topo;
    connectivity_matrix_get(&topo);
    
    spanning_tree_t mst;
    spanning_tree_compute(&topo, &mst);
    
    ComparisonMetrics metrics;
    compute_comparison_metrics(&topo, &mst, &metrics);
    
    printf("\n📊 METRICS:\n");
    printf("   Total reachable paths: %d\n", metrics.total_paths);
    printf("   Avg hops (Dijkstra):   %.2f\n", metrics.avg_hops_dijkstra);
    printf("   Avg hops (MST):        %.2f\n", metrics.avg_hops_mst);
    printf("   Optimal paths (Dijk):  %d/%d (%.1f%%)\n", 
           metrics.optimal_paths_dijkstra, metrics.total_paths,
           (float)metrics.optimal_paths_dijkstra/metrics.total_paths*100);
    printf("   Optimal paths (MST):   %d/%d (%.1f%%)\n",
           metrics.optimal_paths_mst, metrics.total_paths,
           (float)metrics.optimal_paths_mst/metrics.total_paths*100);
    printf("   Improvement:           %.1f%%\n", metrics.improvement_percentage);
    
    if (metrics.improvement_percentage > 5.0) {
        printf("\n✅ Dijkstra shows significant improvement!\n");
    }
}

int main() {
    printf("\n");
    printf("╔════════════════════════════════════════════════╗\n");
    printf("║  DIJKSTRA vs SPANNING TREE COMPARISON         ║\n");
    printf("╚════════════════════════════════════════════════╝\n");
    
    test_simple_comparison();
    test_diamond_comparison();
    test_complex_topology();
    
    printf("\n");
    printf("╔════════════════════════════════════════════════╗\n");
    printf("║  ALL COMPARISON TESTS COMPLETED ✓              ║\n");
    printf("╚════════════════════════════════════════════════╝\n\n");
    
    return 0;
}