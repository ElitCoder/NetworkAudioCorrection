#ifndef PARTIAL_PACKET_H
#define PARTIAL_PACKET_H

#include <vector>

class PartialPacket {
public:
    PartialPacket();
    
    unsigned int getFullSize() const;
    unsigned int getSize() const;
    void addData(const unsigned char *buffer, const unsigned int size);
    bool hasHeader() const;
    bool isFinished() const;
    std::vector<unsigned char>& getData();
    
private:
    void setFullSize();
    
    std::vector<unsigned char> m_data;
    unsigned int m_size;
};

#endif