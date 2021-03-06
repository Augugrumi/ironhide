#ifndef IRONHIDE_CLASSIFICATOR_H
#define IRONHIDE_CLASSIFICATOR_H

#include <cstdio>
#include <map>
#include <linux/ip.h>

#include "packetutils.h"
#include "log.h"

namespace classifier {

enum pkt_type {DEFAULT, TCP, UDP};

/**
 * Mock class of a real packet classifier
 */
class Classifier {
public:
    class Mapper;
private:
    // TODO request sfcs in order to popolate the map
    std::map<Mapper, const char*> sfc_map;
public:
    /**
     * Class that maps a packet type to a SFC id
     */
    class Mapper {
    private:
        pkt_type type_;
    public:
        void set_pkt(unsigned char * pkt,
                     size_t pkt_len);
        bool operator==(const Mapper&) const;
        bool operator<(const Mapper&) const;
        static pkt_type default_mapping();

        //TODO to remove when harbour calls are implemented
        static Mapper defaultUDPMapper();
        static Mapper defaultTCPMapper();
    };
    char* classify_pkt(unsigned char * pkt,
                             size_t pkt_len);

    // TODO to refactor
    //static void set_remote(const char* ip, uint16_t port);
};

} // namespace classifier

#endif //IRONHIDE_CLASSIFICATOR_H
