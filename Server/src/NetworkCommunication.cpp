#include "NetworkCommunication.h"

#include <algorithm>
#include <iostream>
#include <sys/socket.h>
#include <unistd.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <csignal>

using namespace std;

static string getTimestamp() {
	time_t current_time = chrono::system_clock::to_time_t(chrono::system_clock::now());
	
	return ctime(&current_time);
}

void receiveThread(NetworkCommunication &networkCommunication) {
    signal(SIGPIPE, SIG_IGN);
    
    fd_set readSet, errorSet;
    unsigned char *buffer = new unsigned char[BUFFER_SIZE];
    
    while(true) {
        networkCommunication.setFileDescriptorsReceive(readSet, errorSet);
        
        if(!networkCommunication.runSelectReceive(readSet, errorSet, buffer)) {
            break;
        }
    }
    
    delete[] buffer;
    
    cout << "ERROR: terminating receiveThread\n";
}

void sendThread(NetworkCommunication &networkCommunication) {
    signal(SIGPIPE, SIG_IGN);

    while(true) {
        auto& packet = networkCommunication.waitForOutgoingPackets();
        
        int sent = send(packet.first, packet.second.getData() + packet.second.getSent(), packet.second.getSize() - packet.second.getSent(), 0);
        
        bool removePacket = false;
        
        if(sent <= 0) {
            if(sent == 0) {
                removePacket = true;
            }
            
            else {
                if(errno == EWOULDBLOCK || errno == EAGAIN) {
                    continue;
                }
                
                else {
                    removePacket = true;
                }
            }
        }
        
        else {
            packet.second.addSent(sent);
            
            if(packet.second.fullySent()) {
                removePacket = true;
            }
        }
        
        if(removePacket) {
            networkCommunication.removeOutgoingPacket();
        }
    }
    
    cout << "ERROR: terminating sendThread\n";
}

void acceptThread(NetworkCommunication &networkCommunication) {
    signal(SIGPIPE, SIG_IGN);

    fd_set readSet, errorSet;
    
    while(true) {
        networkCommunication.setFileDescriptorsAccept(readSet, errorSet);
        
        if(!networkCommunication.runSelectAccept(readSet, errorSet)) {
            break;
        }
    }
    
    cout << "ERROR: terminating acceptThread\n";
}

bool setupServer(int &masterSocket, const unsigned short port) {
    signal(SIGPIPE, SIG_IGN);
    
    masterSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    
    if(masterSocket < 0) {
        cout << "ERROR: socket() failed\n";
        
        return false;
    }
    
    int on = 1;
    
    if(setsockopt(masterSocket, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<char*>(&on), sizeof(on)) < 0) {
        cout << "ERROR: not reusable address\n";
        
        return false;
    }
    
    sockaddr_in socketInformation;
    socketInformation.sin_family = AF_INET;
    socketInformation.sin_addr.s_addr = INADDR_ANY;
    socketInformation.sin_port = htons(port);
    
    if(bind(masterSocket, reinterpret_cast<sockaddr*>(&socketInformation), sizeof(socketInformation)) < 0) {
        cout << "ERROR: bind() failed\n";
        
        close(masterSocket);
        return false;
    }
    
    size_t socketInformationSize = sizeof(socketInformation);

	if(getsockname(masterSocket, reinterpret_cast<sockaddr*>(&socketInformation), reinterpret_cast<socklen_t*>(&socketInformationSize)) < 0) {
        cout << "ERROR: could not get address information\n";
        
        close(masterSocket);
        return false;
	}
	
	listen(masterSocket, 16);
    
    return true;
}

unsigned int NetworkCommunication::processBuffer(const unsigned char *buffer, const unsigned int received, PartialPacket &partialPacket) {
    if(partialPacket.hasHeader()) {        
        unsigned int insert = received;
        
        if(partialPacket.getSize() + received >= partialPacket.getFullSize()) {            
            insert = partialPacket.getFullSize() - partialPacket.getSize();
        }
        
        if(insert > received) {
            cout << "ERROR: trying to insert more data than available in buffer, insert = " << insert << ", received = " << received << endl;
        }
        
        partialPacket.addData(buffer, insert);
                
        return insert;
    }
    
    else {
        unsigned int leftHeader = 4 - partialPacket.getSize();
        unsigned int adding = received > leftHeader ? leftHeader : received;
        
        if(adding > received) {
            cout << "ERROR: trying to insert more data than available in buffer, adding = " << adding << ", received = " << received << endl;
        }
        
        partialPacket.addData(buffer, adding);
        
        return adding;
    }
}

void NetworkCommunication::assemblePacket(const unsigned char *buffer, const unsigned int received, Connection &connection) {      
    unsigned int processed = 0;
    
    do {
        processed += processBuffer(buffer + processed, received - processed, connection.getPartialPacket());
    } while(processed < received);
        
    moveProcessedPacketsToQueue(connection);
}

void NetworkCommunication::moveProcessedPacketsToQueue(Connection &connection) {
    if(!connection.hasIncomingPacket()) {
        return;
    }
    
    lock_guard<mutex> guard(mIncomingMutex);
    
    while(connection.hasIncomingPacket()) {
		pair<int, Packet> new_packet = { connection.getSocket(), Packet(connection.getIncomingPacket()) };
		
        mIncomingPackets.push_back(new_packet);
        connection.processedPacket();
    }
    
    mIncomingCV.notify_one();
}

NetworkCommunication::NetworkCommunication(const unsigned short port) {
    if(!setupServer(mSocket, port)) {
        return;
    }
    
    mAcceptThread = thread(acceptThread, ref(*this));
    mReceiveThread = thread(receiveThread, ref(*this));
    mSendThread = thread(sendThread, ref(*this));
}

void NetworkCommunication::setFileDescriptorsAccept(fd_set &readSet, fd_set &errorSet) {
    FD_ZERO(&readSet);
    FD_ZERO(&errorSet);
    
    FD_SET(mSocket, &readSet);
    FD_SET(mSocket, &errorSet);
}

void NetworkCommunication::setFileDescriptorsReceive(fd_set &readSet, fd_set &errorSet) {
    FD_ZERO(&readSet);
    FD_ZERO(&errorSet);
    
    FD_SET(mPipe.getSocket(), &readSet);
    FD_SET(mPipe.getSocket(), &errorSet);
    
    lock_guard<mutex> guard(mConnectionsMutex);
    
    for_each(mConnections.begin(), mConnections.end(), [&readSet, &errorSet] (pair<mutex*, Connection> &connectionPair) {        
        FD_SET(connectionPair.second.getSocket(), &readSet);
        FD_SET(connectionPair.second.getSocket(), &errorSet);
    });
}

bool NetworkCommunication::runSelectReceive(fd_set &readSet, fd_set &errorSet, unsigned char *buffer) {    
    if(select(FD_SETSIZE, &readSet, NULL, &errorSet, NULL) == 0) {
        cout << "ERROR: select() returned 0 when there's no timeout\n";
        
        return false;
    }
    
    if(FD_ISSET(mPipe.getSocket(), &errorSet)) {
        cout << "ERROR: errorSet was set in pipe, errno = " << errno << '\n';
        
        return false;
    }
    
    if(FD_ISSET(mPipe.getSocket(), &readSet)) {
        //cout << "INFO: pipe was activated\n";
        
        mPipe.resetPipe();
        return true;
    }
        
    lock_guard<mutex> guard(mConnectionsMutex);
        
    for(auto& connectionPair : mConnections) {
        bool removeConnection = false;
        mutex *connectionMutex = connectionPair.first;
        
        {            
            //lock_guard<mutex> connectionGuard(*connectionMutex);
            Connection &connection = connectionPair.second;
            
            if(FD_ISSET(connection.getSocket(), &errorSet)) {
                cout << "WARNING: errorSet was set in connection\n";
                
                removeConnection = true;
            }
            
            else if(FD_ISSET(connection.getSocket(), &readSet)) {
                int received = recv(connection.getSocket(), buffer, BUFFER_SIZE, 0);
                
                if(received <= 0) {
                    if(received == 0) {
                        cout << "INFO: connection #" << connection.getId() << " disconnected\n";
						cout << "#" << connection.getId() << " was disconnected at " << getTimestamp();
                        
                        removeConnection = true;
                    }
                    
                    else {
                        if(errno == EWOULDBLOCK || errno == EAGAIN) {
                            cout << "WARNING: EWOULDBLOCK activated\n";
                        }
                        
                        else {
                            cout << "WARNING: connection failed with errno = " << errno << '\n';
                            
                            removeConnection = true;
                        }
                    }
                }
                
                else {
                    assemblePacket(buffer, received, connection);
                }
            }
            
            if(removeConnection) {
                lock_guard<mutex> removeGuard(*connectionMutex);
                
                if(close(connection.getSocket()) < 0) {
                    cout << "ERROR: close() got errno = " << errno << endl;
                }
                
                mConnections.erase(remove_if(mConnections.begin(), mConnections.end(), [&connection] (pair<mutex*, Connection> &connectionPair) {
                    return connectionPair.second == connection;
                }));
            }
        }
        
        if(removeConnection) {
            delete connectionMutex;
        }
    }
        
    return true;
}

void NetworkCommunication::addOutgoingPacketToAllExceptUnsafe(const Packet &packet, const vector<int> &except) {
    for_each(mConnections.begin(), mConnections.end(), [&packet, &except, this] (pair<mutex*, Connection> &connectionPair) {
        if(find_if(except.begin(), except.end(), [&connectionPair] (const int fd) { return connectionPair.second == fd; }) != except.end()) {
            return;
        }
                
        addOutgoingPacket(connectionPair.second.getSocket(), packet);
    });
}

void NetworkCommunication::addOutgoingPacketToAllExcept(const Packet &packet, const std::vector<int> &except) {
    lock_guard<mutex> guard(mConnectionsMutex);
    
    for_each(mConnections.begin(), mConnections.end(), [&packet, &except, this] (pair<mutex*, Connection> &connectionPair) {
        if(find_if(except.begin(), except.end(), [&connectionPair] (const int fd) { return connectionPair.second == fd; }) != except.end()) {
            return;
        }
                
        addOutgoingPacket(connectionPair.second.getSocket(), packet);
        
        /*
        lock_guard<mutex> sendingGuard(mOutgoingMutex);
        mOutgoingPackets.push_back({connectionPair.second.getSocket(), packet});
        */
    });
}

bool NetworkCommunication::runSelectAccept(fd_set &readSet, fd_set &errorSet) {
    if(select(FD_SETSIZE, &readSet, NULL, &errorSet, NULL) == 0) {
        cout << "ERROR: select() returned 0 when there's no timeout\n";
        
        return false;
    }
    
    if(FD_ISSET(mSocket, &errorSet)) {
        cout << "ERROR: masterSocket received an error\n";
        
        return false;
    }
    
    if(FD_ISSET(mSocket, &readSet)) {
        int newSocket = accept(mSocket, 0, 0);
        
        if(newSocket < 0) {
            cout << "ERROR: accept() failed with " << errno << endl;
            
            return false;
        }
		
		size_t id;
        
        {
            lock_guard<mutex> guard(mConnectionsMutex);
            mConnections.push_back({new mutex, Connection(newSocket)});
            //mConnections.back().second.setPlayer(new Player("Bert", "", {150000, 171, 150000}, {1, 1, 1}, 0, true)); // REMOVE THIS LATER ON
			
			id = mConnections.back().second.getId();
        }
        
        mPipe.setPipe();
        
        cout << "INFO: connection #" << id << " added\n";
		cout << "#" << id << " was added at " << getTimestamp();
    }
    
    return true;
}

int NetworkCommunication::getSocket() const {
    return mSocket;
}

pair<int, Packet>& NetworkCommunication::waitForOutgoingPackets() {
    unique_lock<mutex> lock(mOutgoingMutex);
    mOutgoingCV.wait(lock, [this] { return !mOutgoingPackets.empty(); });
    
    return mOutgoingPackets.front();
}

void NetworkCommunication::addOutgoingPacket(const int fd, const Packet &packet) {
    lock_guard<mutex> guard(mOutgoingMutex);
    mOutgoingPackets.push_back({fd, packet});
    mOutgoingCV.notify_one();
}

void NetworkCommunication::removeOutgoingPacket() {
    lock_guard<mutex> guard(mOutgoingMutex);
    
    if(mOutgoingPackets.empty()) {
        cout << "ERROR: trying to pop when outgoingPackets is empty\n";
        
        return;
    }
    
    mOutgoingPackets.pop_front();
}

pair<std::mutex*, Connection>* NetworkCommunication::getConnectionAndLock(const int fd) {
    lock_guard<mutex> guard(mConnectionsMutex);
    
    auto position = find_if(mConnections.begin(), mConnections.end(), [fd] (pair<mutex*, Connection> &connectionPair) {        
        return connectionPair.second == fd;
    });
    
    if(position == mConnections.end()) {
        return nullptr;
    }
    
    position->first->lock();
    
    return &*position;
}

void NetworkCommunication::unlockConnection(pair<mutex*, Connection> &connectionPair) {
    if(connectionPair.first == nullptr) {
        cout << "ERROR: trying to unlock mutex which is nullptr\n";
        
        return;
    }
    
    connectionPair.first->unlock();
}

/*
pair<int, Packet>& NetworkCommunication::waitForProcessingPackets() {
    unique_lock<mutex> lock(mIncomingMutex);
    mIncomingCV.wait(lock, [this] { return !mIncomingPackets.empty(); });
    
    return mIncomingPackets.front();
}
*/

pair<int, Packet>* NetworkCommunication::waitForProcessingPackets() {
    unique_lock<mutex> lock(mIncomingMutex);
    
    if(mIncomingCV.wait_for(lock, chrono::milliseconds(10), [this] { return !mIncomingPackets.empty(); })) {
        return &mIncomingPackets.front();
    }
    
    return nullptr;
}

void NetworkCommunication::removeProcessingPacket() {
    lock_guard<mutex> guard(mIncomingMutex);
    
    if(mIncomingPackets.empty()) {
        cout << "ERROR: trying to pop_front when incomingPackets is empty\n";
        
        return;
    }
    
    mIncomingPackets.pop_front();
}

EventPipe::EventPipe() {
    if(pipe(mPipes) < 0) {
        cout << "ERROR: failed to create pipe, won't be able to wake threads, errno = " << errno << '\n';
    }
    
    if(fcntl(mPipes[0], F_SETFL, O_NONBLOCK) < 0) {
        cout << "WARNING: failed to set pipe non-blocking mode\n";
    }
}

EventPipe::~EventPipe() {
    if(mPipes[0] >= 0) {
        close(mPipes[0]);
    }
    
    if(mPipes[1] >= 0) {
        close(mPipes[1]);
    }
}

void EventPipe::setPipe() {
    if(write(mPipes[1], "0", 1) < 0) {
        cout << "ERROR: could not write to pipe, errno = " << errno << '\n';
    }
}

void EventPipe::resetPipe() {
    unsigned char buffer;

    while(read(mPipes[0], &buffer, 1) == 1) {
    }
}

int EventPipe::getSocket() {
    return mPipes[0];
}