#ifndef HANDLE_H
#define HANDLE_H

#include <vector>
#include <string>
#include <array>

using PlacementOutput = std::vector<std::tuple<std::string, std::array<double, 3>, std::vector<std::pair<std::string, double>>>>;
using MicWantedEQ = std::vector<std::vector<std::vector<double>>>;
using SSHOutput = std::vector<std::pair<std::string, std::vector<std::string>>>;

class Handle {
public:
	static PlacementOutput runLocalization(const std::vector<std::string>& ips, bool force_update);
	static std::vector<bool> checkSpeakersOnline(const std::vector<std::string>& ips);
	static void checkSoundImage(const std::vector<std::string>& speakers, const std::vector<std::string>& mics, int iterations);
	static void setBestEQ(const std::vector<std::string>& speakers, const std::vector<std::string>& mics);
	static void setEQStatus(const std::vector<std::string>& ips, bool status);
	static void resetIPs(const std::vector<std::string>& ips);
};

#endif