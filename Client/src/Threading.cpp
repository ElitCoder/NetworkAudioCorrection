#include "Threading.h"
#include "Packet.h"
#include "PartialPacket.h"
#include "NetworkCommunication.h"

#include <iostream>
#include <netinet/in.h>
#include <unistd.h>
#include <netdb.h>
#include <cstring>
#include <netinet/tcp.h>
#include <fcntl.h>

using namespace std;

void Threading::Public::connectToServer(const string &hostname, const unsigned short port, int &serverSocket) {
    serverSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    
    if(serverSocket < 0) {
        cout << "ERROR: socket() failed\n";
        
        close(serverSocket);
        return;
    }
    
    sockaddr_in server;
	server.sin_family = AF_INET;
	hostent* hp = gethostbyname(hostname.c_str());
    
	if(hp == 0) {
        cout << "ERROR: hostent failed\n";
        
        close(serverSocket);
		return;
	}
	
	memcpy(reinterpret_cast<char*>(&server.sin_addr), reinterpret_cast<char*>(hp->h_addr), hp->h_length);
	server.sin_port = htons(port);
    
	if(connect(serverSocket, reinterpret_cast<sockaddr*>(&server), sizeof(server)) < 0) {
        cout << "ERROR: could not connect to server\n";
        
        close(serverSocket);
        return;
	}
    
    int on = 1;
    
    if(setsockopt(serverSocket, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<char*>(&on), sizeof(on)) < 0) {
        cout << "WARNING: could not set TCP_NODELAY\n";
    }
}

unsigned int Threading::Private::composeIncomingPacket(const unsigned char *buffer, const unsigned int received, PartialPacket &partialPacket) {
    if(partialPacket.hasHeader()) {
        unsigned int insert = received;
        
        if(partialPacket.getSize() + received >= partialPacket.getFullSize()) {
            insert = partialPacket.getFullSize() - partialPacket.getSize();
        }
        
        partialPacket.addData(buffer, insert);
        
        return insert;
    }
    
    else {
        unsigned int leftHeader = 4 - partialPacket.getSize();
        unsigned int adding = received > leftHeader ? leftHeader : received;
        
        partialPacket.addData(buffer, adding);
        return adding;
    }
}

void Threading::networkReceivingThread(NetworkCommunication &networkCommunication) {
    unsigned char *buffer = new unsigned char[NetworkCommunicationConstants::BUFFER_SIZE];
    
    while(true) {
        int received = recv(networkCommunication.getSocket(), buffer, NetworkCommunicationConstants::BUFFER_SIZE, 0);
        
        if(received <= 0) {
            break;
        }
                
        int processed = 0;
    
        do {
            processed += Threading::Private::composeIncomingPacket(buffer + processed, received - processed, networkCommunication.getPartialPacket());
        } while(processed < received);
        
        networkCommunication.moveCompletePartialPackets();
    }
    
    cout << "** ERROR **: receiving thread lost connection to server\n";
}

void Threading::networkSendingThread(NetworkCommunication &networkCommunication) {
    while(true) {
        Packet &packet = networkCommunication.getOutgoingPacket();
        
        int sent = send(networkCommunication.getSocket(), packet.getData() + packet.getSent(), packet.getSize() - packet.getSent(), 0);
        
        if(sent <= 0) {
            break;
        }
        
        packet.addSent(sent);
        
        if(packet.fullySent()) {
            networkCommunication.popOutgoingPacket();
        }
    }
    
    cout << "** ERROR **: sending thread lost connection to server\n";
}