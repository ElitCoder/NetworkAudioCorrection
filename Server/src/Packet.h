#ifndef PACKET_H
#define PACKET_H

#include "PartialPacket.h"

#include <string>
#include <vector>

class Packet {
public:
    Packet();
    Packet(const unsigned char *buffer, const unsigned int size);
    
    Packet(const Packet &packet);
    Packet(Packet &&packet) = delete;
    
	Packet(const PartialPacket& partial);
    Packet(PartialPacket &&partialPacket);
    
    Packet& operator=(const Packet &packet) = delete;
    Packet& operator=(Packet &&packet) = delete;
    
    void addHeader(const unsigned char header);
    void addString(const std::string &str);
    void addPointer(const unsigned char *ptr, const unsigned int size);
    void addInt(const int nbr);
    void addFloat(const float nbr);
    void addBool(const bool val);
    
    unsigned char getByte();
    int getInt();
    float getFloat();
    std::string getString();
	bool getBool();
    
    const unsigned char* getData() const;
    unsigned int getSize() const;
    unsigned int getSent() const;
    void addSent(const int sent);
    bool fullySent() const;
    
    bool isEmpty() const;
    void finalize();
	
	friend std::ostream& operator<<(std::ostream& out, const Packet& packet);
    
private:
    bool isFinalized() const;
    
    std::vector<unsigned char> m_packet;
    unsigned int m_sent, m_read;
    
    bool m_finalized;
};

#endif