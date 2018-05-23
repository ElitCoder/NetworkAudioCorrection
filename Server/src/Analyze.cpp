#include "Analyze.h"
#include "Speaker.h"
#include "Profile.h"

// TODO: Remove this dependency
#include "Base.h"
#include "System.h"

// SigPack
#include <sigpack/sigpack.h>

#include <vector>
#include <iostream>

using namespace std;

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

#if 0
static int getClosestIndex(double frequency, const vector<double>& container) {
	double closest = INT_MAX;
	int closest_index = -1;
	
	for (size_t i = 0; i < container.size(); i++) {
		double distance = abs(container.at(i) - frequency);
		
		if (distance < closest) {
			closest = distance;
			closest_index = i;
		}
	}
	
	return closest_index;
}
#endif

namespace nac {
	FFTOutput doFFT(const vector<short>& samples) {
		vector<double> in;
		
		for (auto& sample : samples)
			in.push_back((double)sample / (double)SHRT_MAX);
		
		const int N = 8192;
		arma::vec y = arma::abs(sp::pwelch(arma::vec(in), N, N / 2));
		
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
		
		double std_dev = calculateSD(output);
		cout << "std_dev " << std_dev << endl;
			
		return { frequencies, output };
	}
	
	FFTOutput getDifference(const FFTOutput& input, double target, bool use_mean) {
		auto& frequencies = input.first;
		auto& magnitudes = input.second;
		
		auto mean_target = mean(magnitudes);
		vector<double> difference;
		
		for (auto& db : magnitudes)
			difference.push_back((use_mean ? mean_target : target) - db);
			
		return { frequencies, difference };
	}
	
	FFTOutput applyProfiles(const FFTOutput& input, const Profile& speaker_profile, const Profile& microphone_profile) {
		auto low = max(speaker_profile.getLowCutOff(), microphone_profile.getLowCutOff());
		auto high = min(speaker_profile.getHighCutOff(), microphone_profile.getHighCutOff());
		auto steep_low = max(speaker_profile.getSteepLow(), microphone_profile.getSteepLow());
		auto steep_high = max(speaker_profile.getSteepHigh(), microphone_profile.getSteepHigh());
		
		FFTOutput output = input;
		
		// Actually N / 2
		//const int N = input.first.size();
		auto& frequencies = output.first;
		auto& dbs = output.second;

		// Set cutoffs to 0 and everything outside to steep * length
		for (size_t i = 0; i < frequencies.size(); i++) {
			auto& frequency = frequencies.at(i);
			auto& db = dbs.at(i);
			
			// How steep should this be?
			if (frequency < low) {
				double steps = log2(low / frequency);
				double attenuation = steps * steep_low;
				
				db += attenuation;				
			} else if (frequency > high) {
				double steps = log2(frequency / high);
				double attenuation = steps * steep_high;
				
				db += attenuation;
			}
		}
			
		return output;
	}
	
	FFTOutput toDecibel(const FFTOutput& input) {
		FFTOutput output = input;
		auto& linear = output.second;
		
		for (auto& energy : linear)
			energy = 10 * log10(energy);
			
		return output;	
	}
	
	vector<double> getEQ(const FFTOutput& input, const pair<vector<double>, double>& eq_settings) {
		return fitBands(input, eq_settings, false);
	}
	
	vector<double> findSimulatedEQSettings(const vector<short>& samples, const Filter& filter) {
		return vector<double>();
	}
	
	vector<double> fitBands(const FFTOutput& input, const pair<vector<double>, double>& eq_settings, bool input_db) {
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
			
			//cout << "Added band limit " << lower << endl;
			//cout << "Added band limit " << upper << endl;
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
			
			// Convert to dB
			if (!input_db)
				energy.at(i) = 10 * log10(energy.at(i));
			
			cout << "Frequency\t" << eq_frequencies.at(i) << "\t:\t" << energy.at(i) << endl;
		}
		
		auto db_std_dev = calculateSD(energy);
		cout << "db_std_dev " << db_std_dev << endl;
		
		return energy;
	}
}