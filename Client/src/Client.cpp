#include "NetworkCommunication.h"

#include <iostream>
#include <algorithm>
#include <cmath>

using namespace std;

enum {
	PACKET_SET_SPEAKER_VOLUME_AND_CAPTURE = 1,
	PACKET_START_LOCALIZATION,
	PACKET_PARSE_SERVER_CONFIG,
	PACKET_CHECK_SPEAKERS_ONLINE,
	PACKET_CHECK_SOUND_IMAGE,
	PACKET_SET_EQ,
	PACKET_SET_BEST_EQ,
	PACKET_SET_EQ_STATUS,
	PACKET_RESET_EVERYTHING
};

enum {
	SPEAKER_MIN_VOLUME = 0,				// 0 = -57 dB
	SPEAKER_MAX_VOLUME = 57,			// 57 = 0 dB
	SPEAKER_MIN_CAPTURE = 0,			// 0 = -12 dB
	SPEAKER_MAX_CAPTURE = 63,			// 63 = 35.25 dB
	SPEAKER_CAPTURE_BOOST_MUTE = 0,		// -inf dB
	SPEAKER_CAPTURE_BOOST_NORMAL = 1,	// 0 dB
	SPEAKER_CAPTURE_BOOST_ENABLED = 2	// +20 dB
};

static vector<string> g_freqs = {	"63",
									"125",
									"250",
									"500",
									"1000",
									"2000",
									"4000",
									"8000",
									"16000" };

static NetworkCommunication* g_network;

// Martin & Sofie
//static vector<string> g_ips = { "172.25.13.200", "172.25.9.38", "172.25.11.47", "172.25.12.99", "172.25.11.186" };
//static vector<string> g_external_microphones = {};

// Speakers
static vector<string> g_ips = { "172.25.9.38"  }; //, "172.25.9.38" };
// External microphones
static vector<string> g_external_microphones = { "172.25.12.99", "172.25.13.200" }; //, "172.25.12.99" };

/*
	Available:
	
	172.25.13.200
	172.25.9.38
	172.25.11.47
	172.25.12.99
	172.25.11.186
*/

static bool g_calibrated_sound_image = false;

Packet createSetSpeakerSettings(const vector<string>& ips, const vector<int>& volumes, const vector<int>& captures, const vector<int>& boosts) {
	Packet packet;
	packet.addHeader(PACKET_SET_SPEAKER_VOLUME_AND_CAPTURE);
	packet.addInt(ips.size());
	
	for (size_t i = 0; i < ips.size(); i++) {
		packet.addString(ips.at(i));
		packet.addInt(volumes.at(i));
		packet.addInt(captures.at(i));
		packet.addInt(boosts.at(i));
	}
	
	packet.finalize();
	return packet;
}

// Should this include microphones?
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

static void setSpeakerSettings(int speaker_volume, int speaker_capture, int speaker_boost) {
	vector<string> all_ips(g_ips);
	all_ips.insert(all_ips.end(), g_external_microphones.begin(), g_external_microphones.end());
	
	g_network->pushOutgoingPacket(createSetSpeakerSettings(all_ips, vector<int>(all_ips.size(), speaker_volume), vector<int>(all_ips.size(), speaker_capture), vector<int>(all_ips.size(), speaker_boost)));
	g_network->waitForIncomingPacket();
}

/*
static void setMaxSpeakerSettings() {
	setSpeakerSettings(SPEAKER_MAX_VOLUME, SPEAKER_MAX_CAPTURE, SPEAKER_CAPTURE_BOOST_ENABLED);
}
*/

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
	
	cout << "Resetting... \t" << flush;
	g_network->pushOutgoingPacket(createResetEverything(all_ips));
	g_network->waitForIncomingPacket();
	cout << "done\n\n";
}

void startSpeakerLocalization(const vector<string>& ips, bool force) {
	resetEverything();
	
	/*
	cout << "Setting max speaker settings.. " << flush;
	//setMaxSpeakerSettings();
	cout << "done\n";
	*/
	
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

Packet createParseServerConfig() {
	Packet packet;
	packet.addHeader(PACKET_PARSE_SERVER_CONFIG);
	packet.finalize();
	
	return packet;
}

void parseServerConfig() {
	cout << "Rebuild config files.. ";
	g_network->pushOutgoingPacket(createParseServerConfig());
	g_network->waitForIncomingPacket();
	cout << "done!\n\n";
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

Packet createSoundImage(const vector<string>& speakers, const vector<string>& mics, bool corrected, int num_iterations) {
	Packet packet;
	packet.addHeader(PACKET_CHECK_SOUND_IMAGE);
	packet.addBool(corrected);
	packet.addInt(speakers.size());
	packet.addInt(mics.size());
	packet.addInt(1);	// Play time
	packet.addInt(1);	// Idle time
	packet.addInt(num_iterations);
	
	for (auto& ip : speakers)
		packet.addString(ip);
		
	for (auto& ip : mics)
		packet.addString(ip);
		
	packet.finalize();
	return packet;
}

void soundImage(bool corrected) {
	if (!corrected) {
		cout << "Resetting speaker settings... \t" << flush;
		setSpeakerSettings(SPEAKER_MAX_VOLUME, SPEAKER_MAX_CAPTURE, SPEAKER_CAPTURE_BOOST_ENABLED);
		cout << "done\n";
	}
	
	int num_iterations = 1;
	
	if (corrected) {
		cout << "Number of correction iterations: ";
		cin >> num_iterations;
		
		// If flat is not determined, run calibration first
		if (!g_calibrated_sound_image)
			soundImage(false);
		
		for (int i = 0; i < num_iterations; i++) {
			double percent = (i + 1) / (double)num_iterations;
			
			cout << "Running sound image tests... " << flush;//" << (percent * 100.0) << "% done\n" << flush;
			
			g_network->pushOutgoingPacket(createSoundImage(g_ips, g_external_microphones, corrected, 1));
			auto answer = g_network->waitForIncomingPacket(); 
			answer.getByte();
			
			// Check current score
			int num_mics = answer.getInt();
			
			cout << lround(percent * 100.0) << "% done \tscores: ";
			
			for (int j = 0; j < num_mics; j++) {
				auto ip = answer.getString();
				auto num_dbs = answer.getInt();
				
				for (int k = 0; k < num_dbs; k++)
					answer.getFloat();
					
				auto score = answer.getFloat();
				
				cout << score << " ";
			}
			
			cout << "\n";
		}
		
		cout << "\nDone, use set best EQ to set all speakers to optimal settings!\n\n";
	} else {
		cout << "Analyzing individual speaker profiles... \t" << flush;
		g_network->pushOutgoingPacket(createSoundImage(g_ips, g_external_microphones, corrected, 1));
		g_network->waitForIncomingPacket();
		cout << "done\n\n";
		
		g_calibrated_sound_image = true;
	}
	/*
	if (corrected)
		cout << "Trying corrected sound image... " << flush;
	else	
		cout << "Trying sound image... " << flush;
		
	g_network->pushOutgoingPacket(createSoundImage(g_ips, g_external_microphones, corrected, num_iterations));
	auto answer = g_network->waitForIncomingPacket();
	answer.getByte();
	cout << "done\n\n";
	
	int num_mics = answer.getInt();
	
	for (int i = 0; i < num_mics; i++) {
		string ip = answer.getString();
		int db_size = answer.getInt();
		vector<double> fft_db;
		
		cout << "Microphone " << ip << endl;
		double total_db_fft = 0;
		
		for (int j = 0; j < db_size - 1; j++) {
			double db = answer.getFloat();
			total_db_fft += db;
			fft_db.push_back(db);
			
			cout << "Frequency " << g_freqs[j] << " = " << db << " dB\n";
		}
		
		double total_db = answer.getFloat();
		double db_fft_mean = total_db_fft / (db_size - 1);
		
		double score = answer.getFloat();
		
		cout << "Total mean dB in FFT: " << db_fft_mean << " dB\n";
		cout << "Total dB in time domain: " << total_db << " dB\n\n";
		cout << "Score (the higher the better): " << score << endl;
	}
	*/
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
	cout << "done\n\n" << flush;
	
	auto num = answer.getInt();
	
	for (int i = 0; i < num; i++) {
		double score = answer.getFloat();
		cout << "Score speaker index " << i << " \t" << score << endl;
	}
	
	cout << endl;
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

void run(const string& host, unsigned short port) {
	cout << "Connecting to server.. ";
	NetworkCommunication network(host, port);
	cout << "done!\n\n";
	
	g_network = &network;
	
	while (true) {
		cout << "1. Check if speakers are online (also enables SSH)\n";
		cout << "2. Reparse server config\n\n";
		cout << "3. Start speaker localization script (only speakers)\n";
		cout << "4. Start speaker localization script (only speakers, force update)\n\n";
		cout << "5. Start speaker localization script (all IPs)\n";
		cout << "6. Start speaker localization script (all IPs, force update)\n\n";
		//cout << "7. Check sound image\n";
		//cout << "8. Check corrected sound image\n";
		cout << "7. Calibrate sound image & run correction tests\n";
		cout << "8. Set best EQ\n\n";
		
		cout << "9. Enable EQ in all speakers\n";
		cout << "10. Disable EQ in all speakers\n";
		cout << "11. Set to speaker defaults (all IPs)\n";
		cout << "\n: ";
		
		int input;
		cin >> input;
		
		cout << endl;
		
		switch (input) {
			case 1: speakersOnline();
				break;
				
			case 2: parseServerConfig();
				break;
				
			case 3: startSpeakerLocalization(g_ips, false);
				break;
				
			case 4: startSpeakerLocalization(g_ips, true);
				break;	
				
			case 5: startSpeakerLocalizationAll(false);
				break;
				
			case 6: startSpeakerLocalizationAll(true);
				break;	
				
			case 7: soundImage(true);
				break;
				
			case 8: bestEQ();
				break;
				
			case 9: setEQStatus(true);
				break;
				
			case 10: setEQStatus(false);
				break;
				
			case 11: resetEverything();
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