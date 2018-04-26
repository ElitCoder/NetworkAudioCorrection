#include "Analyze.h"
#include "Speaker.h"

// FFTW3
#include <fftw3.h>

#include <vector>
#include <climits>
#include <cmath>
#include <iostream>

using namespace std;

static const vector<int> band_limits = {	44,		88,
											88,		177,
											177,	355,
											355,	710,
											710,	1420,
											1420,	2840,
											2840,	5680,
											5680,	11360,
											11360,	22720	};

// Not used right now
#if 0								
static const vector<double> eq_limits = {	38.936,		101.936,
											77.254,		202.254,
											154.508,	404.508,
											309.017,	809.017,
											618.034,	1618.034,
											1236.068,	3236.068,
											2472.136,	6472.136,
											4944.272,	12944.272,
											9888.544,	25888.544	};										
#endif

static vector<string> g_frequencies =	{	"63",
											"125",
											"250",
											"500",
											"1000",
											"2000",
											"4000",
											"8000",
											"16000"	};

// Specific profiles depending on hardware
// If we're recording outside of these ranges, changes won't matter since the microphone doesn't pick it up anyway
// If we're boosting outside of these ranges, changes won't matter since the speaker won't play these frequencies
// So, ignore them in the calibration since it won't be effective in real life anyway
const bool	ENABLE_DEVICE_PROFILES	= true;
const int	SPEAKER_MIN_HZ			= 60;
const int	SPEAKER_MAX_HZ			= 20000;
const int	MIC_MIN_HZ				= 20;
const int	MIC_MAX_HZ				= 20000;

template<class T>
static T mean(const vector<T>& container) {
	double sum = 0;
	
	for(const auto& element : container)
		sum += element;
		
	return sum / (double)container.size();	
}

static double calculateSD(const vector<double>& data) {
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

static int getBandIndex(double frequency) {
	// TODO: Microphone/speaker should be profile inputs to the program
	// Mic can't pick up this anyway
	if (ENABLE_DEVICE_PROFILES) {
		if (frequency < MIC_MIN_HZ || frequency > MIC_MAX_HZ)
			return -1;
			
		// Speaker can't play this anyway
		if (frequency < SPEAKER_MIN_HZ || frequency > SPEAKER_MAX_HZ)
			return -1;
	}
	
	for (size_t i = 0; i < band_limits.size(); i += 2) {
		auto& lower = band_limits.at(i);
		auto& higher = band_limits.at(i + 1);
		
		if (frequency >= lower && frequency < higher)
			return i / 2;
	}
	
	return -1;
}

namespace nac {
	vector<double> availability() {
		// Only use this if we're using device profiles
		if (!ENABLE_DEVICE_PROFILES) {
			cout << "Warning: calling nac::availability() with device profiles disabled\n";
			
			return vector<double>(DSP_MAX_BANDS, 1);
		}
			
		// Tests show that this is only valid if we're using 1 speaker, disable it for real testing
		double first = band_limits.at(1) - band_limits.front();
		double last = band_limits.back() - band_limits.at(band_limits.size() - 2);
		
		// How much of the first and last band are affected with changing the DSP since we cut values < 60 & > 20k
		double first_available = band_limits.at(1) - max(SPEAKER_MIN_HZ, MIC_MIN_HZ);
		double last_available = min(MIC_MAX_HZ, SPEAKER_MAX_HZ) - band_limits.at(band_limits.size() - 2);
		
		double first_factor = first_available / first;
		double last_factor = last_available / last;

		vector<double> available_change = { first_factor, 1, 1, 1, 1, 1, 1, 1, last_factor };
		
		cout << "Available DSP change: ";
		for (auto& available : available_change)
			cout << available << " ";
		cout << endl;
		
		return available_change;
	}
	
	FFTOutput fft(const vector<short>& samples) {
		vector<double> in;
		
		for (auto& sample : samples)
			in.push_back((double)sample / SHRT_MAX);

		int N = samples.size();
		double fs = 48000;
		
		fftw_complex *out = (fftw_complex*)fftw_malloc(sizeof(fftw_complex) * N);
		fftw_plan plan_forward = fftw_plan_dft_r2c_1d(N, in.data(), out, FFTW_ESTIMATE);
		fftw_execute(plan_forward);

		vector<double> v;

		for (int i = 0; i <= ((N/2)-1); i++)
			// Linear energy
			v.push_back(sqrt(out[i][0] * out[i][0] + out[i][1] * out[i][1]) / N);
		
		vector<double> frequencies;
		vector<double> magnitudes;
		
		for (int i = 0; i <= ((N / 2) - 1); i++) {
			frequencies.push_back(fs * i / N);
			magnitudes.push_back(v[i]);
		}

		fftw_destroy_plan(plan_forward);
		fftw_free(out);
		
		return { frequencies, magnitudes };
	}
	
	BandOutput calculate(const FFTOutput& input) {
		auto& frequencies = input.first;
		auto& magnitudes = input.second;
		
		vector<double> band_energy(band_limits.size() / 2, 0);
		vector<double> band_nums(band_limits.size() / 2, 0);
		
		// Analyze bands
		for (size_t i = 0; i < frequencies.size(); i++) {
			auto& frequency = frequencies.at(i);
			auto index = getBandIndex(frequency);
			
			if (index < 0)
				continue;
			
			band_energy.at(index) += magnitudes.at(i);
			band_nums.at(index)++;
		}
		
		// Calculate mean band energy
		for (size_t i = 0; i < band_energy.size(); i++)
			band_energy.at(i) /= band_nums.at(i);
		
		vector<double> gains;
	
		for (size_t i = 0; i < band_energy.size(); i++) {
			auto& energy = band_energy.at(i);
			double gain = 20 * log10(energy / (double)SHRT_MAX);
			
			cout << "Gain " << g_frequencies.at(i) << "\t: " << gain << endl;
			gains.push_back(gain);
		}
		
		auto std_dev = calculateSD(band_energy);
		auto std_dev_db = calculateSD(gains);
		
		cout << "Standard deviation: " << std_dev << endl;
		cout << "Standard deviation dB " << std_dev_db << endl;
		
		return { band_energy, gains };
	}
}