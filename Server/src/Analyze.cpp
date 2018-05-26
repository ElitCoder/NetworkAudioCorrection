#include "Analyze.h"
#include "Speaker.h"
#include "Profile.h"

// TODO: Remove this dependency
#include "Base.h"
#include "System.h"
#include "Config.h"

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

static double correctMaxEQ(vector<double>& eq) {
	double total_mean_change = 0;
	auto min_eq = Base::system().getSpeakerProfile().getMinEQ();
	auto max_eq = Base::system().getSpeakerProfile().getMaxEQ();
	
	for (int i = 0; i < 1000; i++) {
		double mean_db = mean(eq);
		
		for (auto& setting : eq)
			setting -= mean_db;
				
		for (auto& setting : eq) {
			if (setting < min_eq)
				setting = min_eq;
			else if (setting > max_eq)
				setting = max_eq;
		}
		
		total_mean_change += mean_db;
	}
	
	return total_mean_change;
}

namespace nac {
	FFTOutput doFFT(const vector<short>& samples, size_t start, size_t stop) {
		vector<double> in;
		
		// Normalize to [0, 1]
		for (size_t i = start; i < (stop == 0 ? samples.size() : stop); i++)
			in.push_back((double)samples.at(i) / (double)SHRT_MAX);
		
		const int N = 8192;
		arma::vec y = arma::abs(sp::pwelch(arma::vec(in), N, N / 2));
		
		vector<double> output;
		vector<double> frequencies;
		
		// pwelch() output is [0, fs)
		for (size_t i = 0; i < y.size() / 2; i++) {
			double frequency = i * 48000.0 / N;
			
			frequencies.push_back(frequency);
			output.push_back(y.at(i));
		}
			
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
	
	FFTOutput toLinear(const FFTOutput& input) {
		FFTOutput output = input;
		auto& dbs = output.second;
		
		for (auto& db : dbs)
			db = pow(10, db / 10);
			
		return output;	
	}
	
	vector<double> getEQ(const FFTOutput& input, const pair<vector<double>, double>& eq_settings) {
		return fitBands(input, eq_settings, false).first;
	}
	
	vector<double> findSimulatedEQSettings(const vector<short>& samples, FilterBank& filter, size_t start, size_t stop) {
		auto speaker_eq = Base::system().getSpeakerProfile().getSpeakerEQ();
		auto& speaker_eq_frequencies = speaker_eq.first;
		
		vector<double> eq_change(speaker_eq_frequencies.size(), 0);
		vector<double> best_eq;
		double best_score = INT_MAX;
		
		for (int i = 0; i < Base::config().get<int>("max_simulation_iterations"); i++) {
			correctMaxEQ(eq_change);
			
			vector<pair<int, double>> gains;
			
			cout << "Trying EQ: ";
			for (size_t i = 0; i < speaker_eq_frequencies.size(); i++) {
				gains.push_back({ speaker_eq_frequencies.at(i), eq_change.at(i) });
				
				cout << eq_change.at(i) << " ";
			}
			cout << endl;
			
			vector<short> simulated_samples;
			filter.apply(samples, simulated_samples, gains, 48000);
			
			// Find basic EQ change
			auto response = nac::doFFT(simulated_samples, start, stop);
			
			cout << "Transformed to:\n";
			auto peer = nac::fitBands(response, speaker_eq, false);
			
			if (Base::config().get<bool>("enable_hardware_profile")) {
				response = nac::toDecibel(response);
				
				auto speaker_profile = Base::system().getSpeakerProfile().invert();
				auto mic_profile = Base::system().getMicrophoneProfile().invert();
				
				if (Base::config().get<bool>("hardware_profile_boost_steeps")) {
					speaker_profile = Base::system().getSpeakerProfile();
					mic_profile = Base::system().getMicrophoneProfile();
				}
				
				response = nac::applyProfiles(response, speaker_profile, mic_profile);
				
				// Revert back to energy
				response = nac::toLinear(response);
				
				cout << "After hardware profile:\n";
				peer = nac::fitBands(response, speaker_eq, false);
			}
			
			auto& negative_curve = peer.first;
			auto& db_std_dev = peer.second;

			// Negative response to get change curve
			vector<double> eq;
			
			for (auto& value : negative_curve)
				eq.push_back(value * (-1));
			
			if (db_std_dev < best_score) {
				best_score = db_std_dev;
				best_eq = eq_change;
			}
			
			if (!best_eq.empty()) {
				if (i > 10) {
					if (best_score < 0.1)
						break;
				}
			}
			
			cout << "Adding EQ: ";
			for (size_t i = 0; i < eq.size(); i++) {
				eq_change.at(i) += eq.at(i);
				
				cout << eq.at(i) << " ";
			}
			cout << endl;
			
			cout << endl;
		}
		
		return best_eq;
	}
	
	pair<vector<double>, double> fitBands(const FFTOutput& input, const pair<vector<double>, double>& eq_settings, bool input_db, const vector<int>& ignore_bands) {
		auto& eq_frequencies = eq_settings.first;
		
		// TODO: Make this more generic by input band size in octaves, e.g. 1/3 octaves and so on
		// Calculate band limits
		vector<double> band_limits;
		
		for (auto& centre : eq_frequencies) {
			double width = pow(2.0, 1.0 / 2.0);
			double lower = centre / width;
			double upper = centre * width;
			
			band_limits.push_back(lower);
			band_limits.push_back(upper);
		}
		
		vector<double> energy(eq_frequencies.size(), 0);
		vector<double> num(eq_frequencies.size(), 0);
		
		FFTOutput actual = input;
		
		if (input_db)
			cout << "Warning: dB input not supported\n";
			
		auto& frequencies = actual.first;
		auto& dbs = actual.second;
		
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
			if (Base::config().get<bool>("is_white_noise")) {
				// Divide with the octave width
				auto low = band_limits.at(i * 2);
				auto high = band_limits.at(i * 2 + 1);
				
				energy.at(i) /= (high - low);
			}
				
			// Convert to dB
			if (!input_db)
				energy.at(i) = 10 * log10(energy.at(i));
			
			cout << "Frequency\t" << eq_frequencies.at(i) << "\t:\t" << energy.at(i) << endl;
		}
		
		vector<double> left;
		
		for (size_t i = 0; i < energy.size(); i++)
			if (find(ignore_bands.begin(), ignore_bands.end(), i) == ignore_bands.end())
				left.push_back(energy.at(i));
		
		auto db_std_dev = calculateSD(energy);
		cout << "db_std_dev " << db_std_dev << endl;
		
		if (!ignore_bands.empty()) {
			db_std_dev = calculateSD(left);
			cout << "db_std_dev " << db_std_dev << " excluding band ";
			for (auto& band : ignore_bands)
				cout << band << " ";
			cout << endl;
		}
		
		return { energy, db_std_dev };
	}
}