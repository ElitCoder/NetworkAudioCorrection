#include "PartialPacket.h"

#include <iostream>

using namespace std;

PartialPacket::PartialPacket() : m_size(0) {
}

unsigned int PartialPacket::getSize() const {
    return m_data.size();
}

void PartialPacket::setFullSize() {
    if(m_data.size() < 4) {
        cout << "ERROR: trying to set full size at partial packet when size < 4\n";
        
        return;
    }
    
    m_size = (m_data.front() << 24) | (m_data.at(1) << 16) | (m_data.at(2) << 8) | m_data.at(3); 
}

unsigned int PartialPacket::getFullSize() const {
    return m_size;
}

void PartialPacket::addData(const unsigned char *buffer, const unsigned int size) {
    if(buffer == nullptr || size == 0) {
        cout << "ERROR: trying to insert nullptr or size = 0 data in partial packet\n";
        
        return;
    }
    
    m_data.insert(m_data.end(), buffer, buffer + size);
    
    if(m_size == 0 && m_data.size() >= 4) {
        setFullSize();
    }
}

bool PartialPacket::hasHeader() const {
    return m_size != 0;
}

bool PartialPacket::isFinished() const {
    return m_size != 0 ? m_data.size() == m_size : false;
}

vector<unsigned char>& PartialPacket::getData() {
    return m_data;
}

const vector<unsigned char>& PartialPacket::getData() const {
	return m_data;
}