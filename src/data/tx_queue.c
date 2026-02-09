// src/data/tx_queue.c
// TX Queue implementation - Thread-safe circular buffer with drop-head policy

#include "tx_queue.h"
#include <string.h>
#include <stdio.h>
#include <sys/time.h>

static uint64_t get_time_ms() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint64_t)tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

void tx_queue_init(tx_queue_t *queue) {
    memset(queue, 0, sizeof(tx_queue_t));
    pthread_mutex_init(&queue->lock, NULL);
    printf("[TX_QUEUE] Initialized (size=%d)\n", TX_QUEUE_SIZE);
}

void tx_queue_destroy(tx_queue_t *queue) {
    pthread_mutex_destroy(&queue->lock);
    printf("[TX_QUEUE] Destroyed (queued=%lu, dequeued=%lu, dropped=%lu)\n",
           queue->packets_queued, queue->packets_dequeued, queue->packets_dropped);
}

bool tx_queue_enqueue(tx_queue_t *queue,
                     uint32_t dst_ip,
                     const uint8_t *data,
                     int length) {
    if (length <= 0 || length > MAX_PACKET_SIZE) {
        return false;
    }
    
    pthread_mutex_lock(&queue->lock);
    
    // Queue full? DROP-HEAD (remove oldest packet)
    if (queue->count >= TX_QUEUE_SIZE) {
        queue->head = (queue->head + 1) % TX_QUEUE_SIZE;
        queue->count--;
        queue->packets_dropped++;
        
        // Log only occasionally
        if (queue->packets_dropped % 100 == 1) {
            printf("[TX_QUEUE] ⚠️  Full! Dropped %lu packets total\n", 
                   queue->packets_dropped);
        }
    }
    
    // Add new packet at tail
    tx_queue_packet_t *pkt = &queue->packets[queue->tail];
    pkt->dst_ip = dst_ip;
    memcpy(pkt->data, data, length);
    pkt->length = length;
    pkt->timestamp_ms = get_time_ms();
    
    queue->tail = (queue->tail + 1) % TX_QUEUE_SIZE;
    queue->count++;
    queue->packets_queued++;
    
    pthread_mutex_unlock(&queue->lock);
    return true;
}

int tx_queue_dequeue(tx_queue_t *queue,
                    uint32_t *dst_ip,
                    uint8_t *data_out,
                    int max_length) {
    pthread_mutex_lock(&queue->lock);
    
    // Queue empty?
    if (queue->count == 0) {
        pthread_mutex_unlock(&queue->lock);
        return -1;
    }
    
    // Get oldest packet
    tx_queue_packet_t *pkt = &queue->packets[queue->head];
    
    if (pkt->length > max_length) {
        pthread_mutex_unlock(&queue->lock);
        fprintf(stderr, "[TX_QUEUE] ERROR: Buffer too small (%d < %d)\n", 
                max_length, pkt->length);
        return -1;
    }
    
    *dst_ip = pkt->dst_ip;
    memcpy(data_out, pkt->data, pkt->length);
    int length = pkt->length;
    
    queue->head = (queue->head + 1) % TX_QUEUE_SIZE;
    queue->count--;
    queue->packets_dequeued++;
    
    pthread_mutex_unlock(&queue->lock);
    return length;
}

bool tx_queue_is_empty(tx_queue_t *queue) {
    pthread_mutex_lock(&queue->lock);
    bool empty = (queue->count == 0);
    pthread_mutex_unlock(&queue->lock);
    return empty;
}

int tx_queue_get_count(tx_queue_t *queue) {
    pthread_mutex_lock(&queue->lock);
    int count = queue->count;
    pthread_mutex_unlock(&queue->lock);
    return count;
}

void tx_queue_print_stats(tx_queue_t *queue) {
    pthread_mutex_lock(&queue->lock);
    printf("\n📦 TX Queue Statistics:\n");
    printf("   Current:  %d packets\n", queue->count);
    printf("   Queued:   %lu packets total\n", queue->packets_queued);
    printf("   Dequeued: %lu packets total\n", queue->packets_dequeued);
    printf("   Dropped:  %lu packets (%.2f%%)\n", 
           queue->packets_dropped,
           queue->packets_queued > 0 ? 
               (100.0 * queue->packets_dropped / queue->packets_queued) : 0.0);
    pthread_mutex_unlock(&queue->lock);
}
