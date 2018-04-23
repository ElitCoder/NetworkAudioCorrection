#ifndef PACKET_H
#define PACKET_H

#include <string>
#include <vector>

class PartialPacket;

class Packet {
public:
    Packet();
    Packet(const unsigned char *buffer, const unsigned int size);
    
    Packet(const Packet &packet);
    
    Packet(PartialPacket &&partialPacket);
    
    Packet& operator=(const Packet &packet) = delete;
    Packet& operator=(Packet &&packet) = delete;
    
    void addHeader(const unsigned char header);
    void addString(const std::string &str);
    void addPointer(const unsigned char *ptr, const unsigned int size);
    void addInt(const int nbr);
    void addFloat(const float nbr);
	void addBool(bool value);
    
    unsigned char getByte();
    float getFloat();
    std::string getString();
    int getInt();
    bool getBool();
    
    const unsigned char* getData() const;
    unsigned int getSize() const;
    unsigned int getSent() const;
    
    void addSent(const int sent);
    bool fullySent() const;
    
    void finalize();
    bool isEmpty() const;
    
    void clear();
	
	friend std::ostream& operator<<(std::ostream& out, const Packet& packet);
    
private:
    bool isFinalized() const;
    
    std::vector<unsigned char> m_packet;
    unsigned int m_sent, m_read;
    
    bool m_finalized;
};

#endif