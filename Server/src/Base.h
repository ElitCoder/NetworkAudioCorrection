#ifndef BASE_H
#define BASE_H

#include "System.h"
#include "Config.h"
#include "NetworkCommunication.h"

class Base {
public:
	static System& system();
	static Config& config();
	static NetworkCommunication& network();
	
	static void startNetwork(int port);
	
private:
	static System system_;
	static Config config_;
	static NetworkCommunication* network_;
};

#endif