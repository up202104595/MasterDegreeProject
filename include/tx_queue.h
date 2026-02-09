// include/tx_queue.h
// TX Queue - Thread-safe producer-consumer queue for data packets

#ifndef TX_QUEUE_H
#define TX_QUEUE_H

#include <stdint.h>
#include <stdbool.h>
#include <pthread.h>

#define TX_QUEUE_SIZE 1000
#define MAX_PACKET_SIZE 1500

// Packet structure in queue
typedef struct {
    uint32_t dst_ip;                    // Destination IP
    uint8_t data[MAX_PACKET_SIZE];      // Packet data
    int length;                         // Data length
    uint64_t timestamp_ms;              // When queued (for latency stats)
} tx_queue_packet_t;

// Thread-safe circular queue
typedef struct {
    tx_queue_packet_t packets[TX_QUEUE_SIZE];
    int head;                           // Next to dequeue
    int tail;                           // Next to enqueue
    int count;                          // Number of packets
    pthread_mutex_t lock;
    
    // Statistics
    uint64_t packets_queued;
    uint64_t packets_dequeued;
    uint64_t packets_dropped;           // Drop-head when full
} tx_queue_t;

// Functions
void tx_queue_init(tx_queue_t *queue);
void tx_queue_destroy(tx_queue_t *queue);

// Add packet (drop-head if full)
bool tx_queue_enqueue(tx_queue_t *queue,
                     uint32_t dst_ip,
                     const uint8_t *data,
                     int length);

// Remove packet (returns length, or -1 if empty)
int tx_queue_dequeue(tx_queue_t *queue,
                    uint32_t *dst_ip,
                    uint8_t *data_out,
                    int max_length);

// Check if empty (thread-safe)
bool tx_queue_is_empty(tx_queue_t *queue);

// Get current count (thread-safe)
int tx_queue_get_count(tx_queue_t *queue);

// Statistics
void tx_queue_print_stats(tx_queue_t *queue);

#endif // TX_QUEUE_H
