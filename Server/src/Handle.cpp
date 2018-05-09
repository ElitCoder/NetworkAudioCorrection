#include "Handle.h"
#include "WavReader.h"
#include "Localization3D.h"
#include "Goertzel.h"
#include "Base.h"
#include "Packet.h"
#include "Connection.h"
#include "Speaker.h"
#include "System.h"
#include "Config.h"
#include "Analyze.h"

#include <iostream>
#include <cmath>
#include <algorithm>
#include <climits>
#include <fstream>

using namespace std;

enum {
	SPEAKER_MIN_VOLUME = 0,				// 0 = -57 dB
	SPEAKER_MAX_VOLUME = 57,			// 57 = 0 dB
	SPEAKER_MIN_CAPTURE = 0,			// 0 = -12 dB
	SPEAKER_MAX_CAPTURE = 63,			// 63 = 35.25 dB
	SPEAKER_CAPTURE_BOOST_MUTE = 0,		// -inf dB
	SPEAKER_CAPTURE_BOOST_NORMAL = 1,	// 0 dB
	SPEAKER_CAPTURE_BOOST_ENABLED = 2	// +20 dB
};

enum {
	TYPE_BEST_EQ,
	TYPE_NEXT_EQ,
	TYPE_FLAT_EQ,
	TYPE_WHITE_EQ
};

enum {
	WHITE_NOISE,
	NINE_FREQ
};

// We're not multithreading anyway
Connection* g_current_connection = nullptr;

static vector<string> g_frequencies =	{	"63",
											"125",
											"250",
											"500",
											"1000",
											"2000",
											"4000",
											"8000",
											"16000"	};

// Customer's profile, just keep it flat for now
// Flat EQ, calibrate towards flat and add different EQs on top of that
vector<double> g_normalization_profile = { 0, 0, 0, 0, 0, 0, 0, 0, 0 };

// Axis own music EQ with adjustments
// After calibration customer profile
vector<double> g_customer_profile = { 5, 3, 2, -2, -1, -3, -2, 1, 1 };

double g_target_db_level = 0;

// The following function is from SO
constexpr char hexmap[] = {	'0', '1', '2', '3', '4', '5', '6', '7',
                           	'8', '9', 'a', 'b', 'c', 'd', 'e', 'f'	};
							
string getHexString(unsigned char* data, int len) {
	string s(len * 2, ' ');
	
	for (int i = 0; i < len; ++i) {
		s[2 * i]     = hexmap[(data[i] & 0xF0) >> 4];
		s[2 * i + 1] = hexmap[data[i] & 0x0F];
	}
	
	return s;
}

/* converts dB to linear gain */
double dB_to_linear_gain(double x) {
    return pow(10,x/20);
}

void to_523(double param_dec, unsigned char * param_hex) {
	long param223;
	long param227;

	//multiply decimal number by 2^23
	param223 = param_dec * (1<<23);
	// convert to positive binary
	param227 = param223 + (1<<27);

	param_hex[3] = param227 & 0xFF;       //byte 3 (LSB) of parameter value
	param_hex[2] = (param227>>8) & 0xFF;  //byte 2 of parameter value
	param_hex[1] = (param227>>16) & 0xFF; //byte 1 of parameter value
	param_hex[0] = (param227>>24) & 0xFF; //byte 0 (MSB) of parameter value

	// invert sign bit to get correct sign
	param_hex[0] = param_hex[0] ^ 0x08;
}

static void enableAudioSystem(const vector<string>& ips) {
	cout << "Starting audio system\n";
	Base::system().runScript(ips, vector<string>(ips.size(), "systemctl start audio_relayd; wait\n"));
}

static void disableAudioSystem(const vector<string>& ips) {
	cout << "Stopping audio system\n";
	Base::system().runScript(ips, vector<string>(ips.size(), "systemctl stop audio*; wait; killall -9 audio-netsend; wait\n"));
}

// It's like we were not here
void resetEverything(const vector<string>& ips) {
	string command = 	"dspd -s -w; wait; ";
	command +=			"amixer -c1 sset 'Headphone' 57 on; wait; "; 				/* 57 is 0 dB for C1004-e */
	command +=			"amixer -c1 sset 'Capture' 63; wait; ";
	command +=			"dspd -s -p flat; wait; ";
	command +=			"amixer -c1 cset numid=170 0x00,0x80,0x00,0x00; wait; "; 	/* Sets DSP gain to 0 */
	command +=			"amixer -c1 sset 'PGA Boost' 2; wait\n";
	
	Base::system().runScript(ips, vector<string>(ips.size(), command));
	
	// Set system speaker settings as well
	for (auto* speaker : Base::system().getSpeakers(ips)) {
		speaker->setVolume(SPEAKER_MAX_VOLUME);
		speaker->clearAllEQs();
	}
}

static void setTestSpeakerSettings(const vector<string>& ips) {
	string command =	"dspd -s -w; wait; dspd -s -m; wait; dspd -s -u limiter; wait; dspd -s -u static; wait; ";
	command +=			"dspd -s -u preset; wait; dspd -s -p flat; wait; ";
	command +=			"amixer -c1 sset 'Headphone' 57 on; wait; amixer -c1 sset 'Capture' 63; wait; amixer -c1 sset 'PGA Boost' 1; wait; ";
	command +=			"amixer -c1 cset numid=170 0x00,0x80,0x00,0x00; wait\n";		/* Sets DSP gain to 0 */
	
	Base::system().runScript(ips, vector<string>(ips.size(), command));
	
	// Set system speaker settings as well
	for (auto* speaker : Base::system().getSpeakers(ips)) {
		speaker->setVolume(SPEAKER_MAX_VOLUME);
		speaker->clearAllEQs();
	}
}

static vector<string> createRunLocalizationScripts(const vector<string>& ips, int play_time, int idle_time, const string& file) {
	vector<string> scripts;
	
	for (size_t i = 0; i < ips.size(); i++) {
		string script =	"arecord -D audiosource -r 48000 -f S16_LE -c 1 -d ";
		script +=		to_string(idle_time + ips.size() * (idle_time + play_time) + idle_time);
		script +=		" /tmp/cap";
		script +=		ips.at(i);
		script +=		".wav &\n";
		script +=		"proc1=$!\n";
		script +=		"sleep ";
		script +=		to_string(idle_time + i * (play_time + idle_time));
		script +=		"\n";
		script +=		"aplay -D localhw_0 -r 48000 -f S16_LE /tmp/";
		script += 		file;
		script +=		"\n";
		script +=		"wait $proc1\n";
		
		scripts.push_back(script);
	}
	
	return scripts;
}

static PlacementOutput assemblePlacementOutput(const vector<Speaker*> speakers) {
	PlacementOutput output;
	
	cout << "Placement:\n";
	
	for (auto* speaker : speakers) {
		auto ip = speaker->getIP();
		const auto& coordinates = speaker->getPlacement().getCoordinates();
		const auto& distances = speaker->getPlacement().getDistances();
		
		//array<double, 3> coordinates = {{ 0 }};
		//vector<pair<string, double>> distances = { { ip, 0 } };
		
		output.push_back(make_tuple(ip, coordinates, distances));
		
		cout << "(" << coordinates.front() << ", " << coordinates.at(1) << ")\n";
	}
	
	return output;
}

PlacementOutput Handle::runLocalization(const vector<string>& ips, bool force_update) {
	if (ips.empty())
		return PlacementOutput();

	cout << "Running localization\n";	
		
	// Does the server already have relevant positions?
	vector<int> placement_ids;
	auto speakers = Base::system().getSpeakers(ips);
	
	for (auto* speaker : speakers)
		placement_ids.push_back(speaker->getPlacementID());
	
	if (adjacent_find(placement_ids.begin(), placement_ids.end(), not_equal_to<int>()) == placement_ids.end() && placement_ids.front() >= 0 && !force_update) {
		cout << "Server already have relevant position info, returning that\n";
		
		return assemblePlacementOutput(speakers);
	}
	
	if (!Base::config().get<bool>("no_scripts")) {
		// Create scripts
		int play_time = Base::config().get<int>("play_time_localization");
		int idle_time = Base::config().get<int>("idle_time");
		
		auto scripts = createRunLocalizationScripts(ips, play_time, idle_time, Base::config().get<string>("goertzel"));
		
		// Disable audio system
		disableAudioSystem(ips);
		
		// Send test files
		Base::system().sendFile(ips, "data/" + Base::config().get<string>("goertzel"), "/tmp/");
		
		// Set test speaker settings
		setTestSpeakerSettings(ips);
		
		// Start localization scripts
		Base::system().runScript(ips, scripts);
		
		// Reset speaker settings
		resetEverything(ips);
		
		// Collect data
		Base::system().getRecordings(ips);
		
		// Enable audio system again
		enableAudioSystem(ips);
	}
	
	auto distances = Goertzel::runGoertzel(ips);
	
	if (distances.empty())
		return PlacementOutput();
	
	auto placement = Localization3D::run(distances, Base::config().get<bool>("fast"));
	
	// Keep track of which localization this is
	static int placement_id = -1;
	placement_id++;
	
	for (size_t i = 0; i < ips.size(); i++) {
		Speaker::SpeakerPlacement speaker_placement(ips.at(i));
		auto& master = distances.at(i);
		
		for (size_t j = 0; j < master.second.size(); j++)
			speaker_placement.addDistance(ips.at(j), master.second.at(j));

		speaker_placement.setCoordinates(placement.at(i));
		
		Base::system().getSpeaker(ips.at(i)).setPlacement(speaker_placement, placement_id);
	}
	
	return assemblePlacementOutput(speakers);
}

vector<bool> Handle::checkSpeakersOnline(const vector<string>& ips) {
	auto speakers = Base::system().getSpeakers(ips);
	vector<bool> online;
	
	for (auto* speaker : speakers)
		online.push_back(speaker->isOnline());
	
	return online;
}

static vector<double> getFFT9(const vector<short>& data, size_t start, size_t end) {
	vector<short> sound(data.begin() + start, data.begin() + end);
	
	// Let's do 9 Goertzel calculations to get the center frequencies
	vector<double> dbs;
	
	for (auto& frequency_string : g_frequencies)
		dbs.push_back(goertzel(sound.size(), stoi(frequency_string), 48000, sound.data()));
		
	return dbs;
}

static vector<double> getSoundImageCorrection(vector<double> dbs, bool white_noise) {
	vector<double> eq;
	
	for (size_t i = 0; i < dbs.size(); i++) {
		// Get score against chosen profile
		double profile_db = g_target_db_level + g_normalization_profile.at(i);
		double db = dbs.at(i);
		double difference = profile_db - db;

		eq.push_back(difference);
	}
	
	if (white_noise) {}
	
	#if 0
	if (white_noise) {
		// Close bands affect eachother when EQ:ing
		vector<double> final_eq(eq);
		
		for (size_t i = 0; i < eq.size() - 1; i++) {
			final_eq.at(i) -= (eq.at(i + 1) / 6);
		}
		
		eq = final_eq;
	}
	#endif
	
	return eq;
}

static void setSpeakerVolume(const string& ip, double volume, double base_dsp_level) {
	auto& speaker = Base::system().getSpeaker(ip);	
	speaker.setVolume(volume);
	
	cout << "Setting speaker volume to " << speaker.getVolume() << endl;
	double delta = speaker.getVolume() - SPEAKER_MAX_VOLUME;
	cout << "Delta: " << delta << endl;
	
	// -18 gives the speaker 6 dB headroom to boost before DSP limiting on maxed EQ
	double final_level = base_dsp_level + delta;
	
	// Don't boost above limit
	if (final_level > 0) {
		cout << "WARNING: Trying to boost DSP gain above 0 dB, it's " << final_level << endl;
		final_level = 0;
	}
	
	unsigned char bytes[4];
	to_523(dB_to_linear_gain(final_level), bytes);
	string hex_string = getHexString(bytes, 4);
	string hex_bytes = "";
	
	cout << "Setting DSP gain bytes to " << hex_string << endl;
	cout << "For level " << final_level << " dB\n";
	
	for (size_t i = 0; i < hex_string.length(); i += 2) 
		hex_bytes += "0x" + hex_string.substr(i, 2) + ",";
		
	hex_bytes.pop_back();	
	
	string command =	"amixer -c1 cset numid=170 ";
	command +=			hex_bytes;
	command +=			"; wait\n";
	
	Base::system().runScript({ ip }, { command });
}

static void setSpeakersEQ(const vector<string>& speaker_ips, int type) {
	auto speakers = Base::system().getSpeakers(speaker_ips);
	vector<string> commands;
	
	// Wanted by best EQ
	double loudest_gain = INT_MIN;
	double loudest_volume = INT_MIN;
	
	for (auto* speaker : speakers) {
		auto total = speaker->getBestVolume() + max(0.0, speaker->getLoudestBestEQ());
		auto volume = speaker->getBestVolume();
		
		if (total > loudest_gain)
			loudest_gain = total;
			
		if (volume > loudest_volume)
			loudest_volume = volume;
	}
	
	for (auto* speaker : speakers) {
		vector<double> eq;
		
		switch (type) {
			case TYPE_BEST_EQ: case TYPE_WHITE_EQ: {
				eq = speaker->getBestEQ();
				speaker->setBestVolume();
				
				break;
			}
			
			case TYPE_NEXT_EQ: {
				eq = speaker->getNextEQ();
				speaker->setNextVolume();
				
				break;
			}
			
			case TYPE_FLAT_EQ: {
				eq = vector<double>(9, 0);
				speaker->setVolume(SPEAKER_MAX_VOLUME);
			}
		}
		
		string command = "dspd -s -u preset; wait; dspd -s -e ";
		
		for (auto setting : eq)
			command += to_string(setting) + ",";
			
		command.pop_back();
		command +=		"; wait\n";
		
		commands.push_back(command);
		
		double dsp_gain;
		
		switch (type) {
			case TYPE_FLAT_EQ: dsp_gain = Base::config().get<double>("calibration_safe_gain");
				break;
				
			case TYPE_NEXT_EQ: dsp_gain = -15; // More headroom for increasing the volume while finding factors (4 steps = 12 dB)
				break;
				
			// We don't know which limits the speaker will set during calibration, so safe it and say it will max the EQ (at -12)
			case TYPE_WHITE_EQ: dsp_gain = Base::config().get<double>("calibration_safe_gain") + (SPEAKER_MAX_VOLUME - loudest_volume);
				break;
				
			case TYPE_BEST_EQ: dsp_gain = SPEAKER_MAX_VOLUME - loudest_gain;
				break;
		}
		
		setSpeakerVolume(speaker->getIP(), speaker->getVolume(), dsp_gain);
	}
	
	Base::system().runScript(speaker_ips, commands);
}

static vector<vector<double>> weightEQs(const vector<string>& speaker_ips, const vector<string>& mic_ips, const MicWantedEQ& eqs) {
	auto num_bands = Base::system().getSpeakerProfile().getNumEQBands();
	
	// Initialize to 0 EQ
	vector<vector<double>> final_eqs(eqs.front().size(), vector<double>(num_bands, 0));
	vector<vector<double>> total_linear_energy = vector<vector<double>>(speaker_ips.size(), vector<double>(num_bands, 0));
	
	// Find total linear energy
	for (int i = 0; i < num_bands; i++) {
		// Go through microphones
		for (size_t j = 0; j < eqs.size(); j++) {
			// Go through speakers
			for (size_t k = 0; k < eqs.at(j).size(); k++) {
				// Frequency response for this speaker
				auto response = Base::system().getSpeaker(mic_ips.at(j)).getFrequencyResponseFrom(speaker_ips.at(k)).at(i);
				double linear_energy = pow(10, response / 10);
				
				total_linear_energy.at(k).at(i) += linear_energy;
			}
		}
	}
	
	// Set weights
	for (int i = 0; i < num_bands; i++) {
		// Go through microphones
		for (size_t j = 0; j < eqs.size(); j++) {
			// Go through speakers
			for (size_t k = 0; k < eqs.at(j).size(); k++) {
				// Frequency response for this speaker
				auto response = Base::system().getSpeaker(mic_ips.at(j)).getFrequencyResponseFrom(speaker_ips.at(k)).at(i);
				double linear_energy = pow(10, response / 10);
				
				double weight = linear_energy / total_linear_energy.at(k).at(i);
				
				// Get EQ at frequency j
				double wanted_eq = eqs.at(j).at(k).at(i);
				
				cout << "Speaker " << speaker_ips.at(k) << " has weight " << weight << " and adding EQ " << weight * wanted_eq << " to band " << i << endl;
				
				// Sk[i] += weight * Mj[i]
				final_eqs.at(k).at(i) += weight * wanted_eq;
			}
		}
	}
	
	return final_eqs;
}

static void runFrequencyResponseScripts(const vector<string>& speakers, const vector<string>& mics, const string& filename, int play) {
	vector<string> scripts;
	
	auto idle = Base::config().get<int>("idle_time");
	//auto play = Base::config().get<int>("play_time");
	
	for (size_t i = 0; i < speakers.size(); i++) {
		string script = "sleep " + to_string(idle + i * (play + idle)) + "; wait; ";
		script +=		"aplay -D localhw_0 -r 48000 -f S16_LE /tmp/" + filename + "; wait\n";
		
		scripts.push_back(script);
	}
	
	for (auto& ip : mics) {
		string script =	"arecord -D audiosource -r 48000 -f S16_LE -c 1 -d " + to_string(idle + speakers.size() * (play + idle));
		script +=		" /tmp/cap" + ip + ".wav; wait\n";
		
		scripts.push_back(script);
	}
	
	vector<string> all_ips(speakers);
	all_ips.insert(all_ips.end(), mics.begin(), mics.end());
	
	Base::system().runScript(all_ips, scripts);
}

/*
	Test different levels of dB until we find a common factor
*/
static FactorData findCorrectionFactor(const vector<string>& speaker_ips, const vector<string>& mic_ips, int iterations) {
	FactorData change_factors = vector<vector<vector<vector<double>>>>(mic_ips.size(), vector<vector<vector<double>>>(speaker_ips.size()));
	FactorData last_dbs = vector<vector<vector<vector<double>>>>(mic_ips.size(), vector<vector<vector<double>>>(speaker_ips.size()));
	
	double step = 3; // How much each iteration should alterate the gain
	
	auto num_bands = Base::system().getSpeakerProfile().getNumEQBands();
	
	for (int i = 0; i < iterations; i++) {
		// Set EQ based on which iteration we're in
		vector<double> eq(num_bands, (i == 0 ? 0 : step));
		
		for (size_t j = 0; j < speaker_ips.size(); j++)
			Base::system().getSpeaker(speaker_ips.at(j)).setNextEQ(eq, 0);
			
		// Propagate it to clients
		setSpeakersEQ(speaker_ips, TYPE_NEXT_EQ);
		
		// Run frequency responses
		runFrequencyResponseScripts(speaker_ips, mic_ips, Base::config().get<string>("sound_image_file_short"), Base::config().get<int>("play_time_freq"));
		
		// Collect data
		Base::system().getRecordings(mic_ips);
		
		auto idle = Base::config().get<int>("idle_time");
		auto play = Base::config().get<int>("play_time");
		
		// Go through frequency analysis
		for (size_t k = 0; k < mic_ips.size(); k++) {
			string filename = "results/cap" + mic_ips.at(k) + ".wav";
			
			vector<short> data;
			WavReader::read(filename, data);
			
			for (size_t j = 0; j < speaker_ips.size(); j++) {
				double sound_start_sec = static_cast<double>(idle + j * (play + idle)) + 1;
				double sound_stop_sec = sound_start_sec + play / 2.0;
				size_t sound_start = lround(sound_start_sec * 48000.0);
				size_t sound_stop = lround(sound_stop_sec * 48000.0);
				
				// Calculate FFT for 9 band as well
				auto db_linears = getFFT9(data, sound_start, sound_stop);
				vector<double> dbs;
				
				for (auto& db_linear : db_linears) {
					double db = 10 * log10(db_linear);
					
					dbs.push_back(db);
				}
				
				cout << "Microphone (" << mic_ips.at(k) << ") gets from " << speaker_ips.at(j) << ":\n";
				for (size_t z = 0; z < dbs.size(); z++)
					cout << "Frequency " << g_frequencies.at(z) << "\t " << dbs.at(z) << " dB\n";
				
				if (!last_dbs.at(k).at(j).empty()) {
					// Calculate new factors based on last results in last_dbs
					auto& last_dbs_result = last_dbs.at(k).at(j).back();
					vector<double> new_factor;
					
					for (size_t l = 0; l < dbs.size(); l++) {
						double change = dbs.at(l) - last_dbs_result.at(l); // Should be > 0 for sane reasons
						double factor = change / step;
						
						new_factor.push_back(factor);
					}
					
					change_factors.at(k).at(j).push_back(new_factor);
				}
				
				// Insert into dB changes
				last_dbs.at(k).at(j).push_back(dbs);
			}
		}
	}
	
	return change_factors;
}

double calculateSD(const vector<double>& data) {
	double sum = 0;
	double mean = 0;
	double std = 0;
	
	for (auto& value : data)
		sum += value;
		
	mean = sum / data.size();
	
	for (auto& value : data)
		std += pow(value - mean, 2);
		
	return sqrt(std / data.size());	
}

static void printFactorData(const vector<FactorData>& data, const vector<string>& mic_ips, const vector<string>& speaker_ips) {
	auto num_bands = Base::system().getSpeakerProfile().getNumEQBands();
	
	vector<double> final_factors = vector<double>(num_bands, 0);
	vector<vector<double>> std_dev = vector<vector<double>>(num_bands, vector<double>());
	
	// Print all data
	for (auto& change_factors : data) {
		for (size_t i = 0; i < change_factors.size(); i++) {
			cout << "Microphone " << mic_ips.at(i) << ":\n";
			auto& speakers = change_factors.at(i);
			
			for (size_t j = 0; j < speakers.size(); j++) {
				cout << "Speaker " << speaker_ips.at(j) << ":\n";
				auto& factors = speakers.at(j);
				
				for (size_t k = 0; k < factors.size(); k++) {
					cout << "Factor " << k * 3 << " dB -> " << (k + 1) * 3 << " dB:\t";
					auto& eq_step = factors.at(k);
					
					for (size_t l = 0; l < eq_step.size(); l++) {
						final_factors.at(l) += eq_step.at(l);
						std_dev.at(l).push_back(eq_step.at(l));
						printf("%02lf\t", eq_step.at(l));
					}
						
					cout << endl;
				}
			}
			
			cout << endl;
		}
	}
	
	vector<double> deviation;
	
	for (int i = 0; i < num_bands; i++) {
		auto std_avk = calculateSD(std_dev.at(i));
		
		deviation.push_back(std_avk);
	}
	
	double general_factor = 0;
	
	// Normalize to sum
	cout << "Final factors:\n";
	for (size_t i = 0; i < final_factors.size(); i++) {
		auto& factor = final_factors.at(i);
		factor /= (data.size() * 3 * speaker_ips.size());
		
		cout << "Frequency " << g_frequencies.at(i) << "\t " << factor << "\t dev " << deviation.at(i) << endl;
		general_factor += factor;
	}
	
	general_factor /= final_factors.size();
	
	cout << "GENERAL FACTOR: " << general_factor << endl;
}

// From Recording.cpp
extern short getRMS(const vector<short>&, size_t, size_t);

static double getSoundLevel(const vector<string>& mic_ips) {
	double total_db = 0;
	
	for (auto& mic_ip : mic_ips) {
		string filename = "results/cap" + mic_ip + ".wav";
		
		vector<short> data;
		WavReader::read(filename, data);
		
		auto idle = Base::config().get<int>("idle_time");
		auto play = Base::config().get<int>("play_time");
		
		double sound_start_sec = static_cast<double>(idle) * 2;
		double sound_stop_sec = sound_start_sec + play - idle * 2;
		size_t sound_start = lround(sound_start_sec * 48000.0);
		size_t sound_stop = lround(sound_stop_sec * 48000.0);
		
		auto sound_level_linear = getRMS(data, sound_start, sound_stop);
		auto sound_level_db = 20.0 * log10(sound_level_linear / (double)SHRT_MAX);
		
		total_db += sound_level_db;
	}
	
	return total_db / mic_ips.size();
}

static void runTestSoundImage(const vector<string>& speaker_ips, const vector<string>& mic_ips, const string& filename) {
	vector<string> scripts;
	
	auto idle = Base::config().get<int>("idle_time");
	auto play = Base::config().get<int>("play_time");
	
	string play_command =	"sleep " + to_string(idle) + "; wait; ";
	play_command +=			"aplay -D localhw_0 -r 48000 -f S16_LE /tmp/" + filename + "; wait\n";
	
	for (size_t i = 0; i < speaker_ips.size(); i++)
		scripts.push_back(play_command);
	
	for (auto& ip : mic_ips) {
		string record_command =	"arecord -D audiosource -r 48000 -f S16_LE -c 1 -d " + to_string(idle + play + idle) + " /tmp/cap" + ip + ".wav; wait\n";
		
		scripts.push_back(record_command);
	}
	
	vector<string> all_ips(speaker_ips);
	all_ips.insert(all_ips.end(), mic_ips.begin(), mic_ips.end());
	
	Base::system().runScript(all_ips, scripts);
}

static void showCalibrationScore(const vector<string>& mic_ips) {
	for (auto& mic_ip : mic_ips) {
		string filename = "results/cap" + mic_ip + ".wav";
		
		vector<short> data;
		WavReader::read(filename, data);
		
		auto idle = Base::config().get<int>("idle_time");
		auto play = Base::config().get<int>("play_time");
		
		double sound_start_sec = static_cast<double>(idle) * 2;
		double sound_stop_sec = sound_start_sec + play - idle * 2;
		size_t sound_start = lround(sound_start_sec * 48000.0);
		size_t sound_stop = lround(sound_stop_sec * 48000.0);
		
		vector<short> sound(data.begin() + sound_start, data.begin() + sound_stop);
		
		auto fft_output = nac::toDecibel(nac::doFFT(sound));
		auto applied = fft_output;
		
		//if (Base::config().get<bool>("enable_hardware_profile"))
		//	applied = nac::applyProfiles(fft_output, Base::system().getSpeakerProfile().invert(), Base::system().getMicrophoneProfile().invert());
		
		nac::fitBands(applied, Base::system().getSpeakerProfile().getSpeakerEQ(), true);
	}
}

static FFTOutput getWhiteResponse(const vector<short>& data, size_t sound_start, size_t sound_stop) {
	vector<short> sound(data.begin() + sound_start, data.begin() + sound_stop);
	
	return nac::doFFT(sound);
}

// From NetworkCommunication.cpp
extern string getTimestamp();

static string writeWhiteNoiseFiles(const string& where, string timestamp = "") {
	if (timestamp.empty()) {
		timestamp = getTimestamp();
		// Remove whitespace
		replace(timestamp.begin(), timestamp.end(), ' ', '_');
		// Remove ':'
		replace(timestamp.begin(), timestamp.end(), ':', '_');
		timestamp.pop_back();
		timestamp += '/';
	}
	
	if (!system(NULL)) {
		cout << "WARNING: No shell available\n";
		
		return timestamp;
	}
	
	// Create folders for this data
	string folder =	"../save/white_noises/";
	folder +=		where + "/" + timestamp;
	
	string mkdir = "mkdir " + folder;
	string move = "cp results/cap* " + folder;
	
	system(mkdir.c_str());
	system(move.c_str());
	
	cout << "mkdir command: " << mkdir << endl;
	cout << "move command: " << move << endl;
	
	return timestamp;
}

static void writeEQSettings(const string& where, const string& timestamp, const vector<string>& speaker_ips) {
	if (!system(NULL))
		return;
		
	string folder = "../save/white_noises/" + where + "/" + timestamp;
	string file = folder + "eqs";
	
	ofstream eqs(file);
	
	if (!eqs.is_open()) {
		cout << "Warning: could not open " << file << " for writing\n";
		
		return;
	}
	
	auto speakers = Base::system().getSpeakers(speaker_ips);
	
	// Write number of speakers
	eqs << speakers.size() << endl;
	
	for (auto* speaker : speakers) {
		for (auto& setting : speaker->getBestEQ())
			eqs << setting << endl;
	}
	
	eqs.close();
}

static string moveFileMATLAB(const string& where, const string& timestamp, const vector<string>& mic_ips) {
	string folder = "../save/white_noises/" + where + "/" + timestamp;
	
	vector<string> copy_after;
	
	if (mic_ips.size() > 1) {
		for (auto& mic_ip : mic_ips)
			copy_after.push_back("cp " + folder + "cap" + mic_ip + ".wav ../matlab/" + where + mic_ip + ".wav");
	} else {
		copy_after.push_back("cp " + folder + "*.wav ../matlab/" + where + ".wav");
	}
	
	for (auto& command : copy_after) {
		cout << "Copy command: " << command << endl;
		system(command.c_str());
	}
		
	return folder;
}

static void moveToMATLAB(const string& timestamp, const vector<string>& mic_ips) {
	if (!system(NULL))
		return;

	moveFileMATLAB("before", timestamp, mic_ips);
	string folder_after = moveFileMATLAB("after", timestamp, mic_ips);
	string copy_eqs = "cp " + folder_after + "eqs ../matlab/";
	
	cout << "Copy EQs: " << copy_eqs << endl;
	system(copy_eqs.c_str());
}

static void addCustomerEQ(const vector<string>& speaker_ips) {
	auto speakers = Base::system().getSpeakers(speaker_ips);
	
	for (auto* speaker : speakers)
		speaker->addCustomerEQ(g_customer_profile);
}

void Handle::checkSoundImage(const vector<string>& speaker_ips, const vector<string>& mic_ips, bool factor_calibration, int type) {
	// Set g_dsp_factor
	bool run_white_noise = false;
	
	if (type == WHITE_NOISE)
		run_white_noise = true;
		
	if (run_white_noise)
		cout << "Running white noise sound image\n";
	else
		cout << "Running 9-freq tone sound image\n";
	
	vector<string> all_ips(speaker_ips);
	all_ips.insert(all_ips.end(), mic_ips.begin(), mic_ips.end());
	
	// Disable audio system
	disableAudioSystem(all_ips);
	
	// Send test files to speakers
	Base::system().sendFile(speaker_ips, "data/" + Base::config().get<string>("white_noise"), "/tmp/", true);
	
	if (!run_white_noise)
		Base::system().sendFile(speaker_ips, "data/" + Base::config().get<string>("sound_image_file_short"), "/tmp/", true);
	
	// Find correction factor
	if (factor_calibration) {
		vector<FactorData> factor_data;
		
		for (int i = 0; i < 25; i++) {
			// Set test settings again
			setTestSpeakerSettings(all_ips);
			
			// Set test DSP gain again
			setSpeakersEQ(speaker_ips, TYPE_FLAT_EQ);
			
			factor_data.push_back(findCorrectionFactor(speaker_ips, mic_ips, 4));
			
			cout << "Factor data for iteration " << (i + 1) << endl;
			printFactorData({ factor_data.back() }, mic_ips, speaker_ips);
		}
		
		cout << "Factor data for all iterations\n";
		printFactorData(factor_data, mic_ips, speaker_ips);
	}
	
	// Set test settings
	setTestSpeakerSettings(all_ips);
	
	// Set test DSP gain
	setSpeakersEQ(speaker_ips, TYPE_FLAT_EQ);
	
	// Set test white noise settings
	setSpeakersEQ(speaker_ips, TYPE_WHITE_EQ);
	
	// Play white noise from all speakers to check sound image & collect the recordings
	runTestSoundImage(speaker_ips, mic_ips, Base::config().get<string>("white_noise"));
	Base::system().getRecordings(mic_ips);
	
	// Get sound level from white noise
	auto flat_level_db = getSoundLevel(mic_ips);
	
	cout << "White noise sound level: " << flat_level_db << endl;
	cout << "Setting target mean to " << flat_level_db << endl;
	
	//g_target_mean = flat_level_db;
	
	// See calibration score before calibrating
	showCalibrationScore(mic_ips);
	
	auto timestamp = writeWhiteNoiseFiles("before");

	// Set test DSP gain
	setSpeakersEQ(speaker_ips, TYPE_FLAT_EQ);
	
	// Run frequency responses
	if (run_white_noise) {
		if (speaker_ips.size() > 1) {
			runFrequencyResponseScripts(speaker_ips, mic_ips, Base::config().get<string>("white_noise"), Base::config().get<int>("play_time"));
			Base::system().getRecordings(mic_ips);
		}
	} else {
		runFrequencyResponseScripts(speaker_ips, mic_ips, Base::config().get<string>("sound_image_file_short"), Base::config().get<int>("play_time_freq"));
		Base::system().getRecordings(mic_ips);
	}
	
	auto idle = Base::config().get<int>("idle_time");
	auto play = Base::config().get<int>("play_time");
	
	if (!run_white_noise)
		play = Base::config().get<int>("play_time_freq");
	
	// Wanted EQs by microphones
	MicWantedEQ wanted_eqs;
	
	// Go through frequency analysis
	for (auto& mic_ip : mic_ips) {
		string filename = "results/cap" + mic_ip + ".wav";
		
		vector<short> data;
		WavReader::read(filename, data);
		
		vector<vector<double>> new_eqs;
		
		for (size_t i = 0; i < speaker_ips.size(); i++) {
			double sound_start_sec = static_cast<double>(idle) * 2 + (i * (play + idle));
			double sound_stop_sec = sound_start_sec + play - idle * 2;
			size_t sound_start = lround(sound_start_sec * 48000.0);
			size_t sound_stop = lround(sound_stop_sec * 48000.0);
			
			vector<double> dbs;
			vector<double> final_eq;
			
			if (run_white_noise) {
				auto eq_settings = Base::system().getSpeakerProfile().getSpeakerEQ();
				
				auto response = getWhiteResponse(data, sound_start, sound_stop);
				response = nac::toDecibel(response);
				
				auto fit_response = nac::fitBands(response, eq_settings, true);
				
				auto difference = nac::getDifference(response, g_target_db_level, false);
				auto applied = difference;
				
				if (Base::config().get<bool>("enable_hardware_profile")) {
					auto speaker_profile = Base::system().getSpeakerProfile();
					auto mic_profile = Base::system().getMicrophoneProfile();
					
					if (Base::config().get<bool>("hardware_profile_boost_steeps")) {
						speaker_profile = speaker_profile.invert();
						mic_profile = mic_profile.invert();
					}
					
					applied = nac::applyProfiles(difference, speaker_profile, mic_profile);
				}
				
				final_eq = nac::fitBands(applied, eq_settings, true);
				dbs = fit_response;
			} else {
				// Calculate FFT for 9 band as well
				auto db_linears = getFFT9(data, sound_start, sound_stop);
				
				for (auto& db_linear : db_linears) {
					double db = 10 * log10(db_linear);
					
					dbs.push_back(db);
				}
				
				// How much does this microphone get per frequency?
				cout << "Microphone (" << mic_ip << ") gets from " << speaker_ips.at(i) << ":\n";
				for (size_t j = 0; j < dbs.size(); j++)
					cout << "Frequency " << g_frequencies.at(j) << "\t " << dbs.at(j) << " dB\n";
				
				// Calculate correction EQ
				auto eq = getSoundImageCorrection(dbs, run_white_noise);
				
				cout << "Which gives the correction EQ of:\n";
				for (size_t j = 0; j < eq.size(); j++)
					cout << "Frequency " << g_frequencies.at(j) << "\t " << eq.at(j) << " dB\n";
					
				final_eq = eq;
			}
			
			new_eqs.push_back(final_eq);
			
			// DBs is vector of the current speaker with all its' frequency dBs
			Base::system().getSpeaker(mic_ip).setFrequencyResponseFrom(speaker_ips.at(i), dbs);
		}
		
		// Add this to further calculations when we have all the information
		wanted_eqs.push_back(new_eqs);
	}
	
	// Weight data against profile and microphones
	auto final_eqs = weightEQs(speaker_ips, mic_ips, wanted_eqs);
	
	// Set new EQs
	for (size_t j = 0; j < speaker_ips.size(); j++) {
		// Add new EQ
		Base::system().getSpeaker(speaker_ips.at(j)).setNextEQ(final_eqs.at(j), 0);
		
		auto num_bands = Base::system().getSpeakerProfile().getNumEQBands();
		
		// Say that this EQ is epic
		Base::system().getSpeaker(speaker_ips.at(j)).setNextEQ(vector<double>(num_bands, 0), INT_MAX);
	}
	
	// Set test white noise settings
	setSpeakersEQ(speaker_ips, TYPE_WHITE_EQ);
	
	// Play white noise from all speakers to check sound image & collect the recordings
	runTestSoundImage(speaker_ips, mic_ips, Base::config().get<string>("white_noise"));
	Base::system().getRecordings(mic_ips);
	
	// See calibration score before calibrating
	showCalibrationScore(mic_ips);
	
	writeWhiteNoiseFiles("after", timestamp);
	writeEQSettings("after", timestamp, speaker_ips);
	moveToMATLAB(timestamp, mic_ips);
	
	if (Base::config().get<bool>("enable_customer_profile"))
		addCustomerEQ(speaker_ips);
	
	// Reset mics & set best EQ
	resetEverything(mic_ips);
	setSpeakersEQ(speaker_ips, TYPE_BEST_EQ);
	enableAudioSystem(all_ips);
}

void Handle::resetIPs(const vector<string>& ips) {
	// Reset speakers & enable audio system
	resetEverything(ips);
	enableAudioSystem(ips);
}

void Handle::setBestEQ(const vector<string>& speakers, const vector<string>& mics) {
	// Reset mics & enable audio system
	resetIPs(mics);
	
	// Set best EQ & enable audio system for speakers
	setSpeakersEQ(speakers, TYPE_BEST_EQ);
	enableAudioSystem(speakers);
}

void Handle::setEQStatus(const vector<string>& ips, bool status) {
	Base::system().runScript(ips, vector<string>(ips.size(), "dspd -s -" + string((status ? "u" : "b")) + " preset; wait\n"));
}

void Handle::setSoundEffects(const std::vector<std::string> &ips, bool status) {
	string enable = "dspd -s -u compressor; wait; dspd -s -u loudness; wait; dspd -s -u hpf; wait\n";
	string disable = "dspd -s -b compressor; wait; dspd -s -b loudness; wait; dspd -s -b hpf; wait\n";
	
	if (status)
		Base::system().runScript(ips, vector<string>(ips.size(), enable));
	else
		Base::system().runScript(ips, vector<string>(ips.size(), disable));
}

template<class T>
static T mean(const vector<T>& container) {
	double sum = 0;
	
	for(const auto& element : container)
		sum += element;
		
	return sum / (double)container.size();
}

void Handle::testing() {
	#if 0
	try {
		string filename = "before.wav";
		vector<short> data;
		WavReader::read(filename, data);
		data = vector<short>(data.begin() + 48000 * 2, data.begin() + 48000 * 30);
		
		auto& speaker_profile = Base::system().getSpeakerProfile();
		
		auto difference = nac::getDecibelDifference(nac::doFFT(data), g_target_mean);
		auto applied = nac::applyProfiles(difference, Base::system().getSpeakerProfile(), Base::system().getMicrophoneProfile());
		auto final_eq = nac::getEQ(applied, speaker_profile.getSpeakerEQ());
	} catch (...) {
		// Testing method failed, we don't care
		return;
	}
	#endif
}