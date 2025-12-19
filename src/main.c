// src/main.c
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include "tdma_node.h"

// Variável global para controlar o loop principal
static volatile int keep_running = 1;

// Handler para CTRL+C
void handle_sigint(int dummy) {
    (void)dummy;
    keep_running = 0;
    printf("\n[MAIN] Caught SIGINT, stopping...\n");
}

int main(int argc, char *argv[]) {
    // ---------------------------------------------------------
    // FIX: Desativa buffer de saída para logs aparecerem logo
    // ---------------------------------------------------------
    setbuf(stdout, NULL);

    if (argc != 4) {
        fprintf(stderr, "Usage: %s <node_id> <total_nodes> <strategy>\n", argv[0]);
        fprintf(stderr, "Strategies: 0=Dijkstra, 1=MST, 2=Hybrid\n");
        return 1;
    }

    node_id_t my_id = atoi(argv[1]);
    int total_nodes = atoi(argv[2]);
    int strategy = atoi(argv[3]);

    // Configura o sinal para sair com CTRL+C
    signal(SIGINT, handle_sigint);

    printf("╔══════════════════════════════════════╗\n");
    printf("║  TDMA DAEMON - NODE %d                ║\n", my_id);
    printf("╚══════════════════════════════════════╝\n");

    tdma_node_t node;
    
    // 1. Inicializa
    if (tdma_node_init(&node, my_id, total_nodes, strategy) < 0) {
        fprintf(stderr, "[MAIN] Failed to initialize node\n");
        return 1;
    }

    // 2. Arranca as threads (RX e TX)
    tdma_node_start(&node);

    // 3. Loop Principal
    while (keep_running) {
        // Verifica se algum vizinho morreu (Timeout)
        tdma_node_check_timeouts(&node);
        
        // Dorme 100ms para não gastar 100% de CPU
        usleep(100000); 
    }

    // 4. Cleanup
    tdma_node_stop(&node);
    printf("[MAIN] Node %d exited cleanly.\n", my_id);
    
    return 0;
}