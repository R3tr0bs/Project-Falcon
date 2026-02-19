#include "net.h"
#include "utils.h"
#include "vga.h"

static uint8_t mac_addr[ETH_ADDR_LEN] = {0x02, 0x00, 0x00, 0x00, 0x00, 0x01};
static uint8_t ip_addr[IP_ADDR_LEN] = {192, 168, 1, 100};
static uint8_t gateway_ip[IP_ADDR_LEN] = {192, 168, 1, 1};
static uint8_t subnet_mask[IP_ADDR_LEN] = {255, 255, 255, 0};

static uint16_t ip_id_counter = 1;
static uint16_t tcp_port_counter = 1024;

static uint16_t net_checksum(const void* data, uint32_t size) {
    const uint16_t* ptr = (const uint16_t*)data;
    uint32_t sum = 0;
    
    while (size > 1) {
        sum += *ptr++;
        size -= 2;
    }
    
    if (size > 0) {
        sum += *(const uint8_t*)ptr;
    }
    
    while (sum >> 16) {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }
    
    return ~sum;
}

void net_init(void) {
    ip_id_counter = 1;
    tcp_port_counter = 1024;
}

void net_set_mac(const uint8_t* mac) {
    for (int i = 0; i < ETH_ADDR_LEN; i++) {
        mac_addr[i] = mac[i];
    }
}

void net_set_ip(const uint8_t* ip) {
    for (int i = 0; i < IP_ADDR_LEN; i++) {
        ip_addr[i] = ip[i];
    }
}

static void net_send_ethernet(uint8_t* dest_mac, uint16_t type, const void* data, uint32_t size) {
    uint8_t packet[1518];
    eth_header_t* eth = (eth_header_t*)packet;
    
    for (int i = 0; i < ETH_ADDR_LEN; i++) {
        eth->dest[i] = dest_mac[i];
        eth->src[i] = mac_addr[i];
    }
    
    eth->type = (type >> 8) | (type << 8);
    
    uint8_t* payload = packet + sizeof(eth_header_t);
    const uint8_t* data_bytes = (const uint8_t*)data;
    
    for (uint32_t i = 0; i < size; i++) {
        payload[i] = data_bytes[i];
    }
    
    uint32_t total_size = sizeof(eth_header_t) + size;
    
    // Simulate sending packet (would interface with network driver)
    // For now, just log it
    print_str("NET: Sending packet, size=");
    print_dec(total_size);
    print_str(" bytes\n");
}

static void net_send_ip(uint8_t* dest_ip, uint8_t protocol, const void* data, uint32_t size) {
    uint8_t packet[1500];
    ip_header_t* ip = (ip_header_t*)packet;
    
    ip->version_ihl = 0x45;
    ip->tos = 0;
    ip->total_length = (sizeof(ip_header_t) + size) >> 8 | (sizeof(ip_header_t) + size) << 8;
    ip->id = ip_id_counter++;
    ip->flags_fragment = 0;
    ip->ttl = 64;
    ip->protocol = protocol;
    ip->checksum = 0;
    
    for (int i = 0; i < IP_ADDR_LEN; i++) {
        ip->src[i] = ip_addr[i];
        ip->dest[i] = dest_ip[i];
    }
    
    ip->checksum = net_checksum(ip, sizeof(ip_header_t));
    
    const uint8_t* data_bytes = (const uint8_t*)data;
    uint8_t* payload = packet + sizeof(ip_header_t);
    
    for (uint32_t i = 0; i < size; i++) {
        payload[i] = data_bytes[i];
    }
    
    // For now, broadcast MAC
    uint8_t broadcast_mac[ETH_ADDR_LEN] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    net_send_ethernet(broadcast_mac, ETH_TYPE_IP, packet, sizeof(ip_header_t) + size);
}

void net_send_icmp_ping(uint8_t* dest_ip) {
    uint8_t packet[64];
    icmp_header_t* icmp = (icmp_header_t*)packet;
    
    icmp->type = 8; // Echo Request
    icmp->code = 0;
    icmp->checksum = 0;
    icmp->id = 0x1234;
    icmp->seq = 1;
    
    // Fill with pattern
    for (int i = sizeof(icmp_header_t); i < 64; i++) {
        packet[i] = i & 0xFF;
    }
    
    icmp->checksum = net_checksum(packet, 64);
    
    net_send_ip(dest_ip, IP_PROTO_ICMP, packet, 64);
}

void net_process_packet(const void* data, uint32_t size) {
    if (size < sizeof(eth_header_t)) return;
    
    const eth_header_t* eth = (const eth_header_t*)data;
    uint16_t type = (eth->type >> 8) | (eth->type << 8);
    
    if (type == ETH_TYPE_IP) {
        if (size < sizeof(eth_header_t) + sizeof(ip_header_t)) return;
        
        const ip_header_t* ip = (const ip_header_t*)((const uint8_t*)data + sizeof(eth_header_t));
        
        // Check if packet is for us
        int for_us = 1;
        for (int i = 0; i < IP_ADDR_LEN; i++) {
            if (ip->dest[i] != ip_addr[i]) {
                for_us = 0;
                break;
            }
        }
        
        if (for_us) {
            print_str("NET: Received IP packet for us\n");
            
            if (ip->protocol == IP_PROTO_ICMP) {
                print_str("NET: ICMP packet received\n");
            }
        }
    }
}

int net_send_packet(const void* data, uint32_t size) {
    // For now, just simulate sending
    print_str("NET: Simulating packet send, size=");
    print_dec(size);
    print_str(" bytes\n");
    return 0;
}

int net_receive_packet(void* buffer, uint32_t max_size) {
    // For now, simulate receiving a ping request
    uint8_t test_data[] = {
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, // Dest MAC
        0x02, 0x00, 0x00, 0x00, 0x00, 0x01, // Src MAC
        0x08, 0x00, // Type: IP
        // IP header would follow...
    };
    
    uint32_t copy_size = sizeof(test_data);
    if (copy_size > max_size) copy_size = max_size;
    
    uint8_t* buf = (uint8_t*)buffer;
    for (uint32_t i = 0; i < copy_size; i++) {
        buf[i] = test_data[i];
    }
    
    return copy_size;
}