// include/udp_transport.h
#ifndef UDP_TRANSPORT_H
#define UDP_TRANSPORT_H

#include <stdint.h>
#include <stdbool.h>
#include <netinet/in.h>
#include "tdma_types.h"

#define UDP_PORT_BASE 5000
#define MAX_PACKET_SIZE 1500

// Tipos de mensagens
typedef enum {
    MSG_HEARTBEAT = 1,      // Keep-alive entre nós
    MSG_TOPOLOGY_UPDATE,    // Atualização da connectivity matrix
    MSG_DATA,              // Dados de aplicação
    MSG_ROUTING_REQUEST,   // Pedido de rota
    MSG_ROUTING_RESPONSE   // Resposta com next hop
} message_type_t;

// Header de mensagem UDP
typedef struct __attribute__((packed)) {
    uint8_t version;           // Versão do protocolo
    message_type_t type;       // Tipo de mensagem
    node_id_t src;            // Nó origem
    node_id_t dst;            // Nó destino
    uint16_t sequence;        // Número de sequência
    uint16_t payload_len;     // Tamanho do payload
} udp_header_t;

// Estrutura de transporte UDP
typedef struct {
    int socket_fd;
    uint16_t port;
    node_id_t my_node_id;
    
    // Estatísticas
    uint64_t packets_sent;
    uint64_t packets_received;
    uint64_t bytes_sent;
    uint64_t bytes_received;
    uint64_t errors;
} udp_transport_t;

// ========================================
// API de Transporte
// ========================================

// Inicializa transporte UDP
int udp_transport_init(udp_transport_t *transport, node_id_t my_id);

// Envia mensagem para um nó específico
int udp_transport_send(udp_transport_t *transport,
                      node_id_t dst_node,
                      message_type_t msg_type,
                      const void *payload,
                      uint16_t payload_len);

// Recebe mensagem (blocking ou non-blocking)
int udp_transport_receive(udp_transport_t *transport,
                         udp_header_t *header,
                         void *payload,
                         uint16_t max_payload_len,
                         bool blocking);

// Broadcast para todos os nós
int udp_transport_broadcast(udp_transport_t *transport,
                           message_type_t msg_type,
                           const void *payload,
                           uint16_t payload_len,
                           int num_nodes);

// Imprime estatísticas
void udp_transport_print_stats(udp_transport_t *transport);

// Cleanup
void udp_transport_destroy(udp_transport_t *transport);

// ========================================
// Funções Helper
// ========================================

// Converte node_id para IP (192.168.2.11, 192.168.2.12, etc.)
void node_id_to_ip(node_id_t node_id, char *ip_str, size_t len);

// Converte node_id para porta UDP
uint16_t node_id_to_port(node_id_t node_id);

#endif // UDP_TRANSPORT_H