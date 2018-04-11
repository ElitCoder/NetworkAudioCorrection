#ifndef HANDLE_H
#define HANDLE_H

#include <vector>
#include <string>
#include <array>

using SSHOutput = std::vector<std::pair<std::string, std::vector<std::string>>>;
using SoundImageFFT9 = std::vector<std::tuple<std::string, std::vector<double>, double>>;
using PlacementOutput = std::vector<std::tuple<std::string, std::array<double, 3>, std::vector<std::pair<std::string, double>>>>;
using MicWantedEQ = std::vector<std::vector<std::vector<double>>>;

class Handle {
public:
	static bool setSpeakerAudioSettings(const std::vector<std::string>& ips, const std::vector<int>& volumes, const std::vector<int>& captures, const std::vector<int>& boosts);
	static PlacementOutput runLocalization(const std::vector<std::string>& ips, bool skip_script, bool force_update);
	static std::vector<bool> checkSpeakersOnline(const std::vector<std::string>& ips);
	static SoundImageFFT9 checkSoundImage(const std::vector<std::string>& speakers, const std::vector<std::string>& mics, int play_time, int idle_time, bool corrected);
	static std::vector<double> setBestEQ(const std::vector<std::string>& speakers, const std::vector<std::string>& mics);
	static void setEQStatus(const std::vector<std::string>& ips, bool status);
	static void resetEverything(const std::vector<std::string>& ips);
};

#endif