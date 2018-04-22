#include "NetworkCommunication.h"
#include "Handle.h"
#include "Base.h"
#include "Packet.h"
#include "Connection.h"
#include "Config.h"

#include <iostream>
#include <algorithm>

// libcurlpp
#include <curlpp/cURLpp.hpp>

using namespace std;

enum {
	PACKET_START_LOCALIZATION = 1,
	PACKET_CHECK_SPEAKERS_ONLINE,
	PACKET_CHECK_SOUND_IMAGE,
	PACKET_SET_BEST_EQ,
	PACKET_SET_EQ_STATUS,
	PACKET_RESET_EVERYTHING
};

extern Connection* g_current_connection;

static void handle(Connection& connection, Packet& input_packet) {
	g_current_connection = &connection;
	
	auto header = input_packet.getByte();
	
	printf("Debug: got packet with header %02X\n", header);
	
	Packet packet;
	packet.addHeader(header);
	
	switch (header) {
		case 0x00: {
			string name = input_packet.getString();
			cout << "Information: client name is " << name << endl;
	
			packet.addBool(true);
			break;
		}
		
		case PACKET_START_LOCALIZATION: {
			vector<string> ips;
			bool force_update = input_packet.getBool();
			int num_ips = input_packet.getInt();
			
			for (int i = 0; i < num_ips; i++)
				ips.push_back(input_packet.getString());
				
			auto placements = Handle::runLocalization(ips, force_update);
			
			packet.addInt(placements.size());
			
			for (auto& speaker : placements) {
				packet.addString(get<0>(speaker));
				
				auto& coordinates = get<1>(speaker);
				packet.addInt(coordinates.size());
				for_each(coordinates.begin(), coordinates.end(), [&packet] (double c) { packet.addFloat(c); });
				
				auto& distances = get<2>(speaker);
				packet.addInt(distances.size());
				for_each(distances.begin(), distances.end(), [&packet] (const pair<string, double>& peer) {
					packet.addString(peer.first);
					packet.addFloat(peer.second);
				});
			}
			
			break;
		}
		
		case PACKET_RESET_EVERYTHING: {
			int num_speakers = input_packet.getInt();
			vector<string> ips;
			
			for (int i = 0; i < num_speakers; i++)
				ips.push_back(input_packet.getString());
				
			Handle::resetIPs(ips);
			
			break;	
		}
		
		case PACKET_CHECK_SPEAKERS_ONLINE: {
			int num_speakers = input_packet.getInt();
			vector<string> ips;
			
			for (int i = 0; i < num_speakers; i++)
				ips.push_back(input_packet.getString());
				
			auto answer = Handle::checkSpeakersOnline(ips);

			packet.addInt(num_speakers);
			
			for (int i = 0; i < num_speakers; i++) {
				packet.addString(ips.at(i));
				packet.addBool(answer.at(i));
			}
			
			break;
		}
		
		case PACKET_CHECK_SOUND_IMAGE: {
			vector<string> speakers;
			vector<string> mics;
			
			bool factor_calibration = input_packet.getBool();
			int num_speakers = input_packet.getInt();
			int num_mics = input_packet.getInt();
			
			for (int i = 0; i < num_speakers; i++)
				speakers.push_back(input_packet.getString());
				
			for (int i = 0; i < num_mics; i++)
				mics.push_back(input_packet.getString());

			Handle::checkSoundImage(speakers, mics, factor_calibration);
			
			break;
		}
		
		case PACKET_SET_BEST_EQ: {
			vector<string> speakers;
			vector<string> mics;
			int num_speakers = input_packet.getInt();
			int num_mics = input_packet.getInt();
			
			for (int i = 0; i < num_speakers; i++)
				speakers.push_back(input_packet.getString());
				
			for (int i = 0; i < num_mics; i++)
				mics.push_back(input_packet.getString());	
				
			Handle::setBestEQ(speakers, mics);
			
			break;
		}
		
		case PACKET_SET_EQ_STATUS: {
			bool status = input_packet.getBool();
			int num = input_packet.getInt();
			vector<string> speakers;
			
			for (int i = 0; i < num; i++)
				speakers.push_back(input_packet.getString());
				
			Handle::setEQStatus(speakers, status);
			
			break;	
		}
		
		default:	cout << "Debug: got some random packet, answering with empty packet\n";
					cout << "Debug: header " << header << endl;
	}
	
	packet.finalize();
	Base::network().addOutgoingPacket(connection.getSocket(), packet);
}

static void start() {
	Base::startNetwork(Base::config().get<int>("port"));
	auto& network = Base::network();
	
	while (true) {
		auto* packet = network.waitForProcessingPackets();
		
		if (packet == nullptr)
			continue;
			
		auto* connection_pair = network.getConnectionAndLock(packet->first);
		
		if (connection_pair == nullptr) {
			network.removeProcessingPacket();
			
			continue;
		}
		
		handle(connection_pair->second, packet->second);
		
		network.unlockConnection(*connection_pair);
		network.removeProcessingPacket();
	}
}

int main() {
	// Initialize curlpp
	curlpp::initialize();
	
	Base::config().parse("config");
	
	cout << "Starting server at port " << Base::config().get<unsigned short>("port") << endl;
	start();
	
	return 0;
}