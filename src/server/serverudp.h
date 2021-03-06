#ifndef IRONHIDE_SERVERUDP_H
#define IRONHIDE_SERVERUDP_H

#include <netinet/in.h>
#include <pthread.h>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <atomic>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <iostream>

#include "utils/asynctaskexecutor.h"
#include "utils/sfcutilities.h"
#include "server.h"

namespace server {
namespace udp {

/**
 * Structure that contains data used for managing a packet received
 */
typedef struct {
    int socket_fd;
    struct sockaddr_in client_address;
    char* pkt;
    ssize_t pkt_len;
} udp_pkt_mngmnt_args;

class ServerUDP : public Server {
public:
    /**
     * Constructor
     * @param port Port on which the udp server waits for packets
     */
    explicit ServerUDP(uint16_t port);
    /**
     * Start server
     */
    void run() override;
};

} // namespace tcp
} // namespace server

#endif //IRONHIDE_SERVERUDP_H
