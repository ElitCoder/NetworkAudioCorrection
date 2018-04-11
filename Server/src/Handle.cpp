#include "Handle.h"
#include "WavReader.h"
#include "Config.h"
#include "Localization3D.h"
#include "Goertzel.h"
#include "Base.h"

#include <iostream>
#include <cmath>
#include <algorithm>
#include <climits>

using namespace std;

static vector<string> g_frequencies = {	"63",
									"125",
									"250",
									"500",
									"1000",
									"2000",
									"4000",
									"8000",
									"16000" };

static double g_target_mean = -50;

static int SPEAKER_MAX_VOLUME;

static SSHOutput runBasicScriptExternal(const vector<string>& speakers, const vector<string>& mics, const vector<string>& all_ips, const vector<string>& scripts, const string& send_from, const string& send_to) {
	if (!Base::system().sendFile(speakers, send_from, send_to))
		return SSHOutput();
		
	auto output = Base::system().runScript(all_ips, scripts);
	
	if (output.empty())
		return output;
		
	if (!Base::system().getRecordings(mics))
		return SSHOutput();
		
	return output;
}

static SSHOutput runBasicScript(const vector<string>& ips, const vector<string>& scripts, const string& send_from, const string& send_to) {
	return runBasicScriptExternal(ips, ips, ips, scripts, send_from, send_to);
}

static short getRMS(const vector<short>& data, size_t start, size_t end) {
	unsigned long long sum = 0;
	
	for (size_t i = start; i < end; i++)
		sum += (data.at(i) * data.at(i));
		
	sum /= (end - start);
	
	return sqrt(sum);
}

// The following 2 functions are from SO
constexpr char hexmap[] = {'0', '1', '2', '3', '4', '5', '6', '7',
                           '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'};

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

static vector<string> createEnableAudioSystem(const vector<string>& ips) {
	string command = "systemctl start audio_relayd; wait\n";
	
	return vector<string>(ips.size(), command);
}

void Handle::resetEverything(const vector<string>& ips) {
	string command = 	"dspd -s -w; wait; ";
	command +=			"amixer -c1 sset 'Headphone' 57 on; wait; "; 				/* 57 is 0 dB for C1004-e */
	command +=			"amixer -c1 sset 'Capture' 63; wait; ";
	command +=			"dspd -s -p flat; wait; ";
	command +=			"amixer -c1 cset numid=170 0x00,0x80,0x00,0x00; wait; "; 	/* Sets DSP gain to 0 */
	command +=			"amixer -c1 sset 'PGA Boost' 2; wait\n";
	
	Base::system().runScript(ips, vector<string>(ips.size(), command));
	
	auto scripts = createEnableAudioSystem(ips);
	Base::system().runScript(ips, scripts);
}

bool Handle::setSpeakerAudioSettings(const vector<string>& ips, const vector<int>& volumes, const vector<int>& captures, const vector<int>& boosts) {
	vector<string> commands;
	
	// In DSP pre-gain
	// -12 dB = 002026f3
	// 0 dB = 00800000
	
	// Let's say this is max volume
	if (!volumes.empty())
		SPEAKER_MAX_VOLUME = volumes.front();
	
	for (size_t i = 0; i < volumes.size(); i++) {
		string volume = to_string(volumes.at(i));
		string capture = to_string(captures.at(i));
		string boost = to_string(boosts.at(i));
		
		string command = 	"dspd -s -w; wait; ";
		command +=			"amixer -c1 sset 'Headphone' " + volume + " on; wait; ";
		command +=			"amixer -c1 sset 'Capture' " + capture + "; wait; ";
		command +=			"dspd -s -m; wait; dspd -s -u limiter; wait; ";
		command +=			"dspd -s -u static; wait; ";
		command +=			"dspd -s -u preset; wait; dspd -s -p flat; wait; ";
		command +=			"amixer -c1 cset numid=170 0x00,0x80,0x00,0x00; wait; "; /* Sets DSP gain to 0 */
		command +=			"amixer -c1 sset 'PGA Boost' " + boost + "; wait\n";
		
		commands.push_back(command);
	}
	
	auto status = !Base::system().runScript(ips, commands).empty();
	
	if (status) {
		// This should be used for resetting speakers
		for (size_t i = 0; i < ips.size(); i++) {
			auto& speaker = Base::system().getSpeaker(ips.at(i));
			
			speaker.setVolume(volumes.at(i));
			speaker.setMicVolume(captures.at(i));
			speaker.setMicBoost(boosts.at(i));
			speaker.clearAllEQs();
		}	
	}
	
	return status;
}

static vector<string> createRunLocalizationScripts(const vector<string>& ips, int play_time, int idle_time, int extra_recording, const string& file) {
	vector<string> scripts;
	
	for (size_t i = 0; i < ips.size(); i++) {
		string script =	"systemctl stop audio*\n";
		script +=		"arecord -D audiosource -r 48000 -f S16_LE -c 1 -d ";
		script +=		to_string(idle_time * 2 + ips.size() * (1 + play_time) + extra_recording - 1 /* no need for one extra second */);
		script +=		" /tmp/cap";
		script +=		ips.at(i);
		script +=		".wav &\n";
		script +=		"proc1=$!\n";
		script +=		"sleep ";
		script +=		to_string(idle_time + i * (play_time + 1));
		script +=		"\n";
		script +=		"aplay -D localhw_0 -r 48000 -f S16_LE /tmp/";
		script += 		file;
		script +=		"\n";
		script +=		"wait $proc1\n";
		script +=		"systemctl start audio_relayd; wait\n";
		
		scripts.push_back(script);
	}
	
	return scripts;
}

void printSSHOutput(SSHOutput outputs) {
	for (auto& output : outputs)
		for (auto& line : output.second)
			cout << "SSH (" << output.first << "): " << line << endl;
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

PlacementOutput Handle::runLocalization(const vector<string>& ips, bool skip_script, bool force_update) {
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
	
	if (!skip_script) {
		// Create scripts
		int play_time = Config::get<int>("speaker_play_length");
		int idle_time = Config::get<int>("idle_time");
		int extra_recording = Config::get<int>("extra_recording");
		
		auto scripts = createRunLocalizationScripts(ips, play_time, idle_time, extra_recording, Config::get<string>("goertzel"));
		
		if (runBasicScript(ips, scripts, "data/" + Config::get<string>("goertzel"), "/tmp/").empty())
			return PlacementOutput();
	}
	
	auto distances = Goertzel::runGoertzel(ips);
	
	if (distances.empty())
		return PlacementOutput();
	
	auto placement = Localization3D::run(distances, Config::get<bool>("fast"));
	
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

static vector<string> createSoundImageScripts(const vector<string>& speakers, const vector<string>& mics, int play_time, int idle_time, const string& filename) {
	vector<string> scripts;
	
	for (size_t i = 0; i < speakers.size(); i++) {
		string script =	"sleep " + to_string(idle_time) + "\n";
		script +=		"aplay -D localhw_0 -r 48000 -f S16_LE /tmp/" + filename + "\n";
		
		scripts.push_back(script);
	}
	
	for (size_t i = 0; i < mics.size(); i++) {
		string script =	"arecord -D audiosource -r 48000 -f S16_LE -c 1 -d " + to_string(idle_time * 2 + play_time);
		script +=		" /tmp/cap";
		script +=		mics.at(i);
		script +=		".wav\n";
		
		scripts.push_back(script);
	}
	
	return scripts;
}

static vector<string> createSoundImageIndividualScripts(const vector<string>& speakers, const vector<string>& mics, int play_time, int idle_time, const string& filename) {
	vector<string> scripts;
	
	for (size_t i = 0; i < speakers.size(); i++) {
		string script =	"sleep " + to_string(idle_time + i * (play_time + 1)) + "\n";
		script +=		"aplay -D localhw_0 -r 48000 -f S16_LE /tmp/" + filename + "\n";
		
		scripts.push_back(script);
	}
	
	for (size_t i = 0; i < mics.size(); i++) {
		string script =	"arecord -D audiosource -r 48000 -f S16_LE -c 1 -d " + to_string(idle_time + speakers.size() * (play_time + 1));
		script +=		" /tmp/cap";
		script +=		mics.at(i);
		script +=		".wav\n";
		
		scripts.push_back(script);
	}
	
	return scripts;
}

static vector<string> createDisableAudioSystem(const vector<string>& ips) {
	string command = "systemctl stop audio*; wait\n";
	
	return vector<string>(ips.size(), command);
}

// Taken from git/SO
static double goertzel(int numSamples,float TARGET_FREQUENCY,int SAMPLING_RATE, short* data)
{
    int     k,i;
    float   floatnumSamples;
    float   omega,sine,cosine,coeff,q0,q1,q2,magnitude,real,imag;

    float   scalingFactor = numSamples / 2.0;

    floatnumSamples = (float) numSamples;
    k = (int) (0.5 + ((floatnumSamples * TARGET_FREQUENCY) / (float)SAMPLING_RATE));
    omega = (2.0 * M_PI * k) / floatnumSamples;
    sine = sin(omega);
    cosine = cos(omega);
    coeff = 2.0 * cosine;
    q0=0;
    q1=0;
    q2=0;

    for(i=0; i<numSamples; i++)
    {
        q0 = coeff * q1 - q2 + data[i];
        q2 = q1;
        q1 = q0;
    }

    // calculate the real and imaginary results
    // scaling appropriately
    real = (q1 - q2 * cosine) / scalingFactor;
    imag = (q2 * sine) / scalingFactor;

    magnitude = sqrtf(real*real + imag*imag);
    return magnitude;
}

static vector<double> getFFT9(const vector<short>& data, size_t start, size_t end) {
	vector<short> sound(data.begin() + start, data.begin() + end);
	
	// Do FFT here - scratch that, let's do 9 Goertzel calculations instead
	double freq63 = goertzel(sound.size(), 63, 48000, sound.data());
	double freq125 = goertzel(sound.size(), 125, 48000, sound.data());
	double freq250 = goertzel(sound.size(), 250, 48000, sound.data());
	double freq500 = goertzel(sound.size(), 500, 48000, sound.data());
	double freq1000 = goertzel(sound.size(), 1000, 48000, sound.data());
	double freq2000 = goertzel(sound.size(), 2000, 48000, sound.data());
	double freq4000 = goertzel(sound.size(), 4000, 48000, sound.data());
	double freq8000 = goertzel(sound.size(), 8000, 48000, sound.data());
	double freq16000 = goertzel(sound.size(), 16000, 48000, sound.data());
	
	return { freq63, freq125, freq250, freq500, freq1000, freq2000, freq4000, freq8000, freq16000 };
} 

template<class T>
static T getMean(const vector<T>& container) {
	double sum = 0;
	
	for(const auto& element : container)
		sum += element;
		
	return sum / (double)container.size();	
}

static double getSoundImageScore(const vector<double>& dbs) {
	double mean = g_target_mean;
	double score = 0;
	
	//// Ignore 63, 125, 250 since they are variable due to microphone
	vector<double> dbs_above_63(dbs.begin(), dbs.end());
	
	for (size_t i = 0; i < dbs_above_63.size(); i++) {
		auto db = dbs_above_63.at(i);
			
		score += (mean - db) * (mean - db);
	}
	
	return 1 / sqrt(score);
}

static vector<double> getSoundImageCorrection(vector<double> dbs) {
	vector<double> eq;
	
	for (auto& db : dbs) {
		double difference = g_target_mean - db;
		
		eq.push_back(trunc(difference));
	}
	
	return eq;
}

static void setSpeakerVolume(const string& ip, int volume, int base_dsp_level) {
	auto& speaker = Base::system().getSpeaker(ip);
	speaker.setVolume(volume);
	//speaker.setVolume(speaker.getCurrentVolume() + delta_volume);
	
	cout << "Setting speaker volume to " << speaker.getCurrentVolume() << endl;
	int delta = speaker.getCurrentVolume() - SPEAKER_MAX_VOLUME;
	
	// current_dsp_level = -18 ?
	// Gives the speaker 6 dB headroom to boost before DSP limiting on maxed EQ
	int final_level = base_dsp_level + delta;
	
	// Don't boost above limit
	if (final_level > 0) {
		cout << "WARNING" << endl;
		cout << "Trying to boost DSP gain above 0 dB\n";
		cout << "Is " << final_level << endl;
		
		final_level = 0;
	}
	
	unsigned char bytes[4];
	to_523(dB_to_linear_gain(final_level), bytes);
	string hex_string = getHexString(bytes, 4);
	string hex_bytes = "";
	
	cout << "Setting DSP gain bytes to " << hex_string << endl;
	cout << "For level " << final_level << endl;
	
	for (size_t i = 0; i < hex_string.length(); i += 2) 
		hex_bytes += "0x" + hex_string.substr(i, 2) + ",";
		
	hex_bytes.pop_back();	
	
	//string command = "amixer -c1 sset 'Headphone' " + to_string(speaker.getCurrentVolume()) + " on; wait\n";
	string command =	"amixer -c1 cset numid=170 ";
	command +=			hex_bytes;
	command +=			"; wait\n";
	
	Base::system().runScript({ ip }, { command });
}

static vector<double> setSpeakersBestEQ(const vector<string>& ips, const vector<string>& mics) {
	auto speakers = Base::system().getSpeakers(ips);
	vector<string> commands;
	vector<double> scores;
	
	int highest_dsp_gain = -10000;
	
	for (auto* speaker : speakers) {
		if (speaker->getBestVolume() > highest_dsp_gain)
			highest_dsp_gain = speaker->getBestVolume();		
	}
	
	for (auto* speaker : speakers) {
		auto correction_eq = speaker->getBestEQ();
		speaker->setBestVolume();
		//vector<int> correction_eq = { 9, -10, 0, 0, 0, 0, 0, 0, 0 };
		
		string command =	"dspd -s -u preset; wait; ";
		//command +=			"amixer -c1 cset numid=170 0x00,0x80,0x00,0x00; wait; ";
		command +=			"dspd -s -e ";
		
		for (auto setting : correction_eq)
			command += to_string(setting) + ",";
			
		command.pop_back();	
		command +=			"; wait\n";
		
		commands.push_back(command);
		
		cout << "Best score: " << speaker->getBestScore() << endl;
		scores.push_back(speaker->getBestScore());
		
		setSpeakerVolume(speaker->getIP(), speaker->getCurrentVolume(), SPEAKER_MAX_VOLUME - highest_dsp_gain);
	}
	
	Base::system().runScript(ips, commands);
	
	// Set mics to normal values
	Handle::resetEverything(mics);
	
	return scores;
}

static void setCorrectedEQ(const vector<string>& ips) {
	auto speakers = Base::system().getSpeakers(ips);
	vector<string> commands;
	
	for (auto* speaker : speakers) {
		auto correction_eq = speaker->getCorrectionEQ();
		speaker->setCorrectionVolume();
		
		string command =	"dspd -s -u preset; wait; ";
		command +=			"dspd -s -e ";
		
		for (auto setting : correction_eq)
			command += to_string(setting) + ",";
			
		command.pop_back();	
		command +=			"; wait\n";
		
		commands.push_back(command);
		
		setSpeakerVolume(speaker->getIP(), speaker->getCurrentVolume(), -18);
	}
	
	Base::system().runScript(ips, commands);
}

static void setFlatEQ(const vector<string>& ips) {
	auto speakers = Base::system().getSpeakers(ips);
	vector<string> commands;
	
	for (auto* speaker : speakers) {
		speaker->setVolume(SPEAKER_MAX_VOLUME);
		speaker->clearAllEQs();
		
		string command =	"dspd -s -u preset; wait; ";
		//command += 			"amixer -c1 cset numid=170 0x00,0x20,0x26,0xf3; wait; ";
		command +=			"dspd -s -e ";
		
		for (auto setting : vector<int>(9, 0))
			command += to_string(setting) + ",";
			
		command.pop_back();	
		command +=			"; wait\n";
		
		commands.push_back(command);
		
		setSpeakerVolume(speaker->getIP(), speaker->getCurrentVolume(), -18);
	}
	
	Base::system().runScript(ips, commands);
}

static vector<vector<double>> weightEQs(const MicWantedEQ& eqs) {
	// Initialize to 0 EQ
	vector<vector<double>> final_eqs(eqs.front().size(), vector<double>(9, 0));
	
	// Go through all frequency bands
	for (int j = 0; j < 9; j++) {
		// Go through microphones
		for (size_t i = 0; i < eqs.size(); i++) {
			// Go through speakers
			for (size_t k = 0; k < eqs.at(i).size(); k++) {
				// Get EQ at frequency j
				double wanted_eq = eqs.at(i).at(k).at(j);
				
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
	
	cout << endl;
		
	return 1 / (sqrt(total_score) / (double)scores.size());
}

static pair<size_t, double> getLoudestFrequencySource(const string& mic_ip, const vector<string>& speaker_ips, vector<double> simulated_eqs, int frequency_index, bool best_range, double change) {
	// index, db, eq_range
	vector<tuple<size_t, double, double>> loudest;
	
	for (size_t index = 0; index < speaker_ips.size(); index++) {
		// TODO: Should make sense, let's try it later on
		// All additions to the speaker should affect the sound level this way
		#if 0
		auto& speaker = Base::system().getSpeaker(speaker_ips.at(index));
	
		if (speaker.isBlockedEQ(frequency_index))
			continue;
	
		double eq_delta = simulated_eqs.at(index);
		double volume_delta = speaker.getCorrectionVolume() - SPEAKER_MAX_VOLUME;
		double base_level = Base::system().getSpeaker(mic_ip).getFrequencyResponseFrom(speaker_ips.at(index)).at(frequency_index);
		
		double final_volume = base_level + volume_delta + eq_delta;
		double db_change = simulated_eqs.at(index);
		#endif
		
		// Always change the closest speaker first
		double final_volume = Base::system().getSpeaker(mic_ip).getFrequencyResponseFrom(speaker_ips.at(index)).at(frequency_index);
		double db_change = simulated_eqs.at(index);
		
		loudest.push_back(make_tuple(index, final_volume, db_change));
	}
	
	if (loudest.empty())
		loudest.push_back(make_tuple(0, 10, 10));
	
	// Sort by loudness
	sort(loudest.begin(), loudest.end(), [] (auto& first, auto& second) {
		return get<1>(first) > get<1>(second);
	});
			
	if (!best_range) {		
		// Always pick the loudest one since we don't care about gain
		return { get<0>(loudest.front()), get<2>(loudest.front()) };
	}
	
	// Pick the first with non-maxed EQ at chosen frequency
	for (auto& information : loudest) {
		// If we're improving a low EQ, let's do it
		if (get<2>(information) < DSP_MIN_EQ && change > 0)
			return { get<0>(information), get<2>(information) };
			
		// If we're lowering a high EQ, let's do it	
		if (get<2>(information) > DSP_MAX_EQ && change < 0)
			return { get<0>(information), get<2>(information) };
			
		// If it fits into the range, let's do it
		if (get<2>(information) < DSP_MAX_EQ && get<2>(information) > DSP_MIN_EQ)
			return { get<0>(information), get<2>(information) };
	}
	
	// All speakers were maxed, pick the one with least maxed EQ
	sort(loudest.begin(), loudest.end(), [] (auto& first, auto& second) {
		return get<2>(first) < get<2>(second);
	});
	
	return { get<0>(loudest.front()), get<2>(loudest.front()) };
}

static vector<double> getSpeakerEQChange(const string& mic_ip, const vector<string>& speaker_ips, int frequency_index, double wanted_change) {
	// Added EQ
	vector<double> added_eq(speaker_ips.size(), 0);
	
	//cout << "Wanted change " << wanted_change << endl;
	
	for (int i = 0; i < lround(abs(wanted_change)); i++) {
		// Current testing EQ
		vector<double> test_eq(added_eq);
		
		for (size_t i = 0; i < test_eq.size(); i++)
			test_eq.at(i) += Base::system().getSpeaker(speaker_ips.at(i)).getCorrectionEQ().at(frequency_index);

		double db_change = wanted_change > 0 ? 1 : -1;
		
		for (auto& eq : added_eq)
			eq += db_change;
			
		size_t main_index = 0;
		auto index = getLoudestFrequencySource(mic_ip, speaker_ips, test_eq, frequency_index, false, db_change);
		added_eq.at(index.first) += db_change;
		test_eq.at(index.first) += db_change;
		main_index = index.first;
		
		double range = index.second += db_change;
		size_t iterations = 0;

		// Can't make it lower or higher anyway, give it to another speaker
		while (db_change < 0 ? (range <= DSP_MIN_EQ) : (range >= DSP_MAX_EQ)) {
			index = getLoudestFrequencySource(mic_ip, speaker_ips, test_eq, frequency_index, true, db_change);
			
			if (index.first != main_index) {
				added_eq.at(index.first) += db_change;
				test_eq.at(index.first) += db_change;
				range = index.second += db_change;
			}
			
			// Already gone through all speakers
			if (++iterations >= speaker_ips.size())
				break;
		}
	}
	
	//cout << "Added EQ: ";
	//for_each(added_eq.begin(), added_eq.end(), [] (auto& eq) { cout << eq << " "; });
	//cout << endl;
	
	return added_eq;
}

SoundImageFFT9 Handle::checkSoundImage(const vector<string>& speakers, const vector<string>& mics, int play_time, int idle_time, bool corrected) {
	vector<string> all_ips(speakers);
	all_ips.insert(all_ips.end(), mics.begin(), mics.end());
	
	// Set corrected EQ if we're trying the corrected sound image or restore flat settings
	if (corrected) {
		setCorrectedEQ(speakers);
	} else {
		auto scripts = createDisableAudioSystem(all_ips);
		Base::system().runScript(all_ips, scripts);
		
		setFlatEQ(speakers);
		
		// Clear saved information in mics as well
		for (auto& mic_ip : mics)
			Base::system().getSpeaker(mic_ip).clearAllEQs();
	}
	
	if (!Config::get<bool>("no_scripts")) {
		// Get sound image from available microphones
		vector<string> scripts;
		
		if (corrected)
			scripts = createSoundImageScripts(speakers, mics, play_time, idle_time, Config::get<string>("white_noise"));
		else
			scripts = createSoundImageIndividualScripts(speakers, mics, play_time, idle_time, Config::get<string>("white_noise"));
		
		vector<string> all_ips(speakers);
		all_ips.insert(all_ips.end(), mics.begin(), mics.end());
		
		if (runBasicScriptExternal(speakers, mics, all_ips, scripts, "data/" + Config::get<string>("white_noise"), "/tmp/").empty())
			return SoundImageFFT9();
	}
		
	SoundImageFFT9 final_result;
	MicWantedEQ weighted_eq;
	vector<double> scores;
		
	for (size_t i = 0; i < mics.size(); i++) {
		string filename = "results/cap" + mics.at(i) + ".wav";
		
		vector<short> data;
		WavReader::read(filename, data);
		
		if (data.empty())
			return SoundImageFFT9();
		
		if (corrected) {
			size_t sound_start = ((double)idle_time + 0.3) * 48000;
			size_t sound_average = getRMS(data, sound_start, sound_start + (48000 / 2));
			
			double db = 20 * log10(sound_average / (double)SHRT_MAX);
			
			// Calculate FFT for 9 band as well
			auto db_fft = getFFT9(data, sound_start, sound_start + (48000 / 2));
			vector<double> dbs;
			
			cout << "Microphone \t" << mics.at(i) << endl;
			
			for (size_t z = 0; z < db_fft.size(); z++) {
				auto& freq = db_fft.at(z);
				double db_freq = 20 * log10(freq / (double)SHRT_MAX);
				
				cout << "Frequency \t" << g_frequencies.at(z) << ": \t" << db_freq << endl;
				dbs.push_back(db_freq);
			}
			
			// Is this the first run ever?
			
			if (!speakers.empty()) {
				if (Base::system().getSpeaker(speakers.front()).isFirstRun()) {
					double new_mean = getMean(dbs) - 3;
					g_target_mean = new_mean;
					
					cout << "Set target mean " << g_target_mean << endl;
				}
			}
			
			// Calculate score
			auto score = getSoundImageScore(dbs);
			
			// Calculate correction & set it to speakers (alpha)
			auto correction = getSoundImageCorrection(dbs);
			
			// Correct EQ
			vector<vector<double>> corrected_dbs(speakers.size());
			
			// Calculate factors (room dependent?)
			// Sets sensitive bands (i.e. small adjustments will amplify)
			auto last_dbs = Base::system().getSpeaker(mics.at(i)).getLastChange().first;
			auto last_corrections = Base::system().getSpeaker(mics.at(i)).getLastChange().second;

			for (size_t f = 0; f < last_dbs.size(); f++) {
				auto last_db = last_dbs.at(f);
				auto last_correction = last_corrections.at(f);
				auto current_db = dbs.at(f);
				
				// We did not change any settings for this band
				if (last_correction < 0.5)
					continue;
				
				double change = current_db - last_db;
				double factor = change / last_correction;
				
				#if 0
				Base::system().getSpeaker(mics.at(i)).setBandSensitive(f, true);
				
				// TODO: Calculate the factor & correction based on something else later on
				if (factor > 2) {
					// Set this band to sensitive
					//Base::system().getSpeaker(mics.at(i)).setBandSensitive(f, true);
				}
				#endif
				
				if (abs(factor) > 6) {
					// Which speaker caused this effect?
					auto speaker_vector = Base::system().getSpeakers(speakers);
					auto iterator = find_if(speaker_vector.begin(), speaker_vector.end(), [&f] (auto* speaker) {
						return abs(speaker->getLastEQChange().at(f)) >= 0.5;
					});
					
					if (iterator != speaker_vector.end()) {
						cout << "FOUND SPEAKER TO BLOCK BAND\n";
						
						// Reset that speaker to the last value of the frequency band and prevent it from being changed again
						//(*iterator)->resetLastEQChange(f);
						//(*iterator)->preventEQChange(f);
					} else {
						cout << "DID NOT FIND MALICOUS SPEAKER\n";
					}
				}
				
				/*
				if (speakers.size() > 1)
					if (factor < 0)
						correction.at(f) *= -1;
						*/
			}
			
			/*
			cout << "Total EQ correction wanted by mic: ";
			for_each(correction.begin(), correction.end(), [] (auto& eq) { cout << eq << endl; });
			cout << endl;
			*/
			
			// Go through all frequency bands
			for (int d = 0; d < 9; d++) {
				if (correction.at(d) > 1)
					correction.at(d) = 1;
				else if (correction.at(d) < -1)
					correction.at(d) = -1;
				// Is this band sensitive? Only adjust it with 1 dB each run
				//if (Base::system().getSpeaker(mics.at(i)).isBandSensitive(d)) {
					
						
				//	cout << "Band " << g_frequencies[d] << " is sensitive, meaning abs(-1)\n";
				//}
				
				// Which speaker was already loudest here? Adjust that one since
				// boosting less sounding speakers won't affect the sound image
				// as much since combining them with a louder source will drown the
				// adjusted sound
				// Source: http://www.csgnetwork.com/decibelamplificationcalc.html
				
				// More information about incoherent/coherent sound sources here
				// http://www.sengpielaudio.com/calculator-spl.htm
				
				auto wanted_change = correction.at(d);
				
				// How the EQ should be
				auto speaker_eq_change = getSpeakerEQChange(mics.at(i), speakers, d, wanted_change);
				
				for (size_t e = 0; e < corrected_dbs.size(); e++)
					corrected_dbs.at(e).push_back(speaker_eq_change.at(e));
			}
			
			// Add this to further calculations when we have all the information
			weighted_eq.push_back(corrected_dbs);
			scores.push_back(score);
			
			// Update last results
			Base::system().getSpeaker(mics.at(i)).setLastChange(dbs, correction);
			
			// 9 band dB first, then time domain dB
			dbs.push_back(db);
			
			final_result.push_back(make_tuple(mics.at(i), dbs, score));
		} else {
			for (size_t j = 0; j < speakers.size(); j++) {
				size_t sound_start = ((double)idle_time + j * (play_time + 1) + 0.3) * 48000;
				
				// Calculate FFT for 9 band as well
				auto db_fft = getFFT9(data, sound_start, sound_start + (48000 / 2));
				vector<double> dbs;
				
				for (size_t z = 0; z < db_fft.size(); z++) {
					auto& freq = db_fft.at(z);
					double db_freq = 20 * log10(freq / (double)SHRT_MAX);
					
					dbs.push_back(db_freq);
				}
				
				// DBs is vector of the current speaker with all it's frequency dBs
				Base::system().getSpeaker(mics.at(i)).setFrequencyResponseFrom(speakers.at(j), dbs);
			}
		}
	}
	
	if (corrected) {
		// Weight mics' different EQs against eachother
		auto final_eqs = weightEQs(weighted_eq);
		
		// Calculate final score
		auto final_score = getFinalScore(scores);
		
		// Set new EQs
		auto actual_speakers = Base::system().getSpeakers(speakers);
		
		for (size_t d = 0; d < actual_speakers.size(); d++)
			actual_speakers.at(d)->setCorrectionEQ(final_eqs.at(d), final_score);
	}
	
	return final_result;
}

vector<double> Handle::setBestEQ(const vector<string>& speakers, const vector<string>& mics) {
	vector<string> all_ips(speakers);
	all_ips.insert(all_ips.end(), mics.begin(), mics.end());
	
	// Enable audio system again
	auto scripts = createEnableAudioSystem(all_ips);
	Base::system().runScript(all_ips, scripts);
	
	return setSpeakersBestEQ(speakers, mics);
}

void Handle::setEQStatus(const vector<string>& ips, bool status) {
	string command =	"dspd -s -" + string((status ? "u" : "b"));
	command +=			" preset; wait\n";
		
	cout << "Change EQ command: " << command << endl;
	
	Base::system().runScript(ips, vector<string>(ips.size(), command));
}