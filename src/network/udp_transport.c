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

// ========================================
// Helper Functions
// ========================================

void node_id_to_ip(node_id_t node_id, char *ip_str, size_t len) {
    snprintf(ip_str, len, "192.168.2.%d", 10 + node_id);
}

uint16_t node_id_to_port(node_id_t node_id) {
    return UDP_PORT_BASE + node_id;
}

// ========================================
// Inicialização
// ========================================

int udp_transport_init(udp_transport_t *transport, node_id_t my_id) {
    memset(transport, 0, sizeof(udp_transport_t));
    
    transport->my_node_id = my_id;
    transport->port = node_id_to_port(my_id);
    
    // Cria socket UDP
    transport->socket_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (transport->socket_fd < 0) {
        perror("socket");
        return -1;
    }
    
    // Permite reutilização de endereço
    int opt = 1;
    if (setsockopt(transport->socket_fd, SOL_SOCKET, SO_REUSEADDR, 
                   &opt, sizeof(opt)) < 0) {
        perror("setsockopt");
        close(transport->socket_fd);
        return -1;
    }
    
    // Bind na porta
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(transport->port);
    
    if (bind(transport->socket_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind");
        close(transport->socket_fd);
        return -1;
    }
    
    printf("[TRANSPORT] Node %d listening on port %d\n", 
           my_id, transport->port);
    
    return 0;
}

// ========================================
// Envio de Mensagens
// ========================================

int udp_transport_send(udp_transport_t *transport,
                      node_id_t dst_node,
                      message_type_t msg_type,
                      const void *payload,
                      uint16_t payload_len) {
    // Monta header
    udp_header_t header;
    header.version = 1;
    header.type = msg_type;
    header.src = transport->my_node_id;
    header.dst = dst_node;
    header.sequence = transport->packets_sent & 0xFFFF;
    header.payload_len = payload_len;
    
    // Prepara buffer completo (header + payload)
    uint8_t buffer[MAX_PACKET_SIZE];
    memcpy(buffer, &header, sizeof(udp_header_t));
    if (payload && payload_len > 0) {
        memcpy(buffer + sizeof(udp_header_t), payload, payload_len);
    }
    
    size_t total_len = sizeof(udp_header_t) + payload_len;
    
    // Endereço de destino
    struct sockaddr_in dst_addr;
    memset(&dst_addr, 0, sizeof(dst_addr));
    dst_addr.sin_family = AF_INET;
    dst_addr.sin_port = htons(node_id_to_port(dst_node));
    
    char dst_ip[16];
    node_id_to_ip(dst_node, dst_ip, sizeof(dst_ip));
    inet_pton(AF_INET, dst_ip, &dst_addr.sin_addr);
    
    // Envia
    ssize_t sent = sendto(transport->socket_fd, buffer, total_len, 0,
                         (struct sockaddr*)&dst_addr, sizeof(dst_addr));
    
    if (sent < 0) {
        perror("sendto");
        transport->errors++;
        return -1;
    }
    
    transport->packets_sent++;
    transport->bytes_sent += sent;
    
    return sent;
}

// ========================================
// Recepção de Mensagens
// ========================================

int udp_transport_receive(udp_transport_t *transport,
                         udp_header_t *header,
                         void *payload,
                         uint16_t max_payload_len,
                         bool blocking) {
    // Configura socket como non-blocking se necessário
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
            return 0;  // No data available (non-blocking)
        }
        perror("recvfrom");
        transport->errors++;
        return -1;
    }
    
    // Parse header
    if (received < (ssize_t)sizeof(udp_header_t)) {
        fprintf(stderr, "[TRANSPORT] Packet too small\n");
        transport->errors++;
        return -1;
    }
    
    memcpy(header, buffer, sizeof(udp_header_t));
    
    // Parse payload
    uint16_t payload_len = header->payload_len;
    if (payload_len > 0 && payload != NULL) {
        if (payload_len > max_payload_len) {
            fprintf(stderr, "[TRANSPORT] Payload too large\n");
            transport->errors++;
            return -1;
        }
        memcpy(payload, buffer + sizeof(udp_header_t), payload_len);
    }
    
    transport->packets_received++;
    transport->bytes_received += received;
    
    return payload_len;
}

// ========================================
// Broadcast
// ========================================

int udp_transport_broadcast(udp_transport_t *transport,
                           message_type_t msg_type,
                           const void *payload,
                           uint16_t payload_len,
                           int num_nodes) {
    int sent_count = 0;
    
    for (int i = 1; i <= num_nodes; i++) {
        if (i == transport->my_node_id) continue;  // Skip self
        
        if (udp_transport_send(transport, i, msg_type, 
                              payload, payload_len) > 0) {
            sent_count++;
        }
    }
    
    return sent_count;
}

// ========================================
// Estatísticas
// ========================================

void udp_transport_print_stats(udp_transport_t *transport) {
    printf("\n=== UDP Transport Stats (Node %d) ===\n", transport->my_node_id);
    printf("Port:             %d\n", transport->port);
    printf("Packets sent:     %lu\n", transport->packets_sent);
    printf("Packets received: %lu\n", transport->packets_received);
    printf("Bytes sent:       %lu\n", transport->bytes_sent);
    printf("Bytes received:   %lu\n", transport->bytes_received);
    printf("Errors:           %lu\n", transport->errors);
    printf("\n");
}

// ========================================
// Cleanup
// ========================================

void udp_transport_destroy(udp_transport_t *transport) {
    if (transport->socket_fd >= 0) {
        close(transport->socket_fd);
        transport->socket_fd = -1;
    }
    printf("[TRANSPORT] Node %d transport destroyed\n", transport->my_node_id);
}