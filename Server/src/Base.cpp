#include "Base.h"

System Base::system_;
Config Base::config_;
NetworkCommunication* Base::network_ = nullptr;

System& Base::system() {
	return system_;
}

Config& Base::config() {
	return config_;
}

NetworkCommunication& Base::network() {
	return *network_;
}

void Base::startNetwork(int port) {
	network_ = new NetworkCommunication(port);
}