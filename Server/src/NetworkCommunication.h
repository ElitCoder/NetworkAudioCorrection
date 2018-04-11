#ifndef NETWORK_COMMUNICATION_H
#define NETWORK_COMMUNICATION_H

#include "Connection.h"

#include <thread>
#include <list>
#include <mutex>
#include <condition_variable>

enum NetworkConstants {
    BUFFER_SIZE = 4096
};

class EventPipe {
public:
    explicit EventPipe();
    ~EventPipe();
    
    void setPipe();
    void resetPipe();
    
    int getSocket();
    
private:
    int mPipes[2];
};

class NetworkCommunication {
public:
    NetworkCommunication(const unsigned short port);
    
    void setFileDescriptorsAccept(fd_set &readSet, fd_set &errorSet);
    void setFileDescriptorsReceive(fd_set &readSet, fd_set &errorSet);
    
    bool runSelectAccept(fd_set &readSet, fd_set &errorSet);
    bool runSelectReceive(fd_set &readSet, fd_set &errorSet, unsigned char *buffer);
    
    int getSocket() const;
    std::pair<std::mutex*, Connection>* getConnectionAndLock(const int fd);
    void unlockConnection(std::pair<std::mutex*, Connection> &connectionPair);
    
    std::pair<int, Packet>& waitForOutgoingPackets();
    void removeOutgoingPacket();
    void addOutgoingPacket(const int fd, const Packet &packet);
    
    std::pair<int, Packet>* waitForProcessingPackets();
    void removeProcessingPacket();
    
    void addOutgoingPacketToAllExcept(const Packet &packet, const std::vector<int> &except);
    void addOutgoingPacketToAllExceptUnsafe(const Packet &packet, const std::vector<int> &except);
    
private:
    void assemblePacket(const unsigned char *buffer, const unsigned int received, Connection &connection);
    unsigned int processBuffer(const unsigned char *buffer, const unsigned int received, PartialPacket &partialPacket);
    void moveProcessedPacketsToQueue(Connection &connection);
    
    int mSocket;
    EventPipe mPipe;
    
    std::thread mReceiveThread, mSendThread, mAcceptThread;
    
    std::mutex mIncomingMutex;
    std::condition_variable mIncomingCV;
    std::list<std::pair<int, Packet>> mIncomingPackets;
    
    std::mutex mOutgoingMutex;
    std::condition_variable mOutgoingCV;
    std::list<std::pair<int, Packet>> mOutgoingPackets;
    
    std::mutex mConnectionsMutex;
    std::vector<std::pair<std::mutex*, Connection>> mConnections;
};

#endif