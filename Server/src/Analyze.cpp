#include "Analyze.h"
#include "Speaker.h"
#include "Profile.h"

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
			in.push_back((double)sample / SHRT_MAX);
		
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
	
	#if 0
	FFTOutput applyEQ(const FFTOutput& input, const vector<double>& set_eq, const pair<vector<double>, double>& eq_settings) {
		auto& eq_frequencies = eq_settings.first;
		auto& frequencies = input.first;
		auto& q = eq_settings.second;
		
		FFTOutput fft_iterative = input;
		vector<double> final_eqs(fft_iterative.second.size(), 0);
		
		for (int i = 0; i < 1; i++) {
			//auto output = fitBands(fft_iterative, eq_settings);
			auto output = set_eq;
			vector<int> window_start_index;
			vector<vector<double>> windows;
			
			for (size_t k = 0; k < eq_frequencies.size(); k++) {
				auto frequency = eq_frequencies.at(k);
				
				double f0 = frequency;
				double ratio = (1.0 + sqrt(5.0)) / 2.0;
				double f_low = f0 * q / ratio;
				double f_high = f0 / q * ratio;
				
				// Get indicies
				auto f0_index = getClosestIndex(f0, frequencies);
				auto f_low_index = getClosestIndex(f_low, frequencies);
				auto f_high_index = getClosestIndex(f_high, frequencies);
				
				//double f_lowest = abs(output.at(k) / 3);
				
				double bandwidth_index = mean(vector<int>({ f0_index - f_low_index, f_high_index - f0_index }));
				int tap_size = lround(bandwidth_index * 4.0);
				
				auto sp_hamming = sp::hann(tap_size);
				vector<double> values;
				
				for (auto& value : sp_hamming)				
					values.push_back(value);
								
				bool is_negative = false;
				
				if (output.at(k) < 0)
					is_negative = true;
				
				for (size_t j = 0; j < values.size(); j++) {
					auto& value = values.at(j);
					
					double mult = value;
					double boost_db = output.at(k);
					double linear_value = pow(10, boost_db / 10);
					
					if (boost_db < 0)
						value = linear_value / value;
					else
						value *= linear_value;
					
					value = 10 * log10(value);
					
					if (value < 0 && !is_negative)
						value = 0;
					else if (value > 0 && is_negative)
						value = 0;	
					//cout << f0 << " value  "<< value << " mult " << mult << " linear " << linear_value <<  endl;
				}
				
				//cout << endl;
				
				windows.push_back(values);
				window_start_index.push_back(f_low_index);
			}
			
			vector<double> magnitudes_db(fft_iterative.second.size(), 0);
			
			for (size_t j = 0; j < windows.size(); j++) {
				auto& start = window_start_index.at(j);
				
				for (size_t k = 0; k < windows.at(j).size(); k++) {
					if (start + k >= magnitudes_db.size())
						break;
					
					//magnitudes_db.at(start + k) = //windows.
					//magnitudes_sign.at(start + k) = windows.at(j).second;
					//magnitudes_db.at(start + k) += windows.at(j).first.at(k);
					
					auto& original_db = magnitudes_db.at(start + k);
					auto& adding_db = windows.at(j).at(k);
					
					if (original_db * adding_db > 0) {
						// Both below zero, abs them and combine incoherent sound sources as negative
						double current = pow(10, abs(original_db) / 10);
						double adding = pow(10, abs(adding_db) / 10);
						
						original_db = 10 * log10(current + adding) * (original_db < 0 ? (-1) : 1);
					} else {
						original_db += adding_db;
					}
					
					//original_db += adding_db;
					
					#if 0
					if (magnitudes_set.at(start + k)) {
						double current = pow(10, magnitudes_db.at(start + k) / 10);
						double adding = pow(10, windows.at(j).first.at(k) / 10);
						
						magnitudes_db.at(start + k) = 10 * log10(current + adding);
					} else {
						magnitudes_db.at(start + k) = windows.at(j).first.at(k);
						magnitudes_set.at(start + k) = true;
					}
					#endif
					
					/*
					// is_negative
					if (windows.at(j).second)
						magnitudes_db.at(start + k) += windows.at(j).first.at(k);
					else
						magnitudes_db.at(start + k) -= windows.at(j).first.at(k);
					*/
				}
				
			}
			
			for (size_t j = 0; j < magnitudes_db.size(); j++) {
				fft_iterative.second.at(j) -= magnitudes_db.at(j);
				final_eqs.at(j) += magnitudes_db.at(j);
			}
			
			//fitBands(fft_iterative, eq_settings);
		}
		
		#if 0
		fft_iterative.second = final_eqs;
		fitBands(fft_iterative, eq_settings);
		
		return fitBands(input, eq_settings);
		#endif
		
		return fft_iterative;
	}
	#endif
	
	vector<double> getEQ(const FFTOutput& input, const pair<vector<double>, double>& eq_settings) {
		return fitBands(input, eq_settings, false);
		
		#if 0
		vector<double> current_eq(eq_settings.first.size(), 0);
		FFTOutput last;
		
		for (int i = 0; i < 10; i++) {
			for (size_t i = 0; i < current_eq.size(); i++) {
				cout << "Frequency\t" << eq_settings.first.at(i) << "\t:\t" << current_eq.at(i) << endl;
			}
			
			auto answer = applyEQ(input, current_eq, eq_settings);
			auto fitted = fitBands(answer, eq_settings);
			
			for (size_t j = 0; j < current_eq.size(); j++)
				current_eq.at(j) += fitted.at(j) / 10.0;
				
			auto db_std_dev = calculateSD(fitted);
			cout << "db_std_dev " << db_std_dev << endl;
			
			if (db_std_dev < 1e-04)
				break;
		}
		
		cout << "Original:\n";
		fitBands(input, eq_settings);
		cout << "Simulated:\n";
		
		for (size_t i = 0; i < current_eq.size(); i++) {
			cout << "Frequency\t" << eq_settings.first.at(i) << "\t:\t" << current_eq.at(i) << endl;
		}
		
		#if 0
		auto& eq_frequencies = eq_settings.first;
		auto& frequencies = input.first;
		auto& q = eq_settings.second;
		
		FFTOutput fft_iterative = input;
		vector<double> final_eqs(fft_iterative.second.size(), 0);
		
		for (int i = 0; i < 100; i++) {
			auto output = fitBands(fft_iterative, eq_settings);
			//vector<double> output(9, 6.0);
			
			vector<int> window_start_index;
			vector<vector<double>> windows;
			
			for (size_t k = 0; k < eq_frequencies.size(); k++) {
				auto frequency = eq_frequencies.at(k);
				
				double f0 = frequency;
				double ratio = (1.0 + sqrt(5.0)) / 2.0;
				double f_low = f0 * q / ratio;
				double f_high = f0 / q * ratio;
				
				// Get indicies
				auto f0_index = getClosestIndex(f0, frequencies);
				auto f_low_index = getClosestIndex(f_low, frequencies);
				auto f_high_index = getClosestIndex(f_high, frequencies);
				
				double bandwidth_index = mean(vector<int>({ f0_index - f_low_index, f_high_index - f0_index }));
				int tap_size = lround(bandwidth_index * 4.0);
				
				auto sp_hamming = sp::hamming(tap_size);
				vector<double> values;
				
				for (auto& value : sp_hamming) {					
					values.push_back(value);
				}
								
				bool is_negative = false;
				
				if (output.at(k) < 0)
					is_negative = true;
				
				for (size_t j = 0; j < values.size(); j++) {
					auto& value = values.at(j);
					
					double boost_db = output.at(k);
					double linear_value = pow(10, boost_db / 10);
					
					if (boost_db < 0)
						value = linear_value / value;
					else
						value *= linear_value;
					
					value = 10 * log10(value);
					
					if (value < 0 && !is_negative)
						value = 0;
					else if (value > 0 && is_negative)
						value = 0;
						
					//cout << "value  "<< value << endl;
				}
				
				windows.push_back(values);
				window_start_index.push_back(f_low_index);
			}
			
			vector<double> magnitudes_db(fft_iterative.second.size(), 0);
			
			for (size_t j = 0; j < windows.size(); j++) {
				auto& start = window_start_index.at(j);
				
				for (size_t k = 0; k < windows.at(j).size(); k++) {
					if (start + k >= magnitudes_db.size())
						break;
					
					//magnitudes_db.at(start + k) = //windows.
					//magnitudes_sign.at(start + k) = windows.at(j).second;
					//magnitudes_db.at(start + k) += windows.at(j).first.at(k);
					
					auto& original_db = magnitudes_db.at(start + k);
					auto& adding_db = windows.at(j).at(k);
					
					if (original_db * adding_db > 0) {
						// Both below zero, abs them and combine incoherent sound sources as negative
						double current = pow(10, abs(original_db) / 10);
						double adding = pow(10, abs(adding_db) / 10);
						
						original_db = 10 * log10(current + adding) * (original_db < 0 ? (-1) : 1);
					} else {
						original_db += adding_db;
					}
					
					#if 0
					if (magnitudes_set.at(start + k)) {
						double current = pow(10, magnitudes_db.at(start + k) / 10);
						double adding = pow(10, windows.at(j).first.at(k) / 10);
						
						magnitudes_db.at(start + k) = 10 * log10(current + adding);
					} else {
						magnitudes_db.at(start + k) = windows.at(j).first.at(k);
						magnitudes_set.at(start + k) = true;
					}
					#endif
					
					/*
					// is_negative
					if (windows.at(j).second)
						magnitudes_db.at(start + k) += windows.at(j).first.at(k);
					else
						magnitudes_db.at(start + k) -= windows.at(j).first.at(k);
					*/
				}
				
			}
			
			for (size_t j = 0; j < magnitudes_db.size(); j++) {
				fft_iterative.second.at(j) -= magnitudes_db.at(j);
				final_eqs.at(j) += magnitudes_db.at(j);
			}
			
			fitBands(fft_iterative, eq_settings);
		}
		
		fft_iterative.second = final_eqs;
		fitBands(fft_iterative, eq_settings);
		
		return fitBands(input, eq_settings);
		#endif
		#endif
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
			
			cout << "Added band limit " << lower << endl;
			cout << "Added band limit " << upper << endl;
		}
		
		#if 0
		// Set speaker profile limitations
		if (speaker_profile.getLowCutOff() > band_limits.front())
			band_limits.front() = speaker_profile.getLowCutOff();
			
		if (speaker_profile.getHighCutOff() < band_limits.back())
			band_limits.back() = speaker_profile.getHighCutOff();
		#endif
		
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