#include "Handle.h"
#include "WavReader.h"
#include "Localization3D.h"
#include "Goertzel.h"
#include "Base.h"
#include "Packet.h"

#include <iostream>
#include <cmath>
#include <algorithm>
#include <climits>

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
	TYPE_FLAT_EQ
};

// We're not multithreading anyway
Connection* g_current_connection = nullptr;

static vector<string> g_frequencies = {	"63",
										"125",
										"250",
										"500",
										"1000",
										"2000",
										"4000",
										"8000",
										"16000" };

static double g_target_mean = -45;

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
	Base::system().runScript(ips, vector<string>(ips.size(), "systemctl stop audio*; wait\n"));
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
	string command =	"dspd -s -w; wait; dspd -s -m; wait; dspd -s -u limiter; wait; dspd -s -u static; wait; dspd -s -u preset; wait; dspd -s -p flat; wait; ";
	command +=			"amixer -c1 sset 'Headphone' 57 on; wait; amixer -c1 sset 'Capture' 63; wait; amixer -c1 sset 'PGA Boost' 2; wait; ";
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
		script +=		to_string(idle_time + ips.size() * (idle_time + play_time));
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
	
	for (auto* speaker : speakers) {
		auto ip = speaker->getIP();
		const auto& coordinates = speaker->getPlacement().getCoordinates();
		const auto& distances = speaker->getPlacement().getDistances();
		
		output.push_back(make_tuple(ip, coordinates, distances));
	}
	
	return output;
}

PlacementOutput Handle::runLocalization(const vector<string>& ips, bool force_update) {
	if (ips.empty())
		return PlacementOutput();

	// Does the server already have relevant positions?
	vector<int> placement_ids;
	
	for (auto& ip : ips)
		placement_ids.push_back(Base::system().getSpeaker(ip).getPlacementID());
	
	if (adjacent_find(placement_ids.begin(), placement_ids.end(), not_equal_to<int>()) == placement_ids.end() && placement_ids.front() >= 0 && !force_update) {
		cout << "Server already have relevant position info, returning that\n";
		
		return assemblePlacementOutput(Base::system().getSpeakers(ips));
	}
	
	if (!Base::config().get<bool>("no_scripts")) {
		// Create scripts
		int play_time = Base::config().get<int>("play_time");
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
	
	return assemblePlacementOutput(Base::system().getSpeakers(ips));
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

static double getSoundImageScore(const vector<double>& dbs) {
	double score = 0;
	
	for (auto& db : dbs)
		score += (g_target_mean - db) * (g_target_mean - db);
	
	return 1 / sqrt(score);
}

static vector<double> getSoundImageCorrection(vector<double> dbs) {
	vector<double> eq;
	
	for (auto& db : dbs)
		eq.push_back(g_target_mean - db);
	
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
	double highest_dsp_gain = INT_MIN;
	
	for (auto* speaker : speakers) {
		if (speaker->getBestVolume() > highest_dsp_gain)
			highest_dsp_gain = speaker->getBestVolume();
	}
	
	for (auto* speaker : speakers) {
		vector<double> eq;
		
		switch (type) {
			case TYPE_BEST_EQ: {
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
			case TYPE_BEST_EQ: dsp_gain = SPEAKER_MAX_VOLUME - highest_dsp_gain;
				break;
				
			case TYPE_NEXT_EQ: case TYPE_FLAT_EQ: dsp_gain = -18;
				break;
		}
		
		setSpeakerVolume(speaker->getIP(), speaker->getVolume(), dsp_gain);
	}
	
	Base::system().runScript(speaker_ips, commands);
}

static vector<vector<double>> weightEQs(const MicWantedEQ& eqs) {
	// Initialize to 0 EQ
	vector<vector<double>> final_eqs(eqs.front().size(), vector<double>(DSP_MAX_BANDS, 0));
	
	// Go through all frequency bands
	for (int j = 0; j < DSP_MAX_BANDS; j++) {
		// Go through microphones
		for (size_t i = 0; i < eqs.size(); i++) {
			// Go through speakers
			for (size_t k = 0; k < eqs.at(i).size(); k++) {
				// Get EQ at frequency j
				double wanted_eq = eqs.at(i).at(k).at(j);
				
				wanted_eq /= 2.0;
				
				/*
				if (eqs.front().size() == 1) {
					wanted_eq /= 2.0;
				} else {
					if (wanted_eq > 1)
						wanted_eq = 1;
					if (wanted_eq < -1)
						wanted_eq = -1;
				}
				*/

				final_eqs.at(k).at(j) += wanted_eq;
			}
		}
	}
	
	// Normalize to amount of mics
	for (auto& speaker : final_eqs)
		for (auto& setting : speaker)
			setting /= eqs.size();
	
	return final_eqs;
}

static double getFinalScore(const vector<double>& scores) {
	double total_score = 0;
	
	cout << "Scores: ";
	
	for (auto& score : scores) {
		cout << score << " ";
		
		double db_difference = 1 / score;
		
		total_score += db_difference * db_difference;
	}
	
	double final_score = 1 / (sqrt(total_score) / (double)scores.size());
	
	cout << "\nFinal score: " << final_score << endl;
		
	return final_score;
}

static size_t getLoudestSpeaker(const string& mic, const vector<string>& speakers, int frequency_index, double change) {
	vector<tuple<size_t, double, double>> loudest;
	
	for (size_t i = 0; i < speakers.size(); i++) {
		auto* speaker = &Base::system().getSpeaker(speakers.at(i));
		
		auto eq_delta = speaker->getNextEQ().at(frequency_index);
		auto volume_delta = speaker->getNextVolume() - SPEAKER_MAX_VOLUME;
		auto base_level = Base::system().getSpeaker(mic).getFrequencyResponseFrom(speaker->getIP()).at(frequency_index);
		
		auto final_volume = base_level + volume_delta + eq_delta;
		
		if (change < 0)
			eq_delta *= -1;
			
		loudest.push_back(make_tuple(i, final_volume, eq_delta));
	}
	
	// Sort by loudness
	sort(loudest.begin(), loudest.end(), [] (auto& first, auto& second) {
		return get<1>(first) > get<1>(second);
	});
	
	// Pick the first with non-maxed EQ at chosen frequency
	for (auto& information : loudest) {
		// If it fits into the range, let's do it
		if (get<2>(information) < DSP_MAX_EQ)
			return get<0>(information);
	}
	
	// All speakers are maxed, pick the one with least maxed EQ
	sort(loudest.begin(), loudest.end(), [] (auto& first, auto& second) {
		return get<2>(first) < get<2>(second);
	});
	
	return get<0>(loudest.front());
}

static vector<double> getSpeakerEQChange(const string& mic, const vector<string>& speakers, int frequency_index, double change) {
	// Add EQ to the most fitting speaker
	// Do it this simple way since abs(change) <= 1
	vector<double> eqs(speakers.size());
	eqs.at(getLoudestSpeaker(mic, speakers, frequency_index, change)) += change;
	
	return eqs;
}

static void runFrequencyResponseScripts(const vector<string>& speakers, const vector<string>& mics, const string& filename) {
	vector<string> scripts;
	
	auto idle = Base::config().get<int>("idle_time");
	auto play = Base::config().get<int>("play_time");
	
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

static void runSoundImageRecordings(const vector<string>& mics) {
	vector<string> scripts;
	
	auto play = Base::config().get<int>("play_time");
	
	for (auto& ip : mics)
		scripts.push_back("arecord -D audiosource -r 48000 -f S16_LE -c 1 -d " + to_string(play) + " /tmp/cap" + ip + ".wav; wait\n");
		
	Base::system().runScript(mics, scripts);
}

static mutex g_long_tone_mutex;
static condition_variable g_long_tone_cv;
static bool g_lone_tone_ready = false;

static void threadLongTone(const vector<string>& speaker_ips) {
	{
		lock_guard<mutex> lock(g_long_tone_mutex);
        g_lone_tone_ready = true;
	}
	
	// Notify sound image thread that the script has started
	g_long_tone_cv.notify_one();

	Base::system().runScript(speaker_ips, vector<string>(speaker_ips.size(), "aplay -D localhw_0 -r 48000 -f S16_LE /tmp/" + Base::config().get<string>("sound_image_file_long") + "; wait\n"), true);
}

void Handle::checkSoundImage(const vector<string>& speaker_ips, const vector<string>& mic_ips, int iterations) {
	vector<string> all_ips(speaker_ips);
	all_ips.insert(all_ips.end(), mic_ips.begin(), mic_ips.end());
	
	// Disable audio system
	disableAudioSystem(all_ips);
	
	// Send test files to speakers
	Base::system().sendFile(speaker_ips, "data/" + Base::config().get<string>("sound_image_file_short"), "/tmp/", false);
	Base::system().sendFile(speaker_ips, "data/" + Base::config().get<string>("sound_image_file_long"), "/tmp/", false);
	
	// Set test settings
	setTestSpeakerSettings(all_ips);
	
	// Set test DSP gain
	setSpeakersEQ(speaker_ips, TYPE_FLAT_EQ);
	
	// Run frequency responses
	runFrequencyResponseScripts(speaker_ips, mic_ips, Base::config().get<string>("sound_image_file_short"));
	
	// Collect data
	Base::system().getRecordings(mic_ips);
	
	auto idle = Base::config().get<int>("idle_time");
	auto play = Base::config().get<int>("play_time");
	
	// Go through frequency analysis
	for (auto& mic_ip : mic_ips) {
		string filename = "results/cap" + mic_ip + ".wav";
		
		vector<short> data;
		WavReader::read(filename, data);
		
		for (size_t i = 0; i < speaker_ips.size(); i++) {
			double sound_sec = static_cast<double>(idle + i * (play + idle)) + 0.33;
			size_t sound_start = lround(sound_sec * 48000.0);
			
			// Calculate FFT for 9 band as well
			auto db_linears = getFFT9(data, sound_start, sound_start + (48000 / 3));
			vector<double> dbs;
			
			for (auto& db_linear : db_linears) {
				double db = 20 * log10(db_linear / (double)SHRT_MAX);
				
				dbs.push_back(db);
			}
			
			// DBs is vector of the current speaker with all its' frequency dBs
			Base::system().getSpeaker(mic_ip).setFrequencyResponseFrom(speaker_ips.at(i), dbs);
		}
	}
	
	thread long_tone_thread(threadLongTone, ref(speaker_ips));
	
	{
		unique_lock<mutex> lock(g_long_tone_mutex);
		g_long_tone_cv.wait(lock, [] { return g_lone_tone_ready; });
		
		this_thread::sleep_for(chrono::seconds(2)); // TODO: Remove this later
	}
	
	// Start iterating
	for (int i = 0; i < iterations; i++) {
		// Record some sound
		runSoundImageRecordings(mic_ips);
		
		// Get recordings
		Base::system().getRecordings(mic_ips);
		
		MicWantedEQ wanted_eqs;
		vector<double> scores;
		
		// Analyze data
		for (auto& ip : mic_ips) {
			string filename = "results/cap" + ip + ".wav";
			
			vector<short> data;
			WavReader::read(filename, data);
			
			double sound_sec = 0.33;
			size_t sound_start = lround(sound_sec * 48000.0);
			
			// Calculate FFT for 9 band as well
			auto db_linears = getFFT9(data, sound_start, sound_start + (48000 / 3));
			vector<double> dbs;
			
			for (auto& db_linear : db_linears) {
				double db = 20 * log10(db_linear / (double)SHRT_MAX);
				
				dbs.push_back(db);
			}
			
			// How much does this microphone get per frequency?
			cout << "Microphone (" << ip << ") gets:\n";
			for (size_t j = 0; j < dbs.size(); j++)
				cout << "Frequency " << g_frequencies.at(j) << "\t " << dbs.at(j) << " dB\n";
			
			// Calculate score
			auto score = getSoundImageScore(dbs);
			
			// Calculate correction EQ
			auto eq = getSoundImageCorrection(dbs);
			
			cout << "Which gives the correction EQ of:\n";
			for (size_t j = 0; j < eq.size(); j++)
				cout << "Frequency " << g_frequencies.at(j) << "\t " << eq.at(j) << " dB\n";
				
			cout << "And a score of " << score << endl;
				
			// New EQs
			vector<vector<double>> new_eqs(speaker_ips.size(), vector<double>(DSP_MAX_BANDS, 0));
				
			// Go through each frequency band
			for (int j = 0; j < DSP_MAX_BANDS; j++) {
				// Which speaker was already loudest here? Adjust that one since
				// boosting less sounding speakers won't affect the sound image
				// as much since combining them with a louder source will drown the
				// adjusted sound
				// Source: http://www.csgnetwork.com/decibelamplificationcalc.html
				
				// More information about incoherent/coherent sound sources here
				// http://www.sengpielaudio.com/calculator-spl.htm
				
				auto change = eq.at(j);
				auto speakers_eq_change = getSpeakerEQChange(ip, speaker_ips, j, change);
				
				for (size_t k = 0; k < speaker_ips.size(); k++)
					new_eqs.at(k).at(j) = speakers_eq_change.at(k);
			}
			
			// Add this to further calculations when we have all the information
			wanted_eqs.push_back(new_eqs);
			scores.push_back(score);
		}
		
		// Weight mics' different EQs against eachother
		auto final_eqs = weightEQs(wanted_eqs);
		
		// Calculate final score
		auto final_score = getFinalScore(scores);
		
		// Set new EQs
		for (size_t j = 0; j < speaker_ips.size(); j++)
			Base::system().getSpeaker(speaker_ips.at(j)).setNextEQ(final_eqs.at(j), final_score);
			
		// Propagate it to clients
		setSpeakersEQ(speaker_ips, TYPE_NEXT_EQ);
			
		// Update client here
		Packet packet;
		packet.addHeader(0x00);
		
		for (auto& score : scores)
			packet.addFloat(score);
			
		packet.addFloat(final_score);
			
		packet.finalize();
		Base::network().addOutgoingPacket(g_current_connection->getSocket(), packet);
	}
	
	// Kill long tone
	Base::system().runScript(speaker_ips, vector<string>(speaker_ips.size(), "killall -9 aplay; wait\n"));
	
	// Set best EQ automatically
	setBestEQ(speaker_ips, mic_ips);
	
	// Wait for thread to terminate
	long_tone_thread.join();
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