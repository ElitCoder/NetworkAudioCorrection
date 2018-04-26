#include "Analyze.h"
#include "Speaker.h"
#include "Profile.h"

// SigPack
#include <sigpack/sigpack.h>

#include <vector>
#include <iostream>

using namespace std;

// SigPack
using namespace arma;
using namespace sp;

#if 0
static const vector<int> band_limits = {	44,		88,
											88,		177,
											177,	355,
											355,	710,
											710,	1420,
											1420,	2840,
											2840,	5680,
											5680,	11360,
											11360,	22720	};
#endif

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

#if 0
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
const bool	ENABLE_DEVICE_PROFILES_INPUT	= false;
const bool	ENABLE_DEVICE_PROFILES_OUTPUT	= false;
const int	SPEAKER_MIN_HZ					= 60;
const int	SPEAKER_MAX_HZ					= 20000;
const int	MIC_MIN_HZ						= 20;
const int	MIC_MAX_HZ						= 20000;
#endif

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

static int getBandIndex(double frequency, const vector<double>& limits) {
	for (int i = 0; i < (int)limits.size(); i += 2) {
		if (frequency >= limits.at(i) && frequency < limits.at(i + 1))
			return i / 2;
	}
	
	return -1;
}

namespace nac {
	FFTOutput doFFT(const vector<short>& samples) {
		vector<double> in;
		
		for (auto& sample : samples)
			in.push_back((double)sample / SHRT_MAX);
		
		const int N = 8192;
		vec y = arma::abs(sp::pwelch(vec(in), N, N / 2));
		
		vector<double> output;
		
		for (auto& element : y)
			output.push_back(element);

		// Output is [0, fs)
		output = vector<double>(output.begin(), output.begin() + output.size() / 2);
			
		cout << "mean(output) " << mean(output) << endl;
		cout << "output.size() " << output.size() << endl;
		cout << "N " << N << endl;
		
		vector<double> frequencies;
		
		for (size_t i = 0; i < output.size(); i++) {
			double frequency = i * 48000.0 / N;
			
			frequencies.push_back(frequency);
		}
			
		return { frequencies, output };
	}
	
	FFTOutput getDecibelDifference(const FFTOutput& input, double target_db) {
		auto& frequencies = input.first;
		auto& magnitudes = input.second;
		vector<double> gain_db;
		vector<double> difference_db;
		
		for (size_t i = 0; i < frequencies.size(); i++) {
			auto& energy = magnitudes.at(i);
			gain_db.push_back(20 * log10(energy));
			
			//cout << "Frequency\t" << frequencies.at(i) << "\t:\t" << gain_db.back() << endl;
		}
		
		//auto db_mean = mean(gain_db);
		auto db_std_dev = calculateSD(gain_db);
		
		//cout << "db_mean " << db_mean << endl;
		cout << "target_db " << target_db << endl;
		cout << "db_std_dev " << db_std_dev << endl;
		
		for (size_t i = 0; i < gain_db.size(); i++) {
			auto& db = gain_db.at(i);
			difference_db.push_back(target_db - db);
			
			cout << "Frequency difference\t" << frequencies.at(i) << "\t:\t" << difference_db.back() << endl;
		}
			
		return { frequencies, difference_db };
	}
	
	FFTOutput applyProfiles(const FFTOutput& input, const Profile& speaker_profile, const Profile& microphone_profile) {
		auto low = max(speaker_profile.getLowCutOff(), microphone_profile.getLowCutOff());
		auto high = min(speaker_profile.getHighCutOff(), microphone_profile.getHighCutOff());
		auto steep = max(speaker_profile.getSteep(), microphone_profile.getSteep());
		
		FFTOutput output = input;
		
		// Actually N / 2
		const int N = input.first.size();
		auto& frequencies = output.first;
		auto& dbs = output.second;
		
		vector<double> graph(N, 0);
		
		// Set cutoffs to 0 and everything outside to steep * length
		for (size_t i = 0; i < frequencies.size(); i++) {
			auto& frequency = frequencies.at(i);
			auto& db = dbs.at(i);
			
			// How steep should this be?
			if (frequency < low) {
				double steps = log2(low / frequency);
				double attenuation = steps * steep;
				
				db += attenuation;
				
				cout << "Attenuated " << frequency << " " << attenuation << endl;
			} else if (frequency > high) {
				double steps = log2(frequency / high);
				double attenuation = steps * steep;
				
				db += attenuation;
				
				cout << "Attenuated " << frequency << " " << attenuation << endl;
			}
		}
			
		return output;
	}
	
	FFTOutput toDecibel(const FFTOutput& input) {
		FFTOutput output = input;
		auto& linear = output.second;
		
		for (auto& energy : linear)
			energy = 20 * log10(energy);
			
		return output;	
	}
	
	vector<double> fitEQ(const FFTOutput& input, const pair<vector<double>, double>& eq_settings) {
		auto& eq_frequencies = eq_settings.first;
		//auto& q = eq_settings.second;
		
		// TODO: Make this more generic by input band size in octaves, e.g. 1/3 octaves and so on
		// Calculate band limits
		vector<double> band_limits;
		
		for (auto& centre : eq_frequencies) {
			double width = pow(2.0, 1.0 / 2.0);
			double lower = centre / width;
			double upper = centre * width;
			
			band_limits.push_back(lower);
			band_limits.push_back(upper);
			
			cout << "Added band limit " << lower << endl;
			cout << "Added band limit " << upper << endl;
		}
		
		vector<double> energy(eq_frequencies.size(), 0);
		vector<double> num(eq_frequencies.size(), 0);
		
		auto& frequencies = input.first;
		auto& dbs = input.second;
		
		for (size_t i = 0; i < dbs.size(); i++) {
			auto& frequency = frequencies.at(i);
			auto index = getBandIndex(frequency, band_limits);
			
			// This frequency is outside band range
			if (index < 0)
				continue;
				
			energy.at(index) += dbs.at(i);
			num.at(index)++;	
		}
		
		cout << "Lower resolution to fit EQ band with size " << eq_frequencies.size() << endl;
		
		for (size_t i = 0; i < num.size(); i++) {
			energy.at(i) /= num.at(i);
			
			cout << "Frequency\t" << eq_frequencies.at(i) << "\t:\t" << energy.at(i) << endl;
		}
		
		return energy;
	}
}