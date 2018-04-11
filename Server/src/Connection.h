#ifndef CONNECTION_H
#define CONNECTION_H

#include "Packet.h"
#include "PartialPacket.h"

#include <list>

class Connection {
public:
    Connection(const int socket);
    
    bool operator==(const Connection &connection);
    bool operator==(const int fd);

    int getSocket() const;
    PartialPacket& getPartialPacket();
    void addPartialPacket(const PartialPacket &partialPacket);
    bool hasIncomingPacket() const;
    PartialPacket& getIncomingPacket();
    void processedPacket();
	size_t getId();
    
private:
    int m_socket;
	size_t m_id;
    
    std::list<PartialPacket> m_inQueue;
};

#endif