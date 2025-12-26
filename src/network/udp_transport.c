// src/network/udp_transport.c
#include "udp_transport.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <fcntl.h>

void node_id_to_ip(node_id_t node_id, char *ip_str, size_t len) {
    snprintf(ip_str, len, "192.168.2.%d", 10 + node_id);
}

uint16_t node_id_to_port(node_id_t node_id) {
    return UDP_PORT_BASE + node_id;
}

int udp_transport_init(udp_transport_t *transport, node_id_t my_id) {
    memset(transport, 0, sizeof(udp_transport_t));
    
    transport->my_node_id = my_id;
    transport->port = node_id_to_port(my_id);
    
    printf("[TRANSPORT-DEBUG] Initializing for node %d\n", my_id);
    
    // Create socket
    transport->socket_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (transport->socket_fd < 0) {
        perror("socket");
        return -1;
    }
    
    printf("[TRANSPORT-DEBUG] Socket created: fd=%d\n", transport->socket_fd);
    
    // Set socket options
    int opt = 1;
    if (setsockopt(transport->socket_fd, SOL_SOCKET, SO_REUSEADDR, 
                   &opt, sizeof(opt)) < 0) {
        perror("setsockopt SO_REUSEADDR");
        close(transport->socket_fd);
        return -1;
    }
    
    // Get my IP
    char my_ip[16];
    node_id_to_ip(my_id, my_ip, sizeof(my_ip));
    
    printf("[TRANSPORT-DEBUG] My IP: %s, Port: %d\n", my_ip, transport->port);
    
    // Bind to INADDR_ANY (aceita de qualquer interface)
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(transport->port);
    addr.sin_addr.s_addr = INADDR_ANY;  // ← MUDANÇA: INADDR_ANY funciona melhor
    
    if (bind(transport->socket_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        fprintf(stderr, "[TRANSPORT-DEBUG] bind() failed on 0.0.0.0:%d - %s\n",
               transport->port, strerror(errno));
        close(transport->socket_fd);
        return -1;
    }
    
    printf("[TRANSPORT] Node %d listening on 0.0.0.0:%d (accepting on %s)\n", 
           my_id, transport->port, my_ip);
    
    return 0;
}

int udp_transport_send(udp_transport_t *transport, node_id_t dst_node,
                      message_type_t msg_type, const void *payload,
                      uint16_t payload_len, uint64_t tx_timestamp_us) {
    
    // Get destination IP
    char dst_ip[16];
    node_id_to_ip(dst_node, dst_ip, sizeof(dst_ip));
    uint16_t dst_port = node_id_to_port(dst_node);
    
    // DEBUG: Print first send
    static bool first_send = true;
    if (first_send) {
        printf("[TRANSPORT-DEBUG] First send from node %d to node %d\n",
               transport->my_node_id, dst_node);
        printf("[TRANSPORT-DEBUG] Destination: %s:%d\n", dst_ip, dst_port);
        first_send = false;
    }
    
    // Build header
    udp_header_t header;
    header.version = 1;
    header.type = msg_type;
    header.src = transport->my_node_id;
    header.dst = dst_node;
    header.sequence = transport->packets_sent & 0xFFFF;
    header.payload_len = payload_len;
    header.tx_timestamp_us = tx_timestamp_us;
    
    // Build packet
    uint8_t buffer[MAX_PACKET_SIZE];
    memcpy(buffer, &header, sizeof(udp_header_t));
    if (payload && payload_len > 0) {
        if (payload_len > MAX_PACKET_SIZE - sizeof(udp_header_t)) {
            fprintf(stderr, "[TRANSPORT] Payload too large: %u\n", payload_len);
            return -1;
        }
        memcpy(buffer + sizeof(udp_header_t), payload, payload_len);
    }
    
    size_t total_len = sizeof(udp_header_t) + payload_len;
    
    // Build destination address
    struct sockaddr_in dst_addr;
    memset(&dst_addr, 0, sizeof(dst_addr));
    dst_addr.sin_family = AF_INET;
    dst_addr.sin_port = htons(dst_port);
    
    if (inet_pton(AF_INET, dst_ip, &dst_addr.sin_addr) <= 0) {
        fprintf(stderr, "[TRANSPORT] Invalid destination IP: %s\n", dst_ip);
        return -1;
    }
    
    // Send packet
    ssize_t sent = sendto(transport->socket_fd, buffer, total_len, 0,
                         (struct sockaddr*)&dst_addr, sizeof(dst_addr));
    
    if (sent < 0) {
        // Print error details on first failure
        static int error_count = 0;
        if (error_count < 5) {
            fprintf(stderr, "[TRANSPORT-DEBUG] sendto() failed:\n");
            fprintf(stderr, "  Socket: %d\n", transport->socket_fd);
            fprintf(stderr, "  Dest IP: %s\n", dst_ip);
            fprintf(stderr, "  Dest Port: %d\n", dst_port);
            fprintf(stderr, "  Error: %s (errno=%d)\n", strerror(errno), errno);
            error_count++;
        }
        
        transport->errors++;
        return -1;
    }
    
    transport->packets_sent++;
    transport->bytes_sent += sent;
    
    return sent;
}

int udp_transport_receive(udp_transport_t *transport, udp_header_t *header,
                         void *payload, uint16_t max_payload_len, bool blocking) {
    
    // Set non-blocking if requested
    if (!blocking) {
        int flags = fcntl(transport->socket_fd, F_GETFL, 0);
        fcntl(transport->socket_fd, F_SETFL, flags | O_NONBLOCK);
    }
    
    uint8_t buffer[MAX_PACKET_SIZE];
    struct sockaddr_in src_addr;
    socklen_t addr_len = sizeof(src_addr);
    
    ssize_t received = recvfrom(transport->socket_fd, buffer, sizeof(buffer), 0,
                               (struct sockaddr*)&src_addr, &addr_len);
    
    if (received < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return 0;  // No data available
        }
        perror("recvfrom");
        transport->errors++;
        return -1;
    }
    
    // DEBUG: Print first receive
    static bool first_recv = true;
    if (first_recv) {
        char src_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &src_addr.sin_addr, src_ip, sizeof(src_ip));
        printf("[TRANSPORT-DEBUG] First receive on node %d from %s:%d\n",
               transport->my_node_id, src_ip, ntohs(src_addr.sin_port));
        first_recv = false;
    }
    
    // Validate packet size
    if (received < (ssize_t)sizeof(udp_header_t)) {
        fprintf(stderr, "[TRANSPORT] Packet too small: %zd bytes\n", received);
        transport->errors++;
        return -1;
    }
    
    // Parse header
    memcpy(header, buffer, sizeof(udp_header_t));
    
    // Validate header
    if (header->version != 1) {
        fprintf(stderr, "[TRANSPORT] Invalid version: %u\n", header->version);
        transport->errors++;
        return -1;
    }
    
    // Copy payload if present
    uint16_t payload_len = header->payload_len;
    if (payload_len > 0 && payload != NULL) {
        if (payload_len > max_payload_len) {
            fprintf(stderr, "[TRANSPORT] Payload too large: %u > %u\n",
                   payload_len, max_payload_len);
            transport->errors++;
            return -1;
        }
        
        if (received < (ssize_t)(sizeof(udp_header_t) + payload_len)) {
            fprintf(stderr, "[TRANSPORT] Incomplete packet\n");
            transport->errors++;
            return -1;
        }
        
        memcpy(payload, buffer + sizeof(udp_header_t), payload_len);
    }
    
    transport->packets_received++;
    transport->bytes_received += received;
    
    return payload_len;
}

int udp_transport_broadcast(udp_transport_t *transport, message_type_t msg_type,
                           const void *payload, uint16_t payload_len,
                           int num_nodes, uint64_t tx_timestamp_us) {
    int sent_count = 0;
    
    for (int i = 1; i <= num_nodes; i++) {
        if (i == transport->my_node_id) continue;
        
        if (udp_transport_send(transport, i, msg_type, payload, payload_len,
                              tx_timestamp_us) > 0) {
            sent_count++;
        }
    }
    
    return sent_count;
}

void udp_transport_print_stats(udp_transport_t *transport) {
    printf("\n=== UDP Transport Stats (Node %d) ===\n", transport->my_node_id);
    printf("Port:         %d\n", transport->port);
    printf("Sent:         %lu packets, %lu bytes\n", 
           transport->packets_sent, transport->bytes_sent);
    printf("Received:     %lu packets, %lu bytes\n",
           transport->packets_received, transport->bytes_received);
    printf("Errors:       %lu\n", transport->errors);
    printf("\n");
}

void udp_transport_destroy(udp_transport_t *transport) {
    if (transport->socket_fd >= 0) {
        close(transport->socket_fd);
        transport->socket_fd = -1;
    }
    printf("[TRANSPORT] Node %d destroyed\n", transport->my_node_id);
}