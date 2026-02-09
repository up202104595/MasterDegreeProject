// include/data_streaming.h
#ifndef DATA_STREAMING_H
#define DATA_STREAMING_H

#include "tdma_types.h"
#include "udp_transport.h"
#include <stdint.h>
#include <stdbool.h>

#define MAX_STREAM_BUFFER (1024 * 1024)  // 1 MB
#define MAX_CHUNK_SIZE 1400

typedef enum {
    STREAM_TYPE_VIDEO = 0,
    STREAM_TYPE_AUDIO = 1,
    STREAM_TYPE_DATA = 2
} stream_type_t;

// ✅ CORRIGIDO: Adicionar source e destination
typedef struct {
    uint32_t stream_id;
    uint32_t sequence_number;
    uint32_t total_chunks;
    uint32_t chunk_size;
    stream_type_t type;
    uint64_t timestamp_us;
    node_id_t source;       // ✅ NOVO!
    node_id_t destination;  // ✅ NOVO!
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
} stream_stats_t;

typedef struct {
    node_id_t my_node_id;
    udp_transport_t *transport;
    
    // TX state
    uint32_t next_stream_id;
    stream_stats_t tx_stats;
    
    // RX state
    uint8_t rx_buffer[MAX_STREAM_BUFFER];
    uint32_t rx_bytes_received;
    stream_stats_t rx_stats;
    
    // ✅ NOVO: Stats de relay
    uint32_t packets_relayed;
    uint32_t streams_received;
    uint32_t frames_received;
    uint64_t total_bytes;
} data_streaming_t;

int data_streaming_init(data_streaming_t *stream,
                       node_id_t my_id,
                       udp_transport_t *transport);

int data_streaming_send(data_streaming_t *stream,
                       node_id_t destination,
                       const uint8_t *data,
                       uint32_t size,
                       stream_type_t type);

int data_streaming_receive(data_streaming_t *stream,
                          uint8_t *buffer,
                          uint32_t buffer_size);

void data_streaming_print_stats(data_streaming_t *stream);
void data_streaming_reset_stats(data_streaming_t *stream);

// Video frame generation (simulated)
int generate_video_frame(uint8_t *buffer, uint32_t size);
int generate_audio_chunk(uint8_t *buffer, uint32_t size);

#endif