#include "NetworkCommunication.h"
#include "Handle.h"
#include "Base.h"
#include "Packet.h"
#include "Connection.h"
#include "Config.h"
#include "Profile.h"
#include "System.h"

#include <iostream>
#include <algorithm>
#include <cmath>

// libcurlpp
#include <curlpp/cURLpp.hpp>

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

extern Connection* g_current_connection;
extern vector<double> g_customer_profile;

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
			vector<double> gains;

			bool factor_calibration = input_packet.getBool();
			int type = input_packet.getInt();
			int num_speakers = input_packet.getInt();
			int num_mics = input_packet.getInt();
			int num_gains = input_packet.getInt();

			for (int i = 0; i < num_speakers; i++)
				speakers.push_back(input_packet.getString());

			for (int i = 0; i < num_mics; i++)
				mics.push_back(input_packet.getString());

			for (int i = 0; i < num_gains; i++)
				gains.push_back(input_packet.getFloat());

			Handle::checkSoundImage(speakers, mics, gains, factor_calibration, type);

			break;
		}

		case PACKET_SET_BEST_EQ: {
			cout << "Warning: not implemented!\n";

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

		case PACKET_SET_SOUND_EFFECTS: {
			bool status = input_packet.getBool();
			int num = input_packet.getInt();
			vector<string> speakers;

			for (int i = 0; i < num; i++)
				speakers.push_back(input_packet.getString());

			Handle::setSoundEffects(speakers, status);

			break;
		}

		case PACKET_TESTING: Handle::testing();
			break;

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
	Base::config().parse("config");

	auto low_cutoff = Base::config().get<double>("hardware_profile_cutoff_low");
	auto high_cutoff = Base::config().get<double>("hardware_profile_cutoff_high");

	// Set profiles here for now
	Profile speaker;
	speaker.setCutoffs(low_cutoff, high_cutoff);

	Profile microphone;
	microphone.setCutoffs(low_cutoff, high_cutoff);

	// / 2.0 since we're doing shelf filter instead of steeping
	// Bring back steeping
	auto steep_low = Base::config().get<double>("hardware_profile_steep_low");//; / 2.0;
	auto steep_high = Base::config().get<double>("hardware_profile_steep_high");// / 2.0;

	speaker.setSteep(steep_low, steep_high);
	microphone.setSteep(steep_low, steep_high);

	vector<double> frequencies = Base::config().getAll<double>("dsp_eq"); //{ 62.5, 125, 250, 500, 1000, 2000, 4000, 8000, 16000 };
	double q = Base::config().get<double>("dsp_eq_q");
	auto string_type = Base::config().get<string>("dsp_eq_type");
	int type = 0;
	if (string_type == "parametric") {
		type = PARAMETRIC;
	} else if (string_type == "graphic") {
		type = GRAPHIC;
	} else if (string_type == "low_shelf") {
		type = LOW_SHELF;
	} else if (string_type == "high_shelf") {
		type = HIGH_SHELF;
	} else if (string_type == "low_pass") {
		type = LOW_PASS;
	} else if (string_type == "high_pass") {
		type = HIGH_PASS;
	} else if (string_type == "band_pass") {
		type = BAND_PASS;
	}

	speaker.setSpeakerEQ(frequencies, q);
	speaker.setMaxEQ(Base::config().get<double>("dsp_eq_max"));
	speaker.setMinEQ(Base::config().get<double>("dsp_eq_min"));

	for (size_t i = 0; i < frequencies.size(); i++)
		speaker.getFilter().addBand(lround(frequencies.at(i)), q, type);

	Base::system().setSpeakerProfile(speaker);
	Base::system().setMicrophoneProfile(microphone);

	g_customer_profile = Base::config().getAll<double>("customer_profile");

	// For testing
	if (Base::config().get<bool>("enable_testing")) {
		Handle::testing();
		return -1;
	}

	// Initialize curlpp
	curlpp::initialize();

	cout << "Starting server at port " << Base::config().get<unsigned short>("port") << endl;
	start();

	return 0;
}