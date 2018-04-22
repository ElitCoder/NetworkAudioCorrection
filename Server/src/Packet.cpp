#include "Packet.h"
#include "PartialPacket.h"

#include <array>
#include <iostream>
#include <cstring>

using namespace std;

Packet::Packet() : m_sent(0), m_read(0), m_finalized(false) {
}

Packet::Packet(const unsigned char *buffer, const unsigned int size) : m_sent(0), m_read(0), m_finalized(false) {
    if(buffer == nullptr || size == 0) {
        cout << "WARNING: trying to create packet with empty buffer\n";
        
        return;
    }
    
    m_packet.insert(m_packet.end(), buffer, buffer + size);
}

Packet::Packet(const PartialPacket& partial) {
	m_sent = 0;
	m_read = 0;
	m_finalized = true;
	m_packet = partial.getData();
	
	if(m_packet.size() < 4) {
        cout << "ERROR: trying to remove header from partialPacket when m_packet is < 4\n";
        
        return;
    }
    
    m_packet.erase(m_packet.begin(), m_packet.begin() + 4);
}

Packet::Packet(PartialPacket &&partialPacket) : m_sent(0), m_read(0), m_finalized(true) {
    m_packet = move(partialPacket.getData());
    
    if(m_packet.size() < 4) {
        cout << "ERROR: trying to remove header from partialPacket when m_packet is < 4\n";
        
        return;
    }
    
    m_packet.erase(m_packet.begin(), m_packet.begin() + 4);
}

Packet::Packet(const Packet &packet) {
    m_sent = packet.m_sent;
    m_read = packet.m_read;
    m_finalized = packet.m_finalized;
    m_packet = packet.m_packet;
}

void Packet::addHeader(const unsigned char header) {
    if(isFinalized()) {
        cout << "ERROR: can't add anything to a finalized packet\n";
        
        return;
    }
    
    m_packet.push_back(header);
}

void Packet::addString(const string &str) {
    if(isFinalized()) {
        cout << "ERROR: can't add anything to a finalized packet\n";
        
        return;
    }
    
    if(str.length() == 0) {
        cout << "WARNING: trying to add an empty string to packet\n";
    }
    
    addInt(str.length());
    m_packet.insert(m_packet.end(), str.begin(), str.end());
}

void Packet::addPointer(const unsigned char *ptr, const unsigned int size) {
    if(isFinalized()) {
        cout << "ERROR: can't add anything to a finalized packet\n";
        
        return;
    }
    
    if(ptr == nullptr || size == 0) {
        cout << "ERROR: trying to add a nullptr or size = 0 to packet\n";
    }
    
    addInt(size);
    m_packet.insert(m_packet.end(), ptr, ptr + size);
}

void Packet::addInt(const int nbr) {
    if(isFinalized()) {
        cout << "ERROR: can't add anything to a finalized packet\n";
        
        return;
    }
    
    m_packet.push_back((nbr >> 24) & 0xFF);
    m_packet.push_back((nbr >> 16) & 0xFF);
    m_packet.push_back((nbr >> 8) & 0xFF);
    m_packet.push_back(nbr & 0xFF);
}

void Packet::addBool(const bool val) {
    if(isFinalized()) {
        cout << "ERROR: can't add anything to a finalized packet\n";
        
        return;
    }
    
    m_packet.push_back(val ? 1 : 0);
}

bool Packet::getBool() {
	return m_packet.at(m_read++) == 1 ? true : false;
}

void Packet::addFloat(const float nbr) {
    if(isFinalized()) {
        cout << "ERROR: can't add anything to a finalized packet\n";
        
        return;
    }
    
    /*
    string floatString = to_string(nbr);
    
    addString(floatString);
    */
    
    string floatString = to_string(nbr);
    
    m_packet.push_back(floatString.length());
    m_packet.insert(m_packet.end(), floatString.begin(), floatString.end());
}

float Packet::getFloat() {
    unsigned char length = m_packet.at(m_read++);
    
    string str(m_packet.begin() + m_read, m_packet.begin() + length + m_read);
    m_read += length;
    
    return stof(str);
    
    /*
    string floatString = getString();
    
    return stof(floatString);
    */
}

string Packet::getString() {
    unsigned int length = getInt();
    
    string str(m_packet.begin() + m_read, m_packet.begin() + m_read + length);
    m_read += length;
    
    return str;
}

const unsigned char* Packet::getData() const {
    if(!isFinalized()) {
        cout << "ERROR: can't return data from packet not finalized\n";
        
        return nullptr;
    }
    
    const unsigned char *data = m_packet.data();
    
    if(data == nullptr) {
        cout << "ERROR: trying to return data from packet when nullptr\n";
    }
    
    return data;
}

unsigned int Packet::getSize() const {
    if(!isFinalized()) {
        cout << "ERROR: can't return size of data from packet not finalized\n";
        
        return 0;
    }
    
    return m_packet.size();
}

unsigned int Packet::getSent() const {
    return m_sent;
}

unsigned char Packet::getByte() {
    try {
        return m_packet.at(m_read++);
    } catch(const out_of_range &e) {
        cout << "ERROR: trying to read beyond packet size, m_read = " << m_read << " packet size = " << m_packet.size() << endl;
        
        return 0;
    }
}

int Packet::getInt() {
    int nbr;
    
    try {
        nbr = (m_packet.at(m_read) << 24) | (m_packet.at(m_read + 1) << 16) | (m_packet.at(m_read + 2) << 8) | m_packet.at(m_read + 3); 
        m_read += 4;
    } catch(...) {
        cout << "ERROR: trying to read beyond packet size, m_read = " << m_read << " packet size = " << m_packet.size() << endl;
        
        return 0;
    }
    
    return nbr;
}

void Packet::addSent(const int sent) {
    m_sent += sent;
}

bool Packet::fullySent() const {
    return m_sent >= m_packet.size();
}

bool Packet::isFinalized() const {
    return m_finalized;
}

void Packet::finalize() {
    if(isFinalized()) {
        cout << "ERROR: packet is already finalized\n";
        
        return;
    }
    
    unsigned int fullPacketSize = m_packet.size() + 4;
    array<unsigned int, 4> packetSize;
    
    packetSize[0] = (fullPacketSize >> 24) & 0xFF;
    packetSize[1] = (fullPacketSize >> 16) & 0xFF;
    packetSize[2] = (fullPacketSize >> 8) & 0xFF;
    packetSize[3] = fullPacketSize & 0xFF;
    
    m_packet.insert(m_packet.begin(), packetSize.begin(), packetSize.end());
    
    m_finalized = true;
}

bool Packet::isEmpty() const {
    return isFinalized() ? m_packet.size() <= 4 : m_packet.empty();
}

ostream& operator<<(ostream& out, const Packet& packet) {
	for (size_t i = 0; i < packet.getSize(); i++)
		printf("%02X ", packet.getData()[i]);
		
	cout << endl;
	
	return out;
}