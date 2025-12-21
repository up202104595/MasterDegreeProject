#include "ra_tdmas_sync.h"
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

uint64_t ra_tdmas_get_current_time_us(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)(ts.tv_sec * 1000000ULL + ts.tv_nsec / 1000ULL);
}

int ra_tdmas_init(ra_tdmas_sync_t *sync, node_id_t my_id,
                  node_id_t *all_nodes, uint8_t num_nodes) {
    memset(sync, 0, sizeof(ra_tdmas_sync_t));
    
    sync->my_node_id = my_id;
    sync->num_slots = num_nodes;
    sync->round_period_us = TDMA_ROUND_PERIOD_MS * 1000;
    sync->round_start_us = ra_tdmas_get_current_time_us();
    sync->round_number = 0;
    sync->is_synchronized = false;
    
    pthread_mutex_init(&sync->lock, NULL);
    pthread_mutex_init(&sync->current_delays.lock, NULL);
    pthread_mutex_init(&sync->previous_delays.lock, NULL);
    
    uint32_t slot_duration = sync->round_period_us / num_nodes;
    
    for (int i = 0; i < num_nodes; i++) {
        sync->slots[i].node_id = all_nodes[i];
        sync->slots[i].start_offset_us = i * slot_duration;
        sync->slots[i].duration_us = slot_duration;
        sync->slots[i].accumulated_shift_us = 0;
        
        if (all_nodes[i] == my_id) {
            sync->my_slot_index = i;
        }
    }
    
    printf("[RA-TDMAs+] Init: Node %d, Slot %d/%d, Duration %u us\n",
           my_id, sync->my_slot_index, num_nodes, slot_duration);
    
    return 0;
}

void ra_tdmas_set_spanning_tree(ra_tdmas_sync_t *sync, spanning_tree_t *mst) {
    sync->mst = mst;
}

void ra_tdmas_on_packet_received(ra_tdmas_sync_t *sync, node_id_t sender_id,
                                 uint64_t tx_timestamp_us, uint64_t rx_timestamp_us) {
    int sender_idx = -1;
    for (int i = 0; i < sync->num_slots; i++) {
        if (sync->slots[i].node_id == sender_id) {
            sender_idx = i;
            break;
        }
    }
    
    if (sender_idx == -1) return;
    
    uint64_t sender_slot_start = sync->slots[sender_idx].start_offset_us;
    uint64_t expected_rx = sync->round_start_us + sender_slot_start + 
                          (tx_timestamp_us - sender_slot_start);
    
    int64_t raw_delay = (int64_t)rx_timestamp_us - (int64_t)expected_rx;
    int64_t half_period = sync->round_period_us / 2;
    int64_t delay = raw_delay;
    
    if (delay > half_period) {
        delay -= sync->round_period_us;
    } else if (delay < -half_period) {
        delay += sync->round_period_us;
    }
    
    pthread_mutex_lock(&sync->current_delays.lock);
    sync->current_delays.delays[sender_idx] = delay;
    sync->current_delays.count[sender_idx]++;
    pthread_mutex_unlock(&sync->current_delays.lock);
}

void ra_tdmas_calculate_slot_adjustment(ra_tdmas_sync_t *sync) {
    if (!sync->mst) return;
    
    pthread_mutex_lock(&sync->current_delays.lock);
    pthread_mutex_lock(&sync->previous_delays.lock);
    
    delay_buffer_t temp = sync->previous_delays;
    sync->previous_delays = sync->current_delays;
    sync->current_delays = temp;
    
    memset(&sync->current_delays.delays, 0, sizeof(sync->current_delays.delays));
    memset(&sync->current_delays.count, 0, sizeof(sync->current_delays.count));
    
    pthread_mutex_unlock(&sync->previous_delays.lock);
    pthread_mutex_unlock(&sync->current_delays.lock);
    
    int my_idx = sync->my_slot_index;
    int64_t filtered_delays[MAX_NODES];
    int valid_count = 0;
    
    for (int i = 0; i < sync->num_slots; i++) {
        if (sync->previous_delays.count[i] == 0) continue;
        if (sync->mst->tree[my_idx][i] == 0) continue;
        
        filtered_delays[valid_count++] = sync->previous_delays.delays[i];
    }
    
    if (valid_count == 0) return;
    
    // Sort for median
    for (int i = 0; i < valid_count - 1; i++) {
        for (int j = 0; j < valid_count - i - 1; j++) {
            if (filtered_delays[j] > filtered_delays[j+1]) {
                int64_t tmp = filtered_delays[j];
                filtered_delays[j] = filtered_delays[j+1];
                filtered_delays[j+1] = tmp;
            }
        }
    }
    
    int64_t median_delay = filtered_delays[valid_count / 2];
    int64_t shift = median_delay;
    
    if (shift < 0) shift = 0;
    
    int64_t max_shift = (MAX_SLOT_SHIFT_MS * 1000);
    if (shift > max_shift) shift = max_shift;
    
    if (shift > 0) {
        pthread_mutex_lock(&sync->lock);
        
        sync->slots[my_idx].start_offset_us += shift;
        sync->slots[my_idx].accumulated_shift_us += shift;
        
        if (sync->slots[my_idx].start_offset_us >= sync->round_period_us) {
            sync->slots[my_idx].start_offset_us -= sync->round_period_us;
        }
        
        pthread_mutex_unlock(&sync->lock);
        
        sync->slot_adjustments++;
        sync->total_shift_applied_us += shift;
        
        printf("[RA-TDMAs+] Node %d: Slot adjusted by %ld us (total: %d us)\n",
               sync->my_node_id, shift, sync->slots[my_idx].accumulated_shift_us);
    }
}

bool ra_tdmas_can_transmit(ra_tdmas_sync_t *sync) {
    uint64_t now = ra_tdmas_get_current_time_us();
    uint64_t time_in_round = (now - sync->round_start_us) % sync->round_period_us;
    
    pthread_mutex_lock(&sync->lock);
    slot_boundary_t *my_slot = &sync->slots[sync->my_slot_index];
    
    uint64_t slot_start = my_slot->start_offset_us;
    uint64_t slot_end = slot_start + my_slot->duration_us;
    
    bool can_tx = (time_in_round >= slot_start && time_in_round < slot_end);
    pthread_mutex_unlock(&sync->lock);
    
    return can_tx;
}

uint32_t ra_tdmas_time_until_my_slot_us(ra_tdmas_sync_t *sync) {
    uint64_t now = ra_tdmas_get_current_time_us();
    uint64_t time_in_round = (now - sync->round_start_us) % sync->round_period_us;
    
    pthread_mutex_lock(&sync->lock);
    uint64_t slot_start = sync->slots[sync->my_slot_index].start_offset_us;
    pthread_mutex_unlock(&sync->lock);
    
    if (time_in_round < slot_start) {
        return (uint32_t)(slot_start - time_in_round);
    } else {
        return (uint32_t)((sync->round_period_us - time_in_round) + slot_start);
    }
}

void ra_tdmas_on_round_end(ra_tdmas_sync_t *sync) {
    sync->round_number++;
    sync->round_start_us += sync->round_period_us;
    sync->sync_rounds_count++;
    
    if (!sync->is_synchronized && sync->sync_rounds_count >= 3) {
        sync->is_synchronized = true;
        printf("[RA-TDMAs+] Node %d: Synchronization achieved!\n", sync->my_node_id);
    }
}

void ra_tdmas_print_slot_boundaries(ra_tdmas_sync_t *sync) {
    printf("\n=== RA-TDMAs+ Slots (Node %d) ===\n", sync->my_node_id);
    printf("Round: %u | Synced: %s\n", sync->round_number, 
           sync->is_synchronized ? "YES" : "NO");
    printf("\nNode | Start (us) | Duration | Shift\n");
    printf("-----|------------|----------|-------\n");
    
    pthread_mutex_lock(&sync->lock);
    for (int i = 0; i < sync->num_slots; i++) {
        char marker = (i == sync->my_slot_index) ? '*' : ' ';
        printf(" %c%2d | %6lu | %6u | %6d\n", marker,
               sync->slots[i].node_id, sync->slots[i].start_offset_us,
               sync->slots[i].duration_us, sync->slots[i].accumulated_shift_us);
    }
    pthread_mutex_unlock(&sync->lock);
    printf("\n");
}

void ra_tdmas_print_delays(ra_tdmas_sync_t *sync) {
    printf("\n=== Delays (Node %d) ===\n", sync->my_node_id);
    pthread_mutex_lock(&sync->previous_delays.lock);
    for (int i = 0; i < sync->num_slots; i++) {
        if (sync->previous_delays.count[i] > 0) {
            printf("  Node %d: %ld us (%u pkts)\n", sync->slots[i].node_id,
                   sync->previous_delays.delays[i], sync->previous_delays.count[i]);
        }
    }
    pthread_mutex_unlock(&sync->previous_delays.lock);
    printf("\n");
}