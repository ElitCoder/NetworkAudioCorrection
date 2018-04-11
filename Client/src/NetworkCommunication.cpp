#include "NetworkCommunication.h"
#include "Threading.h"

#include <iostream>

using namespace std;

NetworkCommunication::NetworkCommunication(const string &host, const unsigned short port) : mSocket(0) {
    Threading::Public::connectToServer(host, port, mSocket);
    
    mReceivingThread = thread(Threading::networkReceivingThread, ref(*this));
    mSendingThread = thread(Threading::networkSendingThread, ref(*this));
}

PartialPacket& NetworkCommunication::getPartialPacket() {
    if(mPartialPackets.empty() || mPartialPackets.back().isFinished()) {
        pushPartialPacket(PartialPacket());
    }
    
    return mPartialPackets.back();
}

void NetworkCommunication::pushPartialPacket(const PartialPacket &partialPacket) {
    mPartialPackets.push_back(partialPacket);
}

void NetworkCommunication::popFullPartialPacket() {
    mPartialPackets.pop_front();
}

bool NetworkCommunication::hasFullPartialPacket() const {
    return mPartialPackets.empty() ? false : mPartialPackets.front().isFinished();
}

PartialPacket& NetworkCommunication::getFullPartialPacket() {
    return mPartialPackets.front();
}

void NetworkCommunication::moveCompletePartialPackets() {
    if(!hasFullPartialPacket()) {
        return;
    }
    
    lock_guard<mutex> guard(mIncomingMutex);
    
    while(hasFullPartialPacket()) {
        mIncomingPackets.push_back(move(getFullPartialPacket()));
        popFullPartialPacket();
    }
    
    mIncomingCV.notify_one();
}

int& NetworkCommunication::getSocket() {
    return mSocket;
}

void NetworkCommunication::pushOutgoingPacket(const Packet &packet) {
    lock_guard<mutex> lockingGuard(mOutgoingMutex);
    mOutgoingPackets.push_back(packet);
    mOutgoingCV.notify_one();
}

Packet& NetworkCommunication::getOutgoingPacket() {
    unique_lock<mutex> uniqueLock(mOutgoingMutex);
    mOutgoingCV.wait(uniqueLock, [this] { return !mOutgoingPackets.empty(); });
    
    return mOutgoingPackets.front();
}

void NetworkCommunication::popOutgoingPacket() {
    lock_guard<mutex> lockingGuard(mOutgoingMutex);
    mOutgoingPackets.pop_front();
}

Packet NetworkCommunication::waitForIncomingPacket() {
	unique_lock<mutex> lock(mIncomingMutex);
	mIncomingCV.wait(lock, [this] { return !mIncomingPackets.empty(); });
	
	Packet packet = mIncomingPackets.front();
	mIncomingPackets.pop_front();
	
	return packet;
}

Packet* NetworkCommunication::getIncomingPacket() {
    unique_lock<mutex> lock(mIncomingMutex);
    
    //if(mIncomingCV.wait(lock, [this] { return !mIncomingPackets.empty(); })) {
    if(mIncomingCV.wait_for(lock, chrono::milliseconds(0), [this] { return !mIncomingPackets.empty(); })) {
        return &mIncomingPackets.front();
    }
    
    return nullptr;
    //return mIncomingPackets.front();
}

void NetworkCommunication::popIncomingPacket() {
    lock_guard<mutex> guard(mIncomingMutex);
    mIncomingPackets.pop_front();
}