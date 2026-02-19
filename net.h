#ifndef NET_H
#define NET_H

#include <stdint.h>

#define ETH_ADDR_LEN 6
#define IP_ADDR_LEN 4

#define ETH_TYPE_IP 0x0800
#define ETH_TYPE_ARP 0x0806

#define IP_PROTO_ICMP 1
#define IP_PROTO_TCP 6
#define IP_PROTO_UDP 17

typedef struct {
    uint8_t dest[ETH_ADDR_LEN];
    uint8_t src[ETH_ADDR_LEN];
    uint16_t type;
} eth_header_t;

typedef struct {
    uint8_t version_ihl;
    uint8_t tos;
    uint16_t total_length;
    uint16_t id;
    uint16_t flags_fragment;
    uint8_t ttl;
    uint8_t protocol;
    uint16_t checksum;
    uint8_t src[IP_ADDR_LEN];
    uint8_t dest[IP_ADDR_LEN];
} ip_header_t;

typedef struct {
    uint16_t src_port;
    uint16_t dest_port;
    uint32_t seq;
    uint32_t ack;
    uint16_t flags;
    uint16_t window;
    uint16_t checksum;
    uint16_t urgent;
} tcp_header_t;

typedef struct {
    uint16_t type;
    uint16_t code;
    uint16_t checksum;
    uint16_t id;
    uint16_t seq;
} icmp_header_t;

void net_init(void);
int net_send_packet(const void* data, uint32_t size);
int net_receive_packet(void* buffer, uint32_t max_size);
void net_set_mac(const uint8_t* mac);
void net_set_ip(const uint8_t* ip);
void net_process_packet(const void* data, uint32_t size);
void net_send_icmp_ping(uint8_t* dest_ip);

#endif