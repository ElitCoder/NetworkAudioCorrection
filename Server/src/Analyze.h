#pragma once
#ifndef NAC_ANALYZE_H
#define NAC_ANALYZE_H

#include <vector>

using FFTOutput = std::pair<std::vector<double>, std::vector<double>>;
using BandOutput = std::vector<double>;

// NetworkAudioCorrection
namespace nac {
	FFTOutput fft(const std::vector<short>& samples);
	BandOutput calculate(const FFTOutput& input);
}

#endif