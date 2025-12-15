// tests/test_topology.c
#include <stdio.h>
#include <assert.h>
#include "tdma_types.h"
#include "connectivity_matrix.h"
#include "spanning_tree.h"

void test_simple_line_topology(void) {
    printf("\n=== Test: Simple Line Topology ===\n");
    
    // Topology: 1 -- 2 -- 3 -- 4
    uint8_t matrix[MAX_NODES][MAX_NODES] = {0};
    matrix[0][1] = matrix[1][0] = 1;  // 1-2
    matrix[1][2] = matrix[2][1] = 1;  // 2-3
    matrix[2][3] = matrix[3][2] = 1;  // 3-4
    
    node_id_t nodes[] = {1, 2, 3, 4};
    
    connectivity_matrix_init();
    connectivity_matrix_set_topology(matrix, nodes, 4);
    connectivity_matrix_print();
    
    connectivity_matrix_t topo;
    connectivity_matrix_get(&topo);
    
    spanning_tree_t tree;
    spanning_tree_compute(&topo, &tree);
    spanning_tree_print(&tree);
    
    printf("✓ Test passed\n");
}

void test_diamond_topology(void) {
    printf("\n=== Test: Diamond Topology ===\n");
    
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
    connectivity_matrix_print();
    
    connectivity_matrix_t topo;
    connectivity_matrix_get(&topo);
    
    spanning_tree_t tree;
    spanning_tree_compute(&topo, &tree);
    spanning_tree_print(&tree);
    
    // Spanning tree should have 3 edges (4 nodes - 1)
    int edge_count = 0;
    for (int i = 0; i < 4; i++) {
        for (int j = i+1; j < 4; j++) {
            if (tree.tree[i][j]) edge_count++;
        }
    }
    assert(edge_count == 3);
    
    printf("✓ Test passed\n");
}

int main(void) {
    test_simple_line_topology();
    test_diamond_topology();
    
    printf("\n=== All tests passed ===\n");
    return 0;
}