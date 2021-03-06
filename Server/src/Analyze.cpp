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
#include <set>

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

static set<int> g_ignore_bands;

static double correctMaxEQ(vector<double>& eq) {
	double total_mean_change = 0;
	auto min_eq = Base::system().getSpeakerProfile().getMinEQ();
	auto max_eq = Base::system().getSpeakerProfile().getMaxEQ();
	auto speaker_eq = Base::system().getSpeakerProfile().getSpeakerEQ();
	auto& speaker_eq_frequencies = speaker_eq.first;

	for (int i = 0; i < 1000; i++) {
		double mean_db = mean(eq);

		if (Base::config().get<bool>("normalize_spectrum")) {
			for (auto& setting : eq)
				setting -= mean_db;
		}

		if (Base::config().get<bool>("boost_max_zero")) {
			/* Move below 0 */
			auto max = *max_element(eq.begin(), eq.end());

			for (size_t j = 0; j < eq.size(); j++) {
				/* Ignore if it's in g_ignore_bands */
				if (g_ignore_bands.count(lround(speaker_eq_frequencies.at(j)))) {
					continue;
				}

				eq.at(j) -= max;
			}

#if 0
			for (auto& setting : eq)
				setting -= max;
#endif
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

/* From SO */
int RoundToMultiple(double toRound, int multiple)
{
    const auto ratio = toRound / multiple;
    const auto iratio = std::lround(ratio);
    return iratio * multiple;
}

/* From SO */
double interpolate( vector<double> &xData, vector<double> &yData, double x, bool extrapolate )
{
	int size = xData.size();

	int i = 0;                                                                  // find left end of interval for interpolation
	if ( x >= xData[size - 2] )                                                 // special case: beyond right end
	{
		i = size - 2;
	}
	else
	{
		while ( x > xData[i+1] ) i++;
	}
	double xL = xData[i], yL = yData[i], xR = xData[i+1], yR = yData[i+1];      // points on either side (unless beyond ends)
	if ( !extrapolate )                                                         // if beyond ends of array and not extrapolating
	{
		if ( x < xL ) yR = yL;
		if ( x > xR ) yL = yR;
	}

	double dydx = ( yR - yL ) / ( xR - xL );                                    // gradient

	return yL + dydx * ( x - xL );                                              // linear interpolation
}

static int g_f_low = -1;
static int g_f_high = -1;

namespace nac {
	FFTOutput doFFT(const vector<short>& samples, size_t start, size_t stop) {
		size_t max_size = (stop == 0 ? samples.size() : stop);
		vector<double> in;
		in.resize(max_size);

		// Normalize to [0, 1]
		#pragma omp parallel for
		for (size_t i = start; i < max_size; i++)
			in.at(i) = (double)samples.at(i) / (double)SHRT_MAX;

		// Assure FFT bin resolution is higher than the EQ resolution
		// Fs / N < lowest octave width
		// For example, 63 Hz with 1/1 gives 44 - 88 Hz = 44 Hz width

		auto eq = Base::system().getSpeakerProfile().getSpeakerEQ().first;
		// Always use FFT size of 48000 for now, since it gives 1 Hz resolution
		// 1 Hz resolution assures that adding additional curves applies correctly
		int N = 65536;

		cout << "Debug: set N to " << N << endl;

		arma::vec y = arma::abs(sp::pwelch(arma::vec(in), N, N / 2));

		vector<double> output;
		vector<double> frequencies;

		output.resize(y.size() / 2);
		frequencies.resize(y.size() / 2);

		// pwelch() output is [0, fs)
		#pragma omp parallel for
		for (size_t i = 0; i < y.size() / 2; i++) {
			double frequency = i * 48000.0 / N;

			frequencies.at(i) = frequency;
			output.at(i) = y.at(i);
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

	FFTOutput applyShelving(const FFTOutput& input) {
		const double SHELF_Q = 1 / sqrt(2); // 0.707
		double low_freq = Base::config().get<double>("low_shelf_freq");
		double low_gain = Base::config().get<double>("low_shelf_gain");
		double high_freq = Base::config().get<double>("high_shelf_freq");
		double high_gain = Base::config().get<double>("high_shelf_gain");

		cout << "Creating shelving with Q " << SHELF_Q << endl;

		// Create the actual IIR
		Filter low_shelf_filter(low_freq, SHELF_Q, LOW_SHELF);
		Filter high_shelf_filter(high_freq, SHELF_Q, HIGH_SHELF);
		low_shelf_filter.reset(low_gain, 48000);
		high_shelf_filter.reset(high_gain, 48000);

		// Ask for gains
		auto output = input;
		auto& freqs = output.first;
		auto& dbs = output.second;

		for (size_t i = 0; i < freqs.size(); i++) {
			dbs.at(i) -= low_shelf_filter.gainAt(freqs.at(i), 48000);
			dbs.at(i) -= high_shelf_filter.gainAt(freqs.at(i), 48000);
		}

		return output;
	}

	static FFTOutput getLoudness(const FFTOutput& input, double spl) {
		const double f[] = { 20, 25, 31.5, 40, 50, 63, 80, 100, 125, 160, 200, 250, 315, 400, 500, 630, 800, 1000, 1250, 1600, 2000, 2500, 3150, 4000, 5000, 6300, 8000, 10000, 12500 };
		const double af[] = { 0.532,0.506,0.480,0.455,0.432,0.409,0.387,0.367,0.349,0.330,0.315,0.301,0.288,0.276,0.267,0.259,0.253,0.250,0.246,0.244,0.243,0.243,0.243,0.242,0.242,0.245,0.254,0.271,0.301 };
		const double Lu[] = { -31.6,-27.2,-23.0,-19.1,-15.9,-13.0,-10.3,-8.1,-6.2,-4.5,-3.1,-2.0,-1.1,-0.4,0.0,0.3,0.5,0.0,-2.7,-4.1,-1.0,1.7,2.5,1.2,-2.1,-7.1,-11.2,-10.7,-3.1 };
		const double Tf[] = { 78.5,68.7,59.5,51.1,44.0,37.5,31.5,26.5,22.1,17.9,14.4,11.4,8.6,6.2,4.4,3.0,2.2,2.4,3.5,1.7,-1.3,-4.2,-6.0,-5.4,-1.5,6.0,12.6,13.9,12.3 };

		double Ln = spl;
		vector<double> freqs;

		for (size_t i = 0; i < sizeof(f) / sizeof(double); i++) {
			double Af = 4.47e-3 * (pow(10.0, 0.025 * Ln) - 1.15) +
						pow(0.4 * pow(10.0, (((Tf[i]+Lu[i])/10)-9)), af[i]);
			double Lp = (10 / af[i]) * log10(Af) - Lu[i] + 94;

			/* Remove offset */
			Lp -= spl;
			Lp = -Lp;

			cout << "For SPL " << spl << " and freq " << f[i] << " db " << Lp << endl;
			freqs.push_back(Lp);
		}

		/* Interpolate */
		FFTOutput output = input;
		auto& o_freqs = output.first;
		auto& dbs = output.second;

		vector<double> l_freqs(f, f + sizeof(f) / sizeof(double));

		for (size_t i = 0; i < o_freqs.size(); i++)
			dbs.at(i) = interpolate(l_freqs, freqs, o_freqs.at(i), false);

		return output;
	}

	FFTOutput applyLoudness(const FFTOutput& input, double monitor_spl, double playback_spl) {
		auto monitor = getLoudness(input, monitor_spl);
		auto playback = getLoudness(input, playback_spl);

		/* Resulting loudness curve is playback - monitor */
		auto resulting = input;

		for (size_t i = 0; i < monitor.first.size(); i++) {
			resulting.second.at(i) += playback.second.at(i) - monitor.second.at(i);
		}

		return resulting;
	}

	FFTOutput applyHC(const FFTOutput& input) {
		// TODO: Fix this. Currently we're just blindly adjusting to 10 dB tilt
#if 0
		// TODO: Parse this outside of this file
		auto name = Base::config().get<string>("hc_file");
		ifstream file(name);
		if (!file.is_open()) {
			cout << "WARNING: Could not open house curve file\n";
			return input;
		}

		string frequency;
		string gain;
		vector<pair<double, double>> points;
		while (!file.eof()) {
			getline(file, frequency);
			getline(file, gain);
			points.push_back({ stod(frequency), stod(gain) });
		}
#endif

		auto output = input;
		auto& freqs = output.first;
		auto& dbs = output.second;

		double k = (-5 - 5) / (log10(20000) - log10(20));
		double m = 5 - k * log10(20);

		for (size_t i = 0; i < freqs.size(); i++) {
			double y = k * log10(freqs.at(i)) + m;
			dbs.at(i) -= y;
		}

		return output;
	}

	FFTOutput applyProfiles(const FFTOutput& input, const Profile& speaker_profile, const Profile& microphone_profile) {
		auto low = max(speaker_profile.getLowCutOff(), microphone_profile.getLowCutOff());
		auto high = min(speaker_profile.getHighCutOff(), microphone_profile.getHighCutOff());
		auto steep_low = max(speaker_profile.getSteepLow(), microphone_profile.getSteepLow());
		auto steep_high = max(speaker_profile.getSteepHigh(), microphone_profile.getSteepHigh());
		double lowest = Base::config().get<double>("hardware_profile_cutoff_low_min_freq");
		double highest = Base::config().get<double>("hardware_profile_cutoff_high_max_freq");

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
				double steps = log2(low / frequency);

				/* Boost with same value as lowest below lowest */
				if (frequency < lowest) {
					steps = log2(low / lowest);
				}

				double attenuation = steps * steep_low;
				db += attenuation;
			} else if (frequency > high) {
				double steps = log2(frequency / high);

				/* Boost with same value as highest above highest */
				if (frequency > highest) {
					steps = log2(highest / high);
				}

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
				double actual_change = eq_change.at(i);

				if (Base::config().get<bool>("dsp_eq_mult_two")) {
					actual_change = RoundToMultiple(actual_change, 2);
				}

				gains.push_back({ speaker_eq_frequencies.at(i), actual_change });
				cout << actual_change << " ";
			}
			cout << endl;

			vector<short> simulated_samples;
			auto response = fft_output;

			if (Base::config().get<bool>("print_freq_response")) {
				// Only print once
				if (i == 0) {
					auto print_freq = response;

					// Normalize power spectrum if we're using pink noise
					if (!Base::config().get<bool>("is_white_noise")) {
						for (size_t j = 0; j < print_freq.first.size(); j++) {
							print_freq.second.at(j) *= print_freq.first.at(j);
						}
					}

					// Power -> dB
					print_freq = nac::toDecibel(print_freq);

					// Write to file
					ofstream file("freq_response.txt");
					for (size_t j = 0; j < print_freq.first.size(); j++) {
						// Only include [20, 20000]
						if (print_freq.first.at(j) < 20 || print_freq.first.at(j) > 20000) {
							continue;
						}

						file << print_freq.first.at(j) << ", " << print_freq.second.at(j) << endl;
					}
					file.close();
				}
			}

			if (Base::config().get<bool>("enable_fast_parametric")) {
				filter.apply(samples, simulated_samples, gains, 48000);
				response = nac::toDecibel(response);

				#pragma omp parallel for
				for (size_t x = 1; x < response.first.size(); x++) {
					response.second.at(x) += filter.gainAt(response.first.at(x), 48000);
				}

				response = nac::toLinear(response);
			} else {
				filter.apply(samples, simulated_samples, gains, 48000, true);

				// Find basic EQ change
				response = nac::doFFT(simulated_samples, start, stop);
			}

			cout << "Transformed to:\n";
			auto peer = nac::fitBands(response, speaker_eq, false, target_db == 0 ? -20000 : target_db);

			bool hardware_profile = Base::config().get<bool>("enable_hardware_profile");
			bool shelving_filters = Base::config().get<bool>("enable_shelving_filters");
			bool loudness = Base::config().get<bool>("enable_loudness_curve");
			bool house_curve = Base::config().get<bool>("enable_house_curve");

			if (hardware_profile || shelving_filters || loudness || house_curve) {
				/* Convert to dB */
				response = nac::toDecibel(response);
			}

			if (hardware_profile) {
				auto speaker_profile = Base::system().getSpeakerProfile().invert();
				auto mic_profile = Base::system().getMicrophoneProfile().invert();

				/* Imitate loudness curve */
				if (Base::config().get<bool>("hardware_profile_invert")) {
					speaker_profile = Base::system().getSpeakerProfile();
					mic_profile = Base::system().getMicrophoneProfile();
				}

				response = nac::applyProfiles(response, speaker_profile, mic_profile);
			}

			if (shelving_filters) {
				response = nac::applyShelving(response);
			}

			if (loudness) {
				double monitor_spl = Base::config().get<double>("loudness_curve_spl_monitor");
				double playback_spl = Base::config().get<double>("loudness_curve_spl_playback");
				response = nac::applyLoudness(response, monitor_spl, playback_spl);
			}

			if (house_curve) {
				response = nac::applyHC(response);
			}

			if (hardware_profile || shelving_filters || loudness || house_curve) {
				/* Convert back to linear */
				response = nac::toLinear(response);

				cout << "After target curve manipulations:\n";
				peer = nac::fitBands(response, speaker_eq, false, target_db == 0 ? -20000 : target_db);
			}

			auto& negative_curve = peer.first;
			auto& db_std_dev = peer.second;

			// Negate response to get change curve
			vector<double> eq;

			/* Set target_db to mean if it's the first run */
			if (i == 0/* || Base::config().get<bool>("boost_max_zero")*/) {
				double sum_target = 0;
				int num_target = 0;
				for (size_t i = 0; i < negative_curve.size(); i++) {
					if (speaker_eq_frequencies.at(i) > 500 && speaker_eq_frequencies.at(i) < 2000) {
						sum_target += negative_curve.at(i);
						num_target++;
					}
				}
				target_db = sum_target / num_target;//mean(negative_curve);
				cout << "Setting target DB to " << target_db << endl;
			}

			/* Calculate distance to target */
			for (size_t i = 0; i < negative_curve.size(); i++) {
				double target = target_db - negative_curve.at(i);
				eq.push_back(target);
			}

			if (db_std_dev < best_score) {
				/* Round to multiple of 2 if enabled */
				if (Base::config().get<bool>("dsp_eq_mult_two")) {
					best_eq.clear();

					for (auto& value : eq_change) {
						best_eq.push_back(RoundToMultiple(value, 2));
					}
				} else {
					best_eq = eq_change;
				}

				best_score = db_std_dev;
			}

			if (!best_eq.empty()) {
				auto extra_precision = Base::config().get<bool>("extra_precision");
				double precision = 0.1;

				if (extra_precision) {
					precision /= 100;
				}

				if (best_score < precision)
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

		if (Base::config().get<bool>("ignore_speaker_limitations")) {
			f_low = f_low < Base::config().get<double>("frequency_range_low") ? Base::config().get<double>("frequency_range_low") : f_low;
			f_high = f_high > Base::config().get<double>("frequency_range_high") ? Base::config().get<double>("frequency_range_high") : f_high;
		}

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
		vector<double> db_vec;
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
					cout << "IGNORING Frequency\t" << eq_frequencies.at(i) << "\t:\t" << energy.at(i) << endl;
					g_ignore_bands.insert(lround(eq_frequencies.at(i)));
					continue;
				}
				if (low < f_low && high > f_low) {
					if (high - f_low < 5) {
						if (target_db > -10000)
							energy.at(i) = target_db;
						else
							mean_band.push_back(i);
						cout << "IGNORING Frequency\t" << eq_frequencies.at(i) << "\t:\t" << energy.at(i) << endl;
						g_ignore_bands.insert(lround(eq_frequencies.at(i)));
						continue;
					}
					energy.at(i) *= (high - low) / (high - f_low);
				}

				if (high > f_high && low < f_high) {
					if (f_high - low < 5) {
						if (target_db > -10000)
							energy.at(i) = target_db;
						else
							mean_band.push_back(i);
						cout << "IGNORING Frequency\t" << eq_frequencies.at(i) << "\t:\t" << energy.at(i) << endl;
						g_ignore_bands.insert(lround(eq_frequencies.at(i)));
						continue;
					}
					energy.at(i) *= (high - low) / (f_high - low);
				}
			}

			// Convert to dB
			if (!input_db)
				energy.at(i) = 10 * log10(energy.at(i));

			tot += energy.at(i);
			db_vec.push_back(energy.at(i));
			nums++;

			cout << "Frequency\t" << eq_frequencies.at(i) << "\t:\t" << energy.at(i) << endl;
		}

		if (!mean_band.empty()) {
			for (auto mean : mean_band) {
				energy.at(mean) = tot / nums;
			}
		}

		auto db_std_dev = calculateSD(db_vec);
		cout << "db_std_dev " << db_std_dev << endl;

		return { energy, db_std_dev };
	}
}