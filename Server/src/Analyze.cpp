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

static vector<size_t> getBandIndex(double frequency, const vector<double>& limits) {
	vector<size_t> indicies;

	for (size_t i = 0; i < limits.size(); i += 2) {
		auto low = limits.at(i);
		auto high = limits.at(i + 1);

		if (limits.size() > (i + 2))
			high = limits.at(i + 2);

		if (frequency >= low && frequency < high)
			indicies.push_back(i / 2);
	}

	return indicies;
}

static double correctMaxEQ(vector<double>& eq) {
	double total_mean_change = 0;
	auto min_eq = Base::system().getSpeakerProfile().getMinEQ();
	auto max_eq = Base::system().getSpeakerProfile().getMaxEQ();

	for (int i = 0; i < 1000; i++) {
		double mean_db = mean(eq);

		if (Base::config().get<bool>("normalize_spectrum")) {
			for (auto& setting : eq)
				setting -= mean_db;
		}

		if (Base::config().get<bool>("boost_max_zero")) {
			/* Move below 0 */
			auto max = *max_element(eq.begin(), eq.end());

			for (auto& setting : eq)
				setting -= max;
		}

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

static int findBestFFTSize(double fs, double band_width) {
	// Start number
	int N = 65536;

	while (fs / (double)N > band_width * 2)
		N *= 2;

	return N;
}

static int g_f_low = -1;
static int g_f_high = -1;

namespace nac {
	FFTOutput doFFT(const vector<short>& samples, size_t start, size_t stop) {
		vector<double> in;

		// Normalize to [0, 1]
		for (size_t i = start; i < (stop == 0 ? samples.size() : stop); i++)
			in.push_back((double)samples.at(i) / (double)SHRT_MAX);

		// Assure FFT bin resolution is higher than the EQ resolution
		// Fs / N < lowest octave width
		// For example, 63 Hz with 1/1 gives 44 - 88 Hz = 44 Hz width

		auto eq = Base::system().getSpeakerProfile().getSpeakerEQ().first;
		int N = 8192;

		if (!eq.empty()) {
			double width = pow(2.0, 1.0 / (2.0 * Base::config().get<double>("dsp_octave_width")));
			double lower = eq.front() / width;
			double upper = eq.front() * width;

			N = findBestFFTSize(48000, upper - lower);
		}

		cout << "Debug: set N to " << N << endl;

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

		//cout << "low " << low << endl;
		//cout << "high " << high << endl;
		//cout << "steep_low " << steep_low << endl;
		//cout << "steep_high " << steep_high << endl;

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
				//cout << "Attenuating frequency " << frequency << endl;

				double steps = log2(low / frequency);
				double attenuation = steps * steep_low;

				db += attenuation;

				//cout << "With " << attenuation << " since steps is " << steps << endl;
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

	vector<double> findSimulatedEQSettings(const vector<short>& samples, FilterBank filter, size_t start, size_t stop) {
		g_f_low = -1;
		g_f_high = -1;

		auto speaker_eq = Base::system().getSpeakerProfile().getSpeakerEQ();
		auto& speaker_eq_frequencies = speaker_eq.first;

		vector<double> eq_change(speaker_eq_frequencies.size(), 0);
		vector<double> best_eq;
		double best_score = INT_MAX;
		double target_db = 0;

		auto fft_output = nac::doFFT(samples, start, stop);

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
			filter.apply(samples, simulated_samples, gains, 48000, true);

			// Find basic EQ change
			auto response = nac::doFFT(simulated_samples, start, stop);
#if 0
			filter.apply(samples, simulated_samples, gains, 48000);
			auto response = fft_output;
			response = nac::toDecibel(response);

			for (size_t x = 1; x < response.first.size(); x++) {
				response.second.at(x) += filter.gainAt(response.first.at(x), 48000);
			}
			response = nac::toLinear(response);
#endif
			cout << "Transformed to:\n";
			auto peer = nac::fitBands(response, speaker_eq, false, target_db == 0 ? -20000 : target_db);

			if (Base::config().get<bool>("enable_hardware_profile")) {
				response = nac::toDecibel(response);

				auto speaker_profile = Base::system().getSpeakerProfile().invert();
				auto mic_profile = Base::system().getMicrophoneProfile().invert();

				response = nac::applyProfiles(response, speaker_profile, mic_profile);

				// Revert back to energy
				response = nac::toLinear(response);

				cout << "After hardware profile:\n";
				peer = nac::fitBands(response, speaker_eq, false, target_db == 0 ? -20000 : target_db);
			}

			auto& negative_curve = peer.first;
			auto& db_std_dev = peer.second;

			// Negate response to get change curve
			vector<double> eq;

			/* Set target_db to mean if it's the first run */
			if (i == 0 || Base::config().get<bool>("boost_max_zero")) {
				target_db = mean(negative_curve);
				cout << "Setting target DB to " << target_db << endl;
			}

			/* Calculate distance to target */
			for (size_t i = 0; i < negative_curve.size(); i++) {
				double target = target_db - negative_curve.at(i);
				eq.push_back(target);
			}

			if (db_std_dev < best_score) {
				best_score = db_std_dev;
				best_eq = eq_change;
			}

			if (!best_eq.empty()) {
				if (best_score < 0.1)
					break;
			}

			cout << "Adding EQ: ";
			for (size_t i = 0; i < eq.size(); i++) {
				// Small changes for many bands
				eq_change.at(i) += eq.at(i) / Base::config().get<double>("simulation_slowdown");

				cout << eq.at(i) << " ";
			}
			cout << endl;

			cout << endl;
		}

		return best_eq;
	}

	pair<vector<double>, double> fitBands(const FFTOutput& input, const pair<vector<double>, double>& eq_settings, bool input_db, double target_db) {
		auto& eq_frequencies = eq_settings.first;

		// Calculate band limits
		vector<double> band_limits;

		for (auto& centre : eq_frequencies) {
			double width = pow(2.0, 1.0 / (2.0 * Base::config().get<double>("dsp_octave_width")));
			double lower = centre / width;
			double upper = centre * width;

			cout << "Calculated width " << width << " with lower " << lower << " and upper " << upper << endl;

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

		auto new_db = actual.second;
		for (size_t i = 0; i < dbs.size(); i++) {
			new_db.at(i) = dbs.at(i) * frequencies.at(i);
		}

		double avg_energy = accumulate(new_db.begin(), new_db.end(), 0.0) / new_db.size();
		double f_low = -1;
		double f_high = -1;

		if (Base::config().get<bool>("ignore_speaker_limitations"))
			avg_energy = 0;
		else
			avg_energy *= Base::config().get<double>("speaker_limitations_factor");

		for (size_t i = 0; i < dbs.size(); i++) {
			auto& frequency = frequencies.at(i);
			auto& db = new_db.at(i);

			if (frequency < 20)
				continue;

			if (frequency < 35)
				cout << "energy " << frequency << " " << db << " avg " << avg_energy << endl;

			if (db >= avg_energy) {
				if (f_low < 0)
					f_low = frequency;
			}
		}

		for (int i = dbs.size() - 1; i > 0; i--) {
			auto& frequency = frequencies.at(i);
			auto& db = new_db.at(i);

			if (frequency > 20000)
				continue;

			if (db >= avg_energy) {
				if (f_high < 0)
					f_high = frequency;
			}
		}

		f_low = f_low < Base::config().get<double>("frequency_range_low") ? Base::config().get<double>("frequency_range_low") : f_low;
		f_high = f_high > Base::config().get<double>("frequency_range_high") ? Base::config().get<double>("frequency_range_high") : f_high;

		if (g_f_low < 0)
			g_f_low = f_low;
		else
			f_low = g_f_low;
		if (g_f_high < 0)
			g_f_high = f_high;
		else
			f_high = g_f_high;
		cout << "f_low " << f_low << " f_high " << f_high << endl;

		for (size_t i = 0; i < dbs.size(); i++) {
			auto& frequency = frequencies.at(i);

			if (frequency < f_low || frequency > f_high)
				continue;

			auto index = getBandIndex(frequency, band_limits);

			//cout << endl;
			for (auto& j : index) {
				energy.at(j) += dbs.at(i);
				num.at(j)++;

				//cout << "INDEX " << j << endl;
			}
			//cout << endl;

#if 0
			// This frequency is outside band range
			if (index.empty())
				continue;


			energy.at(index) += dbs.at(i);
			num.at(index)++;
#endif
		}

		cout << "Lower resolution to fit EQ band with size " << eq_frequencies.size() << endl;

		vector<int> mean_band;
		double tot = 0;
		int nums = 0;

		for (size_t i = 0; i < num.size(); i++) {
			auto low = band_limits.at(i * 2);
			auto high = band_limits.at(i * 2 + 1);
			if (Base::config().get<bool>("is_white_noise")) {
				// Divide with the octave width

				energy.at(i) /= (high - low);
			} else {
				if ((low < f_low && high < f_low) || (high > f_high && low > f_high)) {
					// Set to mean later to avoid boosting
					if (target_db > -10000)
						energy.at(i) = target_db;
					else
						mean_band.push_back(i);
					continue;
				}
				if (low < f_low && high > f_low) {
					energy.at(i) *= (high - low) / (high - f_low);
				}

				if (high > f_high && low < f_high) {
					energy.at(i) *= (high - low) / (f_high - low);
				}
			}

			// Convert to dB
			if (!input_db)
				energy.at(i) = 10 * log10(energy.at(i));

			tot += energy.at(i);
			nums++;

			cout << "Frequency\t" << eq_frequencies.at(i) << "\t:\t" << energy.at(i) << endl;
		}

		if (!mean_band.empty()) {
			for (auto mean : mean_band) {
				energy.at(mean) = tot / nums;
			}
		}

		auto db_std_dev = calculateSD(energy);
		cout << "db_std_dev " << db_std_dev << endl;

		return { energy, db_std_dev };
	}
}