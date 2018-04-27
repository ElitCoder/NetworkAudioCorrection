#pragma once
#ifndef NAC_ANALYZE_H
#define NAC_ANALYZE_H

#include <vector>

// pair < Frequencies, Energy || Magnitudes >
using FFTOutput = std::pair<std::vector<double>, std::vector<double>>;

class Profile;

// NetworkAudioCorrection
namespace nac {
	FFTOutput doFFT(const std::vector<short>& samples);
	FFTOutput getDecibelDifference(const FFTOutput& input, double target_db);
	FFTOutput applyProfiles(const FFTOutput& input, const Profile& speaker_profile, const Profile& microphone_profile);
	FFTOutput toDecibel(const FFTOutput& input);
	std::vector<double> fitBands(const FFTOutput& input, const std::pair<std::vector<double>, double>& eq_settings);
	std::vector<double> getEQ(const FFTOutput& input, const std::pair<std::vector<double>, double>& eq_settings);
}

#endif