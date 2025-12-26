// src/network/data_streaming.c
#include "data_streaming.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <unistd.h>   // ← ADICIONAR para usleep()
#include <math.h>     // ← ADICIONAR para sin()

// ========================================
// Helper Functions
// ========================================

static uint64_t get_current_time_us(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint64_t)tv.tv_sec * 1000000 + tv.tv_usec;
}

static uint64_t get_current_time_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

// ========================================
// Video Frame Generation (Simulated)
// ========================================

int generate_video_frame(uint8_t *buffer, uint32_t size) {
    static uint32_t frame_counter = 0;
    
    if (size < 16) return -1;
    
    buffer[0] = 'V';
    buffer[1] = 'I';
    buffer[2] = 'D';
    buffer[3] = 0xFF;
    
    uint64_t timestamp = get_current_time_us();
    memcpy(buffer + 4, &timestamp, sizeof(timestamp));
    memcpy(buffer + 12, &frame_counter, sizeof(frame_counter));
    frame_counter++;
    
    srand(timestamp & 0xFFFFFFFF);
    for (uint32_t i = 16; i < size; i++) {
        buffer[i] = (uint8_t)(rand() & 0xFF);
    }
    
    return size;
}

int generate_audio_chunk(uint8_t *buffer, uint32_t size) {
    static uint32_t audio_counter = 0;
    
    if (size < 16) return -1;
    
    buffer[0] = 'A';
    buffer[1] = 'U';
    buffer[2] = 'D';
    buffer[3] = 0xFF;
    
    uint64_t timestamp = get_current_time_us();
    memcpy(buffer + 4, &timestamp, sizeof(timestamp));
    memcpy(buffer + 12, &audio_counter, sizeof(audio_counter));
    audio_counter++;
    
    for (uint32_t i = 16; i < size; i++) {
        buffer[i] = (uint8_t)(128 + 127 * sin(i * 0.1));
    }
    
    return size;
}

// ========================================
// Initialization
// ========================================

int data_streaming_init(data_streaming_t *stream,
                       node_id_t my_id,
                       udp_transport_t *transport) {
    memset(stream, 0, sizeof(data_streaming_t));
    
    stream->my_node_id = my_id;
    stream->transport = transport;
    stream->next_stream_id = 1;
    
    printf("[STREAMING] Initialized for node %d\n", my_id);
    return 0;
}

// ========================================
// Transmission (CORRIGIDO)
// ========================================

int data_streaming_send(data_streaming_t *stream,
                       node_id_t destination,
                       const uint8_t *data,
                       uint32_t size,
                       stream_type_t type) {
    
    if (size == 0 || size > MAX_STREAM_BUFFER) {
        fprintf(stderr, "[STREAMING] Invalid size: %u\n", size);
        return -1;
    }
    
    memset(&stream->tx_stats, 0, sizeof(stream_stats_t));
    stream->tx_stats.stream_id = stream->next_stream_id++;
    stream->tx_stats.total_bytes = size;
    stream->tx_stats.start_time_ms = get_current_time_ms();
    
    uint32_t chunk_size = MAX_CHUNK_SIZE - sizeof(stream_header_t);
    uint32_t total_chunks = (size + chunk_size - 1) / chunk_size;
    
    printf("[STREAMING] Sending stream %u: %u bytes in %u chunks to node %d\n",
           stream->tx_stats.stream_id, size, total_chunks, destination);
    
    const char *type_str = type == STREAM_TYPE_VIDEO ? "VIDEO" :
                          type == STREAM_TYPE_AUDIO ? "AUDIO" : "DATA";
    printf("[STREAMING] Type: %s\n", type_str);
    
    uint32_t offset = 0;
    for (uint32_t seq = 0; seq < total_chunks; seq++) {
        uint32_t remaining = size - offset;
        uint32_t this_chunk_size = remaining < chunk_size ? remaining : chunk_size;
        
        // Build packet
        uint8_t packet[MAX_PACKET_SIZE];
        stream_header_t *header = (stream_header_t *)packet;
        
        header->stream_id = stream->tx_stats.stream_id;
        header->sequence_number = seq;
        header->total_chunks = total_chunks;
        header->chunk_size = this_chunk_size;
        header->type = type;
        header->timestamp_us = get_current_time_us();
        
        // Copy payload
        memcpy(packet + sizeof(stream_header_t), data + offset, this_chunk_size);
        
        // ============================================
        // CORRIGIDO: udp_transport_send()
        // ============================================
        uint16_t packet_len = sizeof(stream_header_t) + this_chunk_size;
        
        int sent = udp_transport_send(stream->transport,
                                     destination,           // dst_node
                                     MSG_DATA,             // msg_type
                                     packet,               // payload
                                     packet_len,           // payload_len
                                     header->timestamp_us); // tx_timestamp_us
        // ============================================
        
        if (sent > 0) {
            stream->tx_stats.chunks_sent++;
        } else {
            fprintf(stderr, "[STREAMING] Failed to send chunk %u/%u\n",
                   seq, total_chunks);
        }
        
        offset += this_chunk_size;
        
        if (total_chunks >= 10 && (seq + 1) % (total_chunks / 10) == 0) {
            printf("[STREAMING] Progress: %u%%\n", 
                   (uint32_t)(((seq + 1) * 100) / total_chunks));
        }
        
        usleep(500);
    }
    
    stream->tx_stats.end_time_ms = get_current_time_ms();
    
    uint64_t duration_ms = stream->tx_stats.end_time_ms - 
                          stream->tx_stats.start_time_ms;
    
    if (duration_ms > 0) {
        double duration_sec = duration_ms / 1000.0;
        double bytes_sent = stream->tx_stats.chunks_sent * 
                           (chunk_size + sizeof(stream_header_t));
        stream->tx_stats.throughput_mbps = (bytes_sent * 8) / 
                                           (duration_sec * 1000000);
    }
    
    printf("[STREAMING] Stream %u complete: %u/%u chunks sent in %lu ms\n",
           stream->tx_stats.stream_id,
           stream->tx_stats.chunks_sent,
           total_chunks,
           duration_ms);
    
    printf("[STREAMING] Throughput: %.2f Mbps\n", 
           stream->tx_stats.throughput_mbps);
    
    return stream->tx_stats.chunks_sent;
}

// ========================================
// Reception
// ========================================

int data_streaming_receive(data_streaming_t *stream,
                          uint8_t *buffer,
                          uint32_t buffer_size) {
    
    if (buffer_size < sizeof(stream_header_t)) {
        return -1;
    }
    
    stream_header_t *header = (stream_header_t *)buffer;
    
    if (header->sequence_number == 0) {
        memset(&stream->rx_stats, 0, sizeof(stream_stats_t));
        stream->rx_stats.stream_id = header->stream_id;
        stream->rx_stats.start_time_ms = get_current_time_ms();
        stream->rx_bytes_received = 0;
        
        const char *type_str = header->type == STREAM_TYPE_VIDEO ? "VIDEO" :
                              header->type == STREAM_TYPE_AUDIO ? "AUDIO" : "DATA";
        
        printf("[STREAMING] Receiving stream %u: %u chunks (%s)\n",
               header->stream_id, header->total_chunks, type_str);
    }
    
    if (header->stream_id != stream->rx_stats.stream_id) {
        fprintf(stderr, "[STREAMING] Stream ID mismatch! Expected %u, got %u\n",
               stream->rx_stats.stream_id, header->stream_id);
        return -1;
    }
    
    if (stream->rx_bytes_received + header->chunk_size <= MAX_STREAM_BUFFER) {
        memcpy(stream->rx_buffer + stream->rx_bytes_received,
               buffer + sizeof(stream_header_t),
               header->chunk_size);
        stream->rx_bytes_received += header->chunk_size;
    }
    
    stream->rx_stats.chunks_received++;
    
    if (stream->rx_stats.chunks_received >= header->total_chunks) {
        stream->rx_stats.end_time_ms = get_current_time_ms();
        stream->rx_stats.total_bytes = stream->rx_bytes_received;
        stream->rx_stats.chunks_lost = header->total_chunks - 
                                       stream->rx_stats.chunks_received;
        
        uint64_t duration_ms = stream->rx_stats.end_time_ms - 
                              stream->rx_stats.start_time_ms;
        
        if (duration_ms > 0) {
            double duration_sec = duration_ms / 1000.0;
            stream->rx_stats.throughput_mbps = 
                (stream->rx_stats.total_bytes * 8) / (duration_sec * 1000000);
        }
        
        double loss_rate = (stream->rx_stats.chunks_lost * 100.0) / 
                          header->total_chunks;
        
        printf("[STREAMING] Stream %u complete: %u bytes received in %lu ms\n",
               header->stream_id,
               stream->rx_stats.total_bytes,
               duration_ms);
        printf("[STREAMING] Throughput: %.2f Mbps | Loss: %.2f%%\n",
               stream->rx_stats.throughput_mbps, loss_rate);
        
        if (stream->rx_bytes_received >= 4) {
            if (header->type == STREAM_TYPE_VIDEO &&
                stream->rx_buffer[0] == 'V' &&
                stream->rx_buffer[1] == 'I' &&
                stream->rx_buffer[2] == 'D') {
                printf("[STREAMING] ✅ Video frame integrity verified\n");
            } else if (header->type == STREAM_TYPE_AUDIO &&
                      stream->rx_buffer[0] == 'A' &&
                      stream->rx_buffer[1] == 'U' &&
                      stream->rx_buffer[2] == 'D') {
                printf("[STREAMING] ✅ Audio chunk integrity verified\n");
            }
        }
        
        return stream->rx_stats.total_bytes;
    }
    
    if (header->total_chunks >= 10 && 
        stream->rx_stats.chunks_received % (header->total_chunks / 10) == 0) {
        printf("[STREAMING] RX Progress: %u%%\n",
               (uint32_t)((stream->rx_stats.chunks_received * 100) / 
                         header->total_chunks));
    }
    
    return 0;
}

// ========================================
// Statistics
// ========================================

void data_streaming_print_stats(data_streaming_t *stream) {
    printf("\n╔════════════════════════════════════════════════╗\n");
    printf("║  DATA STREAMING STATISTICS                     ║\n");
    printf("╚════════════════════════════════════════════════╝\n\n");
    
    printf("📤 TX Statistics:\n");
    printf("   Stream ID:     %u\n", stream->tx_stats.stream_id);
    printf("   Total Bytes:   %u\n", stream->tx_stats.total_bytes);
    printf("   Chunks Sent:   %u\n", stream->tx_stats.chunks_sent);
    printf("   Duration:      %lu ms\n",
           stream->tx_stats.end_time_ms - stream->tx_stats.start_time_ms);
    printf("   Throughput:    %.2f Mbps\n", stream->tx_stats.throughput_mbps);
    
    printf("\n📥 RX Statistics:\n");
    printf("   Stream ID:     %u\n", stream->rx_stats.stream_id);
    printf("   Total Bytes:   %u\n", stream->rx_stats.total_bytes);
    printf("   Chunks Recv:   %u\n", stream->rx_stats.chunks_received);
    printf("   Chunks Lost:   %u\n", stream->rx_stats.chunks_lost);
    printf("   Duration:      %lu ms\n",
           stream->rx_stats.end_time_ms - stream->rx_stats.start_time_ms);
    printf("   Throughput:    %.2f Mbps\n", stream->rx_stats.throughput_mbps);
    
    if (stream->rx_stats.chunks_received > 0) {
        double loss_rate = (stream->rx_stats.chunks_lost * 100.0) /
                          (stream->rx_stats.chunks_received + 
                           stream->rx_stats.chunks_lost);
        printf("   Loss Rate:     %.2f%%\n", loss_rate);
    }
    
    printf("\n");
}

void data_streaming_reset_stats(data_streaming_t *stream) {
    memset(&stream->tx_stats, 0, sizeof(stream_stats_t));
    memset(&stream->rx_stats, 0, sizeof(stream_stats_t));
    stream->rx_bytes_received = 0;
}