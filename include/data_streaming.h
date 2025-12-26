// include/data_streaming.h
#ifndef DATA_STREAMING_H
#define DATA_STREAMING_H

#include <stdint.h>
#include <stdbool.h>
#include "udp_transport.h"

#define MAX_CHUNK_SIZE 1400  // MTU safe
#define MAX_STREAM_BUFFER (1024 * 1024)  // 1MB

typedef enum {
    STREAM_TYPE_VIDEO = 1,
    STREAM_TYPE_AUDIO = 2,
    STREAM_TYPE_DATA  = 3
} stream_type_t;

typedef struct {
    uint32_t stream_id;
    uint32_t sequence_number;
    uint32_t total_chunks;
    uint16_t chunk_size;
    stream_type_t type;
    uint64_t timestamp_us;
} __attribute__((packed)) stream_header_t;

typedef struct {
    uint32_t stream_id;
    uint32_t total_bytes;
    uint32_t chunks_sent;
    uint32_t chunks_received;
    uint32_t chunks_lost;
    uint64_t start_time_ms;
    uint64_t end_time_ms;
    double throughput_mbps;
    double avg_latency_ms;
} stream_stats_t;

typedef struct {
    node_id_t my_node_id;
    udp_transport_t *transport;
    
    // TX state
    uint32_t next_stream_id;
    uint8_t tx_buffer[MAX_STREAM_BUFFER];
    
    // RX state
    uint8_t rx_buffer[MAX_STREAM_BUFFER];
    uint32_t rx_bytes_received;
    
    // Stats
    stream_stats_t tx_stats;
    stream_stats_t rx_stats;
    
} data_streaming_t;

// Init
int data_streaming_init(data_streaming_t *stream, 
                       node_id_t my_id,
                       udp_transport_t *transport);

// TX
int data_streaming_send(data_streaming_t *stream,
                       node_id_t destination,
                       const uint8_t *data,
                       uint32_t size,
                       stream_type_t type);

// RX
int data_streaming_receive(data_streaming_t *stream,
                          uint8_t *buffer,
                          uint32_t buffer_size);

// Stats
void data_streaming_print_stats(data_streaming_t *stream);
void data_streaming_reset_stats(data_streaming_t *stream);

// Video simulation
int generate_video_frame(uint8_t *buffer, uint32_t size);

#endif // DATA_STREAMING_H