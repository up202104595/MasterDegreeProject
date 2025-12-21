#ifndef RA_TDMAS_SYNC_H
#define RA_TDMAS_SYNC_H

#include <stdint.h>
#include <stdbool.h>
#include <pthread.h>
#include "tdma_types.h"
#include "spanning_tree.h"

// TDMA Configuration
#define TDMA_ROUND_PERIOD_MS 100
#define MAX_SLOT_SHIFT_MS 6

// Packet timing info
typedef struct {
    node_id_t sender_id;
    uint64_t tx_timestamp_us;
    uint64_t rx_timestamp_us;
    int64_t delay_us;
} packet_timing_t;

// Delay buffer
typedef struct {
    int64_t delays[MAX_NODES];
    uint32_t count[MAX_NODES];
    pthread_mutex_t lock;
} delay_buffer_t;

// Slot boundaries (dynamic!)
typedef struct {
    node_id_t node_id;
    uint64_t start_offset_us;
    uint32_t duration_us;
    int32_t accumulated_shift_us;
} slot_boundary_t;

// RA-TDMAs+ Synchronizer
typedef struct {
    node_id_t my_node_id;
    uint8_t my_slot_index;
    uint8_t num_slots;
    
    uint32_t round_number;
    uint64_t round_start_us;
    uint32_t round_period_us;
    
    slot_boundary_t slots[MAX_NODES];
    
    delay_buffer_t current_delays;
    delay_buffer_t previous_delays;
    
    spanning_tree_t *mst;
    
    bool is_synchronized;
    uint32_t sync_rounds_count;
    
    pthread_mutex_t lock;
    
    uint64_t slot_adjustments;
    int64_t total_shift_applied_us;
} ra_tdmas_sync_t;

// API
int ra_tdmas_init(ra_tdmas_sync_t *sync, node_id_t my_id, 
                  node_id_t *all_nodes, uint8_t num_nodes);
void ra_tdmas_set_spanning_tree(ra_tdmas_sync_t *sync, spanning_tree_t *mst);
void ra_tdmas_on_packet_received(ra_tdmas_sync_t *sync, node_id_t sender_id,
                                 uint64_t tx_timestamp_us, uint64_t rx_timestamp_us);
void ra_tdmas_calculate_slot_adjustment(ra_tdmas_sync_t *sync);
bool ra_tdmas_can_transmit(ra_tdmas_sync_t *sync);
uint32_t ra_tdmas_time_until_my_slot_us(ra_tdmas_sync_t *sync);
uint64_t ra_tdmas_get_current_time_us(void);
void ra_tdmas_on_round_end(ra_tdmas_sync_t *sync);
void ra_tdmas_print_slot_boundaries(ra_tdmas_sync_t *sync);
void ra_tdmas_print_delays(ra_tdmas_sync_t *sync);

#endif