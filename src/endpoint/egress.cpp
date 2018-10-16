//
// Created by zanna on 05/10/18.
//

#include "egress.h"

void endpoint::Egress::manage_exiting_udp_packets(unsigned char* pkt,
                                                  size_t pkt_len,
                                                  const ConnectionEntry& ce,
                                                  socket_fd socket) {
    LOG(ldebug, "manage_exiting udp packets");
    auto header = utils::sfc_header::SFCUtilities::retrieve_header(pkt);
    client::udp::ClientUDP client;
    socket = client.send_only(
            pkt, pkt_len, const_cast<char*>(
                utils::PacketUtils::int_to_ip(header.destination_address).c_str()),
            htons(header.destination_port));
    update_entry(ce, socket, db::endpoint_type::EGRESS_T);
    ssize_t received_len;
    auto buffer = new unsigned char[BUFFER_SIZE];
    struct sockaddr_in server;
    server.sin_family      = AF_INET;
    server.sin_port        = htons(header.destination_port);
    server.sin_addr.s_addr = header.destination_address;
    socklen_t server_addr_len = sizeof(server);
    char* sfcid;
    char* next_ip;
    uint16_t next_port;
    unsigned long ttl;
    unsigned char* formatted_pkt;
    do {
        received_len = recvfrom(socket,
                                buffer, BUFFER_SIZE,
                                0,
                                (struct sockaddr *)&server,
                                &(server_addr_len));
        if (received_len < 0) {
            perror("error receiving data");
            exit(EXIT_FAILURE);
        } else if (received_len > 0) {
            sfcid = classifier_.classify_pkt(buffer, received_len);
            std::vector<db::utils::Address> path =
                    roulette_->get_route_list(atoi(sfcid));
            // +2 because of ingress & egress
            ttl = path.size() + 2;
            next_ip = const_cast<char*>(path[0].get_URL().c_str());
            next_port = path[0].get_port();
            sfc_header flh =
                    utils::sfc_header::SFCUtilities::create_header(
                            atoi(sfcid), 0,
                                    utils::PacketUtils::int_to_ip(
                                            header.source_address).c_str(),
                            htons(header.source_port),
                                    utils::PacketUtils::int_to_ip(
                                            header.destination_address).c_str(),
                            htons(header.destination_port), ttl, 0);

            utils::sfc_header::SFCUtilities::prepend_header(buffer, pkt_len,
                                                            flh, formatted_pkt);

            client::udp::ClientUDP().send_and_wait_response(formatted_pkt,
                                                            pkt_len + SFCHDR_LEN,
                                                            next_ip, next_port);
        }
    } while(received_len > 0);

    free(pkt);
    free(buffer);

    // TODO think when closing socket
}


void endpoint::Egress::manage_exiting_tcp_packets(unsigned char* pkt,
                                                  size_t pkt_len,
                                                  const ConnectionEntry& ce,
                                                  socket_fd socket) {
    LOG(ldebug, "manage_exiting tcp packets");
    auto header = utils::sfc_header::SFCUtilities::retrieve_header(pkt);

    if (socket == -1) {
        client::tcp::ClientTCP client;
        client.connect_to_server(
                const_cast<char*>(
                        utils::PacketUtils::int_to_ip(header.destination_address).c_str()),
                htons(header.destination_port));
        socket = client.access_to_socket();
        update_entry(ce, socket, db::endpoint_type::EGRESS_T);
    }
    send(socket, pkt, pkt_len, 0);

    ssize_t received_len;
    auto buffer = new unsigned char[BUFFER_SIZE];
    char* sfcid;
    char* next_ip;
    uint16_t next_port;
    unsigned long ttl;
    unsigned char* formatted_pkt;
    do {
        received_len = read(socket, buffer, BUFFER_SIZE);
        if (received_len < 0) {
            perror("error receiving data");
            exit(EXIT_FAILURE);
        } else if (received_len > 0) {
            sfcid = classifier_.classify_pkt(buffer, received_len);
            std::vector<db::utils::Address> path =
                    roulette_->get_route_list(atoi(sfcid));
            // +2 because of ingress & egress
            ttl = path.size() + 2;
            next_ip = const_cast<char*>(path[0].get_URL().c_str());
            next_port = path[0].get_port();
            sfc_header flh =
                    utils::sfc_header::SFCUtilities::create_header(
                            atoi(sfcid), 0, utils::PacketUtils::int_to_ip(
                                            header.source_address).c_str(),
                            htons(header.source_port), utils::PacketUtils::int_to_ip(
                                            header.destination_address).c_str(),
                            header.destination_port, ttl, 0);
            utils::sfc_header::SFCUtilities::prepend_header(buffer, pkt_len,
                                                            flh, formatted_pkt);
            client::udp::ClientUDP().send_and_wait_response(formatted_pkt,
                                                            pkt_len + SFCHDR_LEN,
                                                            next_ip, next_port);
        }
    } while(received_len > 0);

    free(pkt);
    free(buffer);

    //close(new_socket_fd);
    // TODO think when closing socket
}

void endpoint::Egress::manage_pkt_from_chain(void * mngmnt_args) {
    LOG(ldebug, "manage_pkt_from_chain");
    auto args = (server::udp::udp_pkt_mngmnt_args *)mngmnt_args;

    std::string ack = "ACK";
    sendto(args->socket_fd,
           ack.c_str(),
           ack.length(),
           0,
           reinterpret_cast<struct sockaddr*>(&args->client_address),
           sizeof(args->client_address));

    sfc_header flh = utils::sfc_header::SFCUtilities::retrieve_header(
            (unsigned char*)args->pkt);

    auto headers =
            utils::PacketUtils::retrieve_ip_udp_header(
                    (unsigned char*)(args->pkt + SFCHDR_LEN));

    // TODO think something more extensible to support more protocols
    // '6' is the code for TCP '17' for UDP
    db::protocol_type conn_type = (headers.first.protocol == 6?
                                        db::protocol_type::TCP :
                                        db::protocol_type::UDP);

    ConnectionEntry ce(utils::PacketUtils::int_to_ip(flh.source_address),
                       utils::PacketUtils::int_to_ip(flh.destination_address),
                       htons(flh.source_port), htons(flh.destination_port),
                       std::to_string(flh.p_id), conn_type);

    args->socket_fd = retrieve_connection(ce);

    if (conn_type == db::protocol_type::TCP) {
        std::function<void ()> f = [this, args, ce]() {
            manage_exiting_tcp_packets((unsigned char*)args->pkt, args->pkt_len,
                                       ce, args->socket_fd);
        };
        // TODO do something better with a real thread pool
        //ASYNC_TASK(f);
        std::thread t1(f);
        t1.detach();
    } else {
        std::function<void ()> f = [this, args, ce]() {
            manage_exiting_udp_packets((unsigned char*)args->pkt, args->pkt_len,
                                       ce, args->socket_fd);
        };
        // TODO do something better with a real thread pool
        //ASYNC_TASK(f);
        std::thread t1(f);
        t1.detach();
    };

}

void endpoint::Egress::start(uint16_t int_port, uint16_t ext_port) {
    std::function<void (void*)> vnf_callback = [this](void* args) {
        this->manage_pkt_from_chain(args);
    };
    std::thread udp_internal_thread([&int_port, &vnf_callback]{
        LOG(ldebug, "Starting server udp for internal pkts"
                    + std::to_string(int_port));
        auto server = new server::udp::ServerUDP(int_port);
        server->set_pkt_manager(vnf_callback);
        server->run();
    });

    udp_internal_thread.join();
}