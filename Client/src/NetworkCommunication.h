#ifndef NETWORK_COMMUNICATION_H
#define NETWORK_COMMUNICATION_H

#include "Packet.h"

#include <thread>
#include <mutex>
#include <condition_variable>
#include <list>

enum NetworkCommunicationConstants {
    BUFFER_SIZE = 4096
};

class NetworkCommunication {
public:
    NetworkCommunication(const std::string &host, const unsigned short port);
    
    NetworkCommunication(const NetworkCommunication &networkCommunication) = delete;
    NetworkCommunication(NetworkCommunication &&networkCommunication) = delete;
    
    NetworkCommunication& operator=(const NetworkCommunication &networkCommunication) = delete;
    NetworkCommunication& operator=(NetworkCommunication &&networkCommunication) = delete;
    
    int& getSocket();
    
    void pushOutgoingPacket(const Packet &packet);
    void popOutgoingPacket();
    Packet& getOutgoingPacket();
    
    PartialPacket& getPartialPacket();
    PartialPacket& getFullPartialPacket();
    
    void moveCompletePartialPackets();
    
	Packet waitForIncomingPacket();
    Packet* getIncomingPacket();
    void popIncomingPacket();
    
private:
    void popFullPartialPacket();
    void pushPartialPacket(const PartialPacket &partialPacket);
    bool hasFullPartialPacket() const;
    
    std::thread mReceivingThread, mSendingThread;
    int mSocket;
    
    std::list<Packet> mOutgoingPackets;
    std::mutex mOutgoingMutex;
    std::condition_variable mOutgoingCV;
    
    std::list<Packet> mIncomingPackets;
    std::mutex mIncomingMutex;
    std::condition_variable mIncomingCV;
    
    std::list<PartialPacket> mPartialPackets;
};

#endif