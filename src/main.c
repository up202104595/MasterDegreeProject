// src/main.c
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <string.h>
#include "tdma_node.h"
#include "data_streaming.h"

static tdma_node_t node;
static volatile sig_atomic_t keep_running = 1;

void signal_handler(int signum) {
    (void)signum;
    printf("\n[MAIN] Caught SIGINT, stopping...\n");
    keep_running = 0;
}

// ========================================
// Thread de Streaming (Node 1 apenas)
// Simula vídeo 720p @ 30 FPS
// ========================================
void* streaming_test_thread(void* arg) {
    tdma_node_t *n = (tdma_node_t*)arg;
    
    // Espera rede estabilizar
    printf("[STREAMING-TEST] Waiting 30 seconds for network to stabilize...\n");
    sleep(30);
    
    printf("\n");
    printf("╔════════════════════════════════════════════════╗\n");
    printf("║  VIDEO STREAMING TEST                          ║\n");
    printf("║  Simulating 720p @ 30 FPS                      ║\n");
    printf("╚════════════════════════════════════════════════╝\n");
    printf("\n");
    printf("📹 Video Parameters:\n");
    printf("   Resolution:  1280x720\n");
    printf("   Frame rate:  30 FPS\n");
    printf("   Codec:       H.264 (simulated)\n");
    printf("   Bitrate:     ~2 Mbps\n");
    printf("   Frame size:  ~8 KB per frame\n");
    printf("\n");
    
    // Simulate 3 seconds of video = 90 frames
    const int FRAMES_TO_SEND = 90;  // 3 seconds @ 30 FPS
    const int FRAME_SIZE = 8192;    // 8 KB per frame (compressed H.264)
    const int FRAME_INTERVAL_MS = 33;  // 1000ms / 30fps ≈ 33ms
    
    int frames_sent = 0;
    int frames_failed = 0;
    uint64_t total_bytes = 0;
    
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);
    
    printf("[STREAMING-TEST] Starting transmission of %d frames...\n", FRAMES_TO_SEND);
    printf("\n");
    
    for (int i = 0; i < FRAMES_TO_SEND && keep_running; i++) {
        // Gerar frame realista
        uint8_t video_frame[FRAME_SIZE];
        int frame_size = generate_video_frame(video_frame, sizeof(video_frame));
        
        if (frame_size > 0) {
            // Enviar para Node 4
            int result = data_streaming_send(&n->streaming, 4, 
                                            video_frame, frame_size,
                                            STREAM_TYPE_VIDEO);
            
            if (result > 0) {
                frames_sent++;
                total_bytes += frame_size;
                
                // Progress indicator (every 30 frames = 1 second)
                if ((i + 1) % 30 == 0) {
                    printf("[STREAMING-TEST] Progress: %d/%d frames (%.1f seconds)\n",
                           i + 1, FRAMES_TO_SEND, (i + 1) / 30.0);
                }
            } else {
                frames_failed++;
                if (frames_failed < 5) {  // Only log first few failures
                    printf("[STREAMING-TEST] ⚠️  Frame %d failed to send\n", i + 1);
                }
            }
            
            // Simulate 30 FPS timing (33ms between frames)
            usleep(FRAME_INTERVAL_MS * 1000);
        }
    }
    
    clock_gettime(CLOCK_MONOTONIC, &end);
    
    // Calculate statistics
    double duration_sec = (end.tv_sec - start.tv_sec) + 
                         (end.tv_nsec - start.tv_nsec) / 1e9;
    double actual_bitrate = (total_bytes * 8) / (duration_sec * 1000000);  // Mbps
    
    printf("\n");
    printf("╔════════════════════════════════════════════════╗\n");
    printf("║  STREAMING TEST RESULTS                        ║\n");
    printf("╚════════════════════════════════════════════════╝\n");
    printf("\n");
    printf("📊 Transmission Statistics:\n");
    printf("   Frames sent:       %d / %d\n", frames_sent, FRAMES_TO_SEND);
    printf("   Frames failed:     %d\n", frames_failed);
    printf("   Success rate:      %.1f%%\n", 
           (frames_sent * 100.0) / FRAMES_TO_SEND);
    printf("   Total data:        %.2f KB\n", total_bytes / 1024.0);
    printf("   Duration:          %.2f seconds\n", duration_sec);
    printf("   Actual bitrate:    %.2f Mbps\n", actual_bitrate);
    printf("   Average FPS:       %.1f\n", frames_sent / duration_sec);
    printf("\n");
    
    if (frames_sent >= FRAMES_TO_SEND * 0.9) {  // 90% success
        printf("✅ Video streaming test PASSED!\n");
    } else {
        printf("⚠️  Video streaming test INCOMPLETE (check network)\n");
    }
    printf("\n");
    
    return NULL;
}

// ========================================
// Main Function
// ========================================
int main(int argc, char *argv[]) {
    // Desativa buffer para logs aparecerem imediatamente
    setbuf(stdout, NULL);
    
    if (argc < 4) {
        fprintf(stderr, "Usage: %s <node_id> <total_nodes> <strategy>\n", argv[0]);
        fprintf(stderr, "  node_id:      1-255\n");
        fprintf(stderr, "  total_nodes:  2-16\n");
        fprintf(stderr, "  strategy:     0=Dijkstra, 1=MST, 2=Hybrid\n");
        return 1;
    }
    
    node_id_t my_id = atoi(argv[1]);
    int total_nodes = atoi(argv[2]);
    routing_strategy_t strategy = atoi(argv[3]);
    
    if (my_id < 1 || my_id > 255) {
        fprintf(stderr, "Error: node_id must be 1-255\n");
        return 1;
    }
    
    if (total_nodes < 2 || total_nodes > MAX_NODES) {
        fprintf(stderr, "Error: total_nodes must be 2-%d\n", MAX_NODES);
        return 1;
    }
    
    if (strategy < 0 || strategy > 2) {
        fprintf(stderr, "Error: strategy must be 0-2\n");
        return 1;
    }
    
    // Banner
    printf("╔══════════════════════════════════════╗\n");
    printf("║  TDMA DAEMON - NODE %-2d               ║\n", my_id);
    printf("╚══════════════════════════════════════╝\n");
    
    // Setup signal handler
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    // Initialize node
    if (tdma_node_init(&node, my_id, total_nodes, strategy) < 0) {
        fprintf(stderr, "[MAIN] Failed to initialize node\n");
        return 1;
    }
    
    // Start node
    if (tdma_node_start(&node) < 0) {
        fprintf(stderr, "[MAIN] Failed to start node\n");
        tdma_node_destroy(&node);
        return 1;
    }
    
    // ============================================
    // Start streaming test thread (Node 1 only)
    // ============================================
    pthread_t streaming_thread;
    if (my_id == 1) {
        printf("[MAIN] Starting streaming test thread...\n");
        if (pthread_create(&streaming_thread, NULL, streaming_test_thread, &node) != 0) {
            fprintf(stderr, "[MAIN] Failed to start streaming thread\n");
        }
    }
    // ============================================
    
    // Main loop - print stats every 30 seconds
    int stats_interval = 30;
    int elapsed = 0;
    
    while (keep_running) {
        sleep(1);
        elapsed++;
        
        if (elapsed >= stats_interval) {
            printf("\n");
            printf("═══════════════════════════════════════════════\n");
            printf("  Node %d Status Report (Runtime: %d seconds)\n", my_id, elapsed);
            printf("═══════════════════════════════════════════════\n");
            
            // Heartbeat stats
            printf("\n📡 TDMA Stats:\n");
            printf("  Heartbeats sent:     %lu\n", node.heartbeats_sent);
            printf("  Heartbeats received: %lu\n", node.heartbeats_received);
            printf("  Round number:        %u\n", node.ra_sync.round_number);
            printf("  Synchronized:        %s\n", 
                   node.ra_sync.is_synchronized ? "YES" : "NO");
            
            // RA-TDMAs+ stats
            if (node.ra_sync.slot_adjustments > 0) {
                printf("\n⏱️  RA-TDMAs+ Sync:\n");
                printf("  Slot adjustments:    %lu\n", node.ra_sync.slot_adjustments);
                printf("  Total shift:         %ld μs\n", node.ra_sync.total_shift_applied_us);
            }
            
            // Routing stats
            printf("\n🗺️  Routing:\n");
            printf("  Topology version:    %lu\n", node.routing_mgr.topology_version);
            printf("  Recomputations:      %u\n", node.routing_mgr.recomputations);
            
            // Transport stats
            printf("\n📦 Transport:\n");
            printf("  Packets sent:        %lu\n", node.transport.packets_sent);
            printf("  Packets received:    %lu\n", node.transport.packets_received);
            
            printf("\n");
            
            elapsed = 0;
        }
    }
    
    // Wait for streaming thread to finish
    if (my_id == 1) {
        pthread_join(streaming_thread, NULL);
    }
    
    // Cleanup
    printf("\n[MAIN] Stopping node...\n");
    tdma_node_stop(&node);
    
    // Final statistics
    printf("\n");
    printf("╔════════════════════════════════════════════════╗\n");
    printf("║  FINAL STATISTICS - NODE %d                     \n", my_id);
    printf("╚════════════════════════════════════════════════╝\n");
    
    tdma_node_print_status(&node);
    
    tdma_node_destroy(&node);
    
    printf("\n[MAIN] Node %d exited cleanly.\n", my_id);
    return 0;
}