#ifndef THREADING_H
#define THREADING_H

#include "NetworkCommunication.h"

namespace Threading {
    namespace Public {
        void connectToServer(const std::string &hostname, const unsigned short port, int &serverSocket);
    }
    
    namespace Private {
        unsigned int composeIncomingPacket(const unsigned char *buffer, const unsigned int received, PartialPacket &partialPacket);
    }
    
    void networkReceivingThread(NetworkCommunication &networkCommunication);
    void networkSendingThread(NetworkCommunication &networkCommunication);
}

#endif