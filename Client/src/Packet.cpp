#include "Packet.h"
#include "PacketSetupException.h"
#include "PartialPacket.h"

#include <array>
#include <cstring>
#include <iostream>

using namespace std;

Packet::Packet() : m_sent(0), m_read(0), m_finalized(false) {
}

void Packet::clear() {
    m_packet.clear();
    m_sent = 0;
    m_read = 0;
    m_finalized = false;
}

Packet::Packet(const unsigned char *buffer, const unsigned int size) : m_sent(0), m_read(0), m_finalized(false) {
    m_packet.insert(m_packet.end(), buffer, buffer + size);
}

Packet::Packet(PartialPacket &&partialPacket) : m_sent(0), m_read(0), m_finalized(true) {
    m_packet = move(partialPacket.getData());
    
    m_packet.erase(m_packet.begin(), m_packet.begin() + 4);
}

Packet::Packet(const Packet &packet) {
    m_sent = packet.m_sent;
    m_read = packet.m_read;
    m_finalized = packet.m_finalized;
    m_packet = packet.m_packet;
}

void Packet::addHeader(const unsigned char header) {
    m_packet.push_back(header);
}

/*
void Packet::addFloat(const float nbr) {
    if(isFinalized()) {
        cout << "ERROR: can't add anything to a finalized packet\n";
        
        return;
    }
    
    string floatString = to_string(nbr);
    
    addString(floatString);
}
*/

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

void Packet::addBool(const bool val) {
    if(isFinalized()) {
        cout << "ERROR: can't add anything to a finalized packet\n";
        
        return;
    }
    
    m_packet.push_back(val ? 1 : 0);
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

void Packet::addInt(const int nbr) {
    m_packet.push_back((nbr >> 24) & 0xFF);
    m_packet.push_back((nbr >> 16) & 0xFF);
    m_packet.push_back((nbr >> 8) & 0xFF);
    m_packet.push_back(nbr & 0xFF);
}

void Packet::addString(const string &str) {
    addInt(str.length());
    m_packet.insert(m_packet.end(), str.begin(), str.end());
}

void Packet::addPointer(const unsigned char *ptr, const unsigned int size) {
    addInt(size);
    m_packet.insert(m_packet.end(), ptr, ptr + size);
}

unsigned char Packet::getByte() {    
    return m_packet.at(m_read++);
}

/*
float Packet::getFloat() {
    string floatString = getString();
    
    return stof(floatString);
}
*/

int Packet::getInt() {
    int nbr = 0;
    
    try {
        nbr = (m_packet.at(m_read) << 24) | (m_packet.at(m_read + 1) << 16) | (m_packet.at(m_read + 2) << 8) | m_packet.at(m_read + 3); 
    } catch(...) {
        cout << "ERROR: exception at getInt() with m_packet.size() = " << m_packet.size() << " and trying to read at m_read = " << m_read << endl;
    }
    
    m_read += 4;
    
    return nbr;
}

bool Packet::getBool() {
    return m_packet.at(m_read++) == 1 ? true : false;
}

string Packet::getString() {
    unsigned int length = getInt();
    
    string str(m_packet.begin() + m_read, m_packet.begin() + m_read + length);
    m_read += length;
    
    return str;
}

const unsigned char* Packet::getData() const {
    if(!isFinalized()) {
        throw PacketSetupException();
    }
    
    return m_packet.data();
}

unsigned int Packet::getSize() const {
    if(!isFinalized()) {
        throw PacketSetupException();
    }
    
    return m_packet.size();
}

unsigned int Packet::getSent() const {
    if(!isFinalized()) {
        throw PacketSetupException();
    }
    
    return m_sent;
}

void Packet::addSent(const int sent) {
    if(!isFinalized()) {
        throw PacketSetupException();
    }
    
    m_sent += sent;
}

bool Packet::isFinalized() const {
    return m_finalized;
}

bool Packet::fullySent() const {
    return m_sent >= m_packet.size();
}

void Packet::finalize() {
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