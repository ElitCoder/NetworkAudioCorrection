#include "NetworkCommunication.h"
#include "Packet.h"

#include <iostream>
#include <algorithm>
#include <cmath>
#include <vector>

using namespace std;

enum {
	PACKET_START_LOCALIZATION = 1,
	PACKET_CHECK_SPEAKERS_ONLINE,
	PACKET_CHECK_SOUND_IMAGE,
	PACKET_CHECK_SOUND_IMAGE_WHITE,
	PACKET_SET_BEST_EQ,
	PACKET_SET_EQ_STATUS,
	PACKET_RESET_EVERYTHING,
	PACKET_SET_SOUND_EFFECTS,
	PACKET_TESTING
};

enum {
	WHITE_NOISE,
	NINE_FREQ
};

static NetworkCommunication* g_network;
/*
	Available:
	
	172.25.13.200
	172.25.13.250
	172.25.9.38
	172.25.11.47
	172.25.12.99
	172.25.11.186
	172.25.15.12 <- fucked?
*/
//"172.25.15.12"
//"172.25.14.41" //
// Speakers
static vector<string> g_ips = { "172.25.14.27" }; //, "172.25.9.38", "172.25.12.168", "172.25.13.250", "172.25.11.47" }; //"172.25.15.109", "172.25.14.21" //"172.25.9.38", "172.25.12.99", "172.25.11.47", "172.25.13.250",
// External microphones
static vector<string> g_external_microphones = { "172.25.15.233", "172.25.13.82" }; //, "172.25.13.82" };

Packet createStartSpeakerLocalization(const vector<string>& ips, bool force) {
	Packet packet;
	packet.addHeader(PACKET_START_LOCALIZATION);
	packet.addBool(force);
	packet.addInt(ips.size());
	
	for (auto& ip : ips)
		packet.addString(ip);
		
	packet.finalize();
	return packet;
}

Packet createResetEverything(const vector<string>& ips) {
	Packet packet;
	packet.addHeader(PACKET_RESET_EVERYTHING);
	packet.addInt(ips.size());
	
	for (auto& ip : ips)
		packet.addString(ip);
		
	packet.finalize();
	return packet;
}

void resetEverything() {
	vector<string> all_ips(g_ips);
	all_ips.insert(all_ips.end(), g_external_microphones.begin(), g_external_microphones.end());
	
	cout << "Resetting...\t " << flush;
	g_network->pushOutgoingPacket(createResetEverything(all_ips));
	g_network->waitForIncomingPacket();
	cout << "done\n\n";
}

void startSpeakerLocalization(const vector<string>& ips, bool force) {
	cout << "Running speaker localization script.. " << flush;
	g_network->pushOutgoingPacket(createStartSpeakerLocalization(ips, force));
	auto answer = g_network->waitForIncomingPacket();
	answer.getByte();
	cout << "done\n\n";
	
	// Parse data
	int num_speakers = answer.getInt();
	
	for (int i = 0; i < num_speakers; i++) {
		string own_ip = answer.getString();
		cout << own_ip << ": ";
		
		int num_dimensions = answer.getInt();
		
		cout << "(";
		for (int j = 0; j < num_dimensions; j++) {
			cout << answer.getFloat();
			
			if (j + 1 != num_dimensions)
				cout << ", ";
		}
		cout << ")\n";
			
		cout << "With distances:\n";
		int num_distances = answer.getInt();
		
		for (int j = 0; j < num_distances; j++) {
			string ip = answer.getString();
			double distance = answer.getFloat();
			
			cout << own_ip << " -> " << ip << "\t= " << distance << " m\n";
		}
		
		cout << endl;	
	}
}

void startSpeakerLocalizationAll(bool force) {
	vector<string> all_ips(g_ips);
	all_ips.insert(all_ips.end(), g_external_microphones.begin(), g_external_microphones.end());
	
	startSpeakerLocalization(all_ips, force);
}

Packet createCheckSpeakerOnline(const vector<string>& ips) {
	Packet packet;
	packet.addHeader(PACKET_CHECK_SPEAKERS_ONLINE);
	packet.addInt(ips.size());
	
	for (auto& ip : ips)
		packet.addString(ip);
		
	packet.finalize();
	return packet;
}

void speakersOnline() {
	vector<string> all_ips(g_ips);
	all_ips.insert(all_ips.end(), g_external_microphones.begin(), g_external_microphones.end());
	
	cout << "Trying speakers.. " << flush;
	g_network->pushOutgoingPacket(createCheckSpeakerOnline(all_ips));
	auto answer = g_network->waitForIncomingPacket();
	cout << "done!\n\n";
	
	answer.getByte();
	int num_speakers = answer.getInt();
	
	for (int i = 0; i < num_speakers; i++) {
		string ip = answer.getString();
		bool online = answer.getBool();
		
		if (find(g_external_microphones.begin(), g_external_microphones.end(), ip) != g_external_microphones.end())
			cout << "(Listening) ";
		else
			cout << "(Playing) ";
		
		cout << ip << " is " << (online ? "online" : "NOT online") << endl;
	}
	
	cout << endl;
}

Packet createSoundImage(const vector<string>& speakers, const vector<string>& mics, bool factor_calibration, int type) {
	Packet packet;
	packet.addHeader(PACKET_CHECK_SOUND_IMAGE);
	packet.addBool(factor_calibration);
	packet.addInt(type);
	packet.addInt(speakers.size());
	packet.addInt(mics.size());
	
	for (auto& ip : speakers)
		packet.addString(ip);
		
	for (auto& ip : mics)
		packet.addString(ip);
		
	packet.finalize();
	return packet;
}

void soundImage(int type) {
	char answer;
	cout << "Should factor calibration run? (Y/N): ";
	cin >> answer;
	
	switch (type) {
		case WHITE_NOISE: cout << "\n(White noise)";
			break;
			
		case NINE_FREQ: cout << "\n(9-freq)";
			break;
	}
	
	cout << "\nRunning sound image correction...\t" << flush;
	g_network->pushOutgoingPacket(createSoundImage(g_ips, g_external_microphones, answer == 'Y', type));
	g_network->waitForIncomingPacket();
	cout << "done\n\n";
}

Packet createBestEQ(const vector<string>& speakers, const vector<string>& mics) {
	Packet packet;
	packet.addHeader(PACKET_SET_BEST_EQ);
	packet.addInt(speakers.size());
	packet.addInt(mics.size());
	
	for (auto& ip : speakers)
		packet.addString(ip);
		
	for (auto& ip : mics)
		packet.addString(ip);
				
	packet.finalize();
	return packet;
}

void bestEQ() {
	cout << "Setting best EQ... " << flush;
	g_network->pushOutgoingPacket(createBestEQ(g_ips, g_external_microphones));
	auto answer = g_network->waitForIncomingPacket();
	answer.getByte();
	cout << "done\n\n";
}

Packet createSetEQStatus(const vector<string>& ips, bool status) {
	Packet packet;
	packet.addHeader(PACKET_SET_EQ_STATUS);
	packet.addBool(status);
	packet.addInt(ips.size());
	
	for (auto& ip : ips)
		packet.addString(ip);
		
	packet.finalize();
	return packet;
}

void setEQStatus(bool status) {
	cout << (status ? "Enabling" : "Disabling") << " EQ in speakers... \t" << flush;
	g_network->pushOutgoingPacket(createSetEQStatus(g_ips, status));
	g_network->waitForIncomingPacket();
	cout << "done\n\n";
}

Packet createSetSoundEffects(const vector<string>& ips, bool status) {
	Packet packet;
	packet.addHeader(PACKET_SET_SOUND_EFFECTS);
	packet.addBool(status);
	packet.addInt(ips.size());
	
	for (auto& ip : ips)
		packet.addString(ip);
		
	packet.finalize();
	return packet;
}

void setSoundEffects(bool status) {
	cout << (status ? "Enabling" : "Disabling") << " Axis sound effects... \t" << flush;
	g_network->pushOutgoingPacket(createSetSoundEffects(g_ips, status));
	g_network->waitForIncomingPacket();
	cout << "done\n\n";
}

Packet createTesting() {
	Packet packet;
	packet.addHeader(PACKET_TESTING);
	packet.finalize();
	return packet;
}

void testing() {
	cout << "Running testing method in server.. \t" << flush;
	g_network->pushOutgoingPacket(createTesting());
	g_network->waitForIncomingPacket();
	cout << "done\n\n";
}

void run(const string& host, unsigned short port) {
	cout << "Connecting to server.. ";
	NetworkCommunication network(host, port);
	cout << "done!\n\n";
	
	g_network = &network;
	
	while (true) {
		cout << "1. Check if speakers are online (also enables SSH)\n\n";
		cout << "2. Start speaker localization script (only speakers)\n";
		cout << "3. Start speaker localization script (only speakers, force update)\n\n";
		cout << "4. Start speaker localization script (all IPs)\n";
		cout << "5. Start speaker localization script (all IPs, force update)\n\n";
		
		cout << "6. Calibrate sound image (9-freq)\n";
		cout << "7. Calibrate sound image (white noise)\n";
		cout << "8. Set best EQ\n\n";
		
		cout << "9. Enable EQ in all speakers\n";
		cout << "10. Disable EQ in all speakers\n";
		cout << "11. Enable Axis sound effects\n";
		cout << "12. Disable Axis sound effects\n\n";
		cout << "13. Set to speaker defaults (all IPs)\n";
		cout << "\n: ";
		
		int input;
		cin >> input;
		
		cout << endl;
		
		switch (input) {
			case 1: speakersOnline();
				break;
				
			case 2: startSpeakerLocalization(g_ips, false);
				break;
				
			case 3: startSpeakerLocalization(g_ips, true);
				break;	
				
			case 4: startSpeakerLocalizationAll(false);
				break;
				
			case 5: startSpeakerLocalizationAll(true);
				break;	
				
			case 6: soundImage(NINE_FREQ);
				break;
				
			case 7: soundImage(WHITE_NOISE);
				break;
				
			case 8: bestEQ();
				break;
				
			case 9: setEQStatus(true);
				break;
				
			case 10: setEQStatus(false);
				break;
				
			case 11: setSoundEffects(true);
				break;
				
			case 12: setSoundEffects(false);
				break;
				
			case 13: resetEverything();
				break;
				
			case 99: testing();
				break;
				
			default: cout << "Wrong input format!\n";
		}
		
		cout << endl;
	}
	
	g_network = nullptr;
}

int main() {
	const string HOST = "localhost";
	const unsigned short PORT = 10200;
	
	run(HOST, PORT);
	
	return 0;
}