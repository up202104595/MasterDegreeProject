// tests/test_dijkstra.c
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include "tdma_types.h"
#include "connectivity_matrix.h"
#include "dijkstra.h"

void test_dijkstra_line_topology(void) {
    printf("\n=== Test: Dijkstra on Line Topology ===\n");
    
    // Topology: 1 -- 2 -- 3 -- 4
    uint8_t matrix[MAX_NODES][MAX_NODES] = {0};
    matrix[0][1] = matrix[1][0] = 1;
    matrix[1][2] = matrix[2][1] = 1;
    matrix[2][3] = matrix[3][2] = 1;
    
    node_id_t nodes[] = {1, 2, 3, 4};
    
    connectivity_matrix_init();
    connectivity_matrix_set_topology(matrix, nodes, 4);
    
    connectivity_matrix_t topo;
    connectivity_matrix_get(&topo);
    
    // Run Dijkstra from node 1
    dijkstra_result_t results[MAX_NODES] = {0};
    int ret = dijkstra_compute(1, &topo, results);
    assert(ret == 0);
    
    dijkstra_print_results(1, results, 4);
    
    // Verify results
    // Node 1 -> 2: next_hop=2, distance=1
    assert(results[1].destination == 2);
    assert(results[1].next_hop == 2);
    assert(results[1].distance == 1);
    assert(results[1].reachable == true);
    
    // Node 1 -> 3: next_hop=2, distance=2
    assert(results[2].destination == 3);
    assert(results[2].next_hop == 2);
    assert(results[2].distance == 2);
    assert(results[2].reachable == true);
    
    // Node 1 -> 4: next_hop=2, distance=3
    assert(results[3].destination == 4);
    assert(results[3].next_hop == 2);
    assert(results[3].distance == 3);
    assert(results[3].reachable == true);
    
    printf("✓ Test passed\n");
}

void test_dijkstra_diamond_topology(void) {
    printf("\n=== Test: Dijkstra on Diamond Topology ===\n");
    
    // Topology:
    //     1
    //    / \
    //   2   3
    //    \ /
    //     4
    
    uint8_t matrix[MAX_NODES][MAX_NODES] = {0};
    matrix[0][1] = matrix[1][0] = 1;  // 1-2
    matrix[0][2] = matrix[2][0] = 1;  // 1-3
    matrix[1][3] = matrix[3][1] = 1;  // 2-4
    matrix[2][3] = matrix[3][2] = 1;  // 3-4
    
    node_id_t nodes[] = {1, 2, 3, 4};
    
    connectivity_matrix_set_topology(matrix, nodes, 4);
    
    connectivity_matrix_t topo;
    connectivity_matrix_get(&topo);
    
    // Run Dijkstra from node 1
    dijkstra_result_t results[MAX_NODES] = {0};
    int ret = dijkstra_compute(1, &topo, results);
    assert(ret == 0);
    
    dijkstra_print_results(1, results, 4);
    
    // Verify: 1 -> 4 should be 2 hops (via 2 or 3)
    assert(results[3].destination == 4);
    assert(results[3].distance == 2);
    assert(results[3].reachable == true);
    assert(results[3].next_hop == 2 || results[3].next_hop == 3);
    
    printf("✓ Test passed\n");
}

void test_dijkstra_link_failure(void) {
    printf("\n=== Test: Dijkstra with Link Failure ===\n");
    
    // Initial: Diamond topology
    uint8_t matrix[MAX_NODES][MAX_NODES] = {0};
    matrix[0][1] = matrix[1][0] = 1;  // 1-2
    matrix[0][2] = matrix[2][0] = 1;  // 1-3
    matrix[1][3] = matrix[3][1] = 1;  // 2-4
    matrix[2][3] = matrix[3][2] = 1;  // 3-4
    
    node_id_t nodes[] = {1, 2, 3, 4};
    
    connectivity_matrix_set_topology(matrix, nodes, 4);
    connectivity_matrix_t topo;
    connectivity_matrix_get(&topo);
    
    // Initial paths
    dijkstra_result_t results_before[MAX_NODES] = {0};
    dijkstra_compute(1, &topo, results_before);
    
    printf("BEFORE link failure:\n");
    dijkstra_print_results(1, results_before, 4);
    
    // Simulate link failure: 1-2
    printf("\n[FAILURE] Link 1-2 breaks\n\n");
    matrix[0][1] = matrix[1][0] = 0;
    connectivity_matrix_set_topology(matrix, nodes, 4);
    connectivity_matrix_get(&topo);
    
    // Recompute paths
    dijkstra_result_t results_after[MAX_NODES] = {0};
    dijkstra_compute(1, &topo, results_after);
    
    printf("AFTER link failure:\n");
    dijkstra_print_results(1, results_after, 4);
    
    // Verify: should still reach node 4 via node 3
    assert(results_after[3].destination == 4);
    assert(results_after[3].distance == 2);
    assert(results_after[3].reachable == true);
    assert(results_after[3].next_hop == 3);
    
    // Verify: path to node 4 changed
    assert(dijkstra_path_changed(&results_before[3], &results_after[3]));
    
    printf("✓ Test passed - Alternative path found!\n");
}

void test_dijkstra_disconnected(void) {
    printf("\n=== Test: Dijkstra with Disconnected Nodes ===\n");
    
    // Topology: 1 -- 2    3 -- 4  (two islands)
    uint8_t matrix[MAX_NODES][MAX_NODES] = {0};
    matrix[0][1] = matrix[1][0] = 1;  // 1-2
    matrix[2][3] = matrix[3][2] = 1;  // 3-4
    
    node_id_t nodes[] = {1, 2, 3, 4};
    
    connectivity_matrix_set_topology(matrix, nodes, 4);
    connectivity_matrix_t topo;
    connectivity_matrix_get(&topo);
    
    dijkstra_result_t results[MAX_NODES] = {0};
    dijkstra_compute(1, &topo, results);
    
    dijkstra_print_results(1, results, 4);
    
    // Node 1 can reach node 2
    assert(results[1].reachable == true);
    assert(results[1].distance == 1);
    
    // Node 1 CANNOT reach nodes 3 and 4
    assert(results[2].reachable == false);
    assert(results[3].reachable == false);
    
    printf("✓ Test passed - Correctly identified unreachable nodes\n");
}

int main(void) {
    test_dijkstra_line_topology();
    test_dijkstra_diamond_topology();
    test_dijkstra_link_failure();
    test_dijkstra_disconnected();
    
    printf("\n=== All Dijkstra tests passed ===\n");
    return 0;
}