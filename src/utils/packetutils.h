//
// Created by zanna on 08/10/18.
//

#ifndef IRONHIDE_PACKETUTILS_H
#define IRONHIDE_PACKETUTILS_H

#include <string>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <cstring>
#include <linux/tcp.h>
#include <linux/udp.h>
#include <linux/ip.h>

namespace utils {

typedef std::pair<struct iphdr, tcphdr> header_ip_tcp;
typedef std::pair<struct iphdr, udphdr> header_ip_udp;

class PacketUtils {
private:
    PacketUtils() = default;
public:
    static const char* ack;
    static const uint8_t ack_size;
    static std::string int_to_ip(uint32_t ip_int);
    static uint32_t ip_to_int(const char *ip_string);
    static uint16_t retrieve_port(uint16_t port);
    static header_ip_tcp retrieve_ip_tcp_header(unsigned char*);
    static header_ip_udp retrieve_ip_udp_header(unsigned char*);
    static iphdr retrieve_ip_header(unsigned char *);
};

#define INT_TO_IP(ip_int) \
    utils::PacketUtils::int_to_ip(ip_int)
#define INT_TO_IP_C_STR(ip_int) \
    utils::PacketUtils::int_to_ip(ip_int).c_str()
#define ACK utils::PacketUtils::ack
#define ACK_SIZE utils::PacketUtils::ack_size
} // namespace utils

#endif //IRONHIDE_PACKETUTILS_H
