#include "Connection.h"

#include <fcntl.h>
#include <netinet/tcp.h>
#include <netinet/in.h>
#include <iostream>

using namespace std;

Connection::Connection(const int socket) : m_socket(socket) {
    if(fcntl(m_socket, F_SETFL, O_NONBLOCK) == -1) {
        cout << "WARNING: could not make non-blocking sockets\n";
    }
    
    int on = 1;
    
    if(setsockopt(m_socket, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<char*>(&on), sizeof(on)) < 0) {
        cout << "WARNING: could not set TCP_NODELAY\n";
    }
	
	static size_t id;
	m_id = id++;
}

size_t Connection::getId() {
	return m_id;
}

bool Connection::operator==(const Connection &connection) {
    return m_socket == connection.m_socket;
}

bool Connection::operator==(const int fd) {
    return m_socket == fd;
}

int Connection::getSocket() const {
    return m_socket;
}

PartialPacket& Connection::getPartialPacket() {
    if(m_inQueue.empty() || m_inQueue.back().isFinished()) {
        addPartialPacket(PartialPacket());
    }
    
    return m_inQueue.back();
}

void Connection::addPartialPacket(const PartialPacket &partialPacket) {
    m_inQueue.push_back(partialPacket);
}

bool Connection::hasIncomingPacket() const {
    return m_inQueue.empty() ? false : m_inQueue.front().isFinished();
}

PartialPacket& Connection::getIncomingPacket() {
    if(m_inQueue.empty()) {
        cout << "ERROR: trying to get an incoming packet while the inQueue is empty\n";
    }
    
    return m_inQueue.front();
}

void Connection::processedPacket() {
    if(m_inQueue.empty()) {
        cout << "ERROR: trying to pop_front when the inQueue is empty\n";
        
        return;
    }
    
    m_inQueue.pop_front();
}