#ifndef RA_TDMAS_SYNC_H
#define RA_TDMAS_SYNC_H

#include <stdint.h>
#include <stdbool.h>
#include <pthread.h>
#include "tdma_types.h"     // Certifica-te que tens node_id_t definido aqui
#include "spanning_tree.h"  // Certifica-te que tens a estrutura MST definida aqui

// --- CONFIGURAÇÃO TDMA ---
#define TDMA_ROUND_PERIOD_MS 100  // Duração total de uma ronda (100ms)
#define MAX_SLOT_SHIFT_MS 6       // Limite máximo de correção por ronda

// Estrutura para guardar tempos de pacotes recebidos
typedef struct {
    node_id_t sender_id;
    uint64_t tx_timestamp_us;
    uint64_t rx_timestamp_us;
    int64_t delay_us;
} packet_timing_t;

// Buffer para cálculo de média de atrasos
typedef struct {
    int64_t delays[MAX_NODES];
    uint32_t count[MAX_NODES];
    pthread_mutex_t lock;
} delay_buffer_t;

// Limites do Slot (Início e Duração)
typedef struct {
    node_id_t node_id;
    uint64_t start_offset_us;
    uint32_t duration_us;
    int32_t accumulated_shift_us; // Para debug: quanto já corrigimos
} slot_boundary_t;

// --- ESTRUTURA PRINCIPAL DE SINCRONIZAÇÃO ---
typedef struct {
    node_id_t my_node_id;
    uint8_t my_slot_index;
    uint8_t num_slots;
    
    uint32_t round_number;
    uint64_t round_start_us;    // O "Zero" desta ronda
    uint32_t round_period_us;   // 100ms em microsegundos
    
    slot_boundary_t slots[MAX_NODES];
    
    delay_buffer_t current_delays;
    delay_buffer_t previous_delays;
    
    spanning_tree_t *mst; // Ponteiro para a árvore (para saber quem ouvir)
    
    bool is_synchronized;
    uint32_t sync_rounds_count;
    
    pthread_mutex_t lock;
    
    // Estatísticas
    uint64_t slot_adjustments;
    int64_t total_shift_applied_us;
} ra_tdmas_sync_t;

// --- API PÚBLICA ---

// Inicializa a estrutura
int ra_tdmas_init(ra_tdmas_sync_t *sync, node_id_t my_id, 
                  node_id_t *all_nodes, uint8_t num_nodes);

// Atualiza o ponteiro para a Spanning Tree (para filtrar vizinhos)
void ra_tdmas_set_spanning_tree(ra_tdmas_sync_t *sync, spanning_tree_t *mst);

// Chama isto sempre que receberes um pacote (para medir o atraso)
void ra_tdmas_on_packet_received(ra_tdmas_sync_t *sync, node_id_t sender_id,
                                 uint64_t tx_timestamp_us, uint64_t rx_timestamp_us);

// Calcula e aplica correções ao relógio
void ra_tdmas_calculate_slot_adjustment(ra_tdmas_sync_t *sync);

// PERGUNTA CRÍTICA: Posso enviar agora? (Retorna true/false)
bool ra_tdmas_can_transmit(ra_tdmas_sync_t *sync);

// Quanto tempo falta para a minha vez? (Para usleep)
uint32_t ra_tdmas_time_until_my_slot_us(ra_tdmas_sync_t *sync);

// Utilitário de tempo
uint64_t ra_tdmas_get_current_time_us(void);

// Chama no fim de cada ciclo do loop principal
void ra_tdmas_on_round_end(ra_tdmas_sync_t *sync);

// Funções de Debug
void ra_tdmas_print_slot_boundaries(ra_tdmas_sync_t *sync);
void ra_tdmas_print_delays(ra_tdmas_sync_t *sync);

#endif