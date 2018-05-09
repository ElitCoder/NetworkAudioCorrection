#include "Recording.h"
#include "Goertzel.h"

#include <iostream>
#include <sstream>
#include <climits>
#include <algorithm>
#include <cmath>

using namespace std;

Recording::Recording(const string& ip, int id) :
	ip_(ip) {
	id_ = id;
	
	cout << "Setting Recording ID to " << id_ << endl;
}

vector<short>& Recording::getData() {
	return data_;
}

void Recording::addDistance(int id, double distance) {
	distances_.push_back({ id, distance});
}

void Recording::setDistance(int id, double distance) {
	for (auto& pair : distances_)
		if (pair.first == id)
			pair.second = distance;
}

double Recording::getDistance(int id) {
	for (size_t i = 0; i < distances_.size(); i++)
		if (distances_.at(i).first == id)
			return distances_.at(i).second;
	
	cout << "Warning: could not find client\n";		
	return 0;		
}

short getRMS(const vector<short>& data, size_t start, size_t end) {
	unsigned long long sum = 0;
	
	for (size_t i = start; i < end; i++)
		sum += (data.at(i) * data.at(i));
		
	sum /= (end - start);
	
	return sqrt(sum);
}

void Recording::findStartingTones(int num_recordings, int play_time, int idle_time) {
	int frequency = 4000;
	
	// T = 1 / frequency
	// N = 2 * T * Fs
	int N = lround(2.0 * 1.0 / (double)frequency * 48000.0);
	
	cout << "Setting N to " << N << endl;
	
	// Calculate start and stop times
	vector<pair<size_t, size_t>> times;
	vector<size_t> results(num_recordings, 0);
	
	for (int i = 0; i < num_recordings; i++) {
		double start_sec = idle_time + i * (idle_time + play_time) - 0.5;
		double stop_sec = start_sec + play_time;
		
		times.push_back({ start_sec * 48000.0, stop_sec * 48000.0 });
	}
	
	// Check noise level
	auto noise = getRMS(data_, data_.size() - idle_time * 48000, data_.size());
	auto noise_dft = goertzel(idle_time * 48000, frequency, 48000, data_.data() + data_.size() - idle_time * 48000) / (double)SHRT_MAX;
	
	cout << id_ << " noise level: " << noise << endl;
	cout << id_ << " noise dft for " << frequency << " Hz: " << noise_dft << endl;
	
	double noise_peak = INT_MIN;
	
	// Find peak noise
	for (size_t i = data_.size() - idle_time * 48000; i < data_.size() - N; i++) {
		double dft = goertzel(N, frequency, 48000, data_.data() + i) / (double)SHRT_MAX;
		
		if (dft > noise_peak)
			noise_peak = dft;
	}
	
	cout << id_ << " noise peak: " << noise_peak << endl;
	
	// Find starting timestamps
	#pragma omp parallel for
	for (size_t i = 0; i < times.size(); i++) {
		auto start = times.at(i).first;
		auto stop = times.at(i).second;
		
		double sound_level_start = (double)start + 0.8 * 48000.0;
		double sound_level_stop = sound_level_start + (play_time / 2.0) * 48000.0;
		
		auto sound_level = goertzel(lround(sound_level_stop - sound_level_start), frequency, 48000, data_.data() + lround(sound_level_start)) / (double)SHRT_MAX;
		double threshold = sound_level * 0.1;
		
		if (noise_peak > threshold) {
			cout << "Warning: noise_peak > threshold, readjusting threshold\n";
			
			threshold = noise_peak * 2;
		}
		
		cout << id_ << " setting threshold " << threshold << " for " << i << endl;
		
		bool found = false;
		
		for (size_t j = start; j < stop; j++) {
			auto dft = goertzel(N, frequency, 48000, data_.data() + j) / (double)SHRT_MAX;
			
			if (dft < threshold)
				continue;
				
			results.at(i) = j;
			found = true;
			
			cout << id_ << " found tone " << i << " at " << (double)j / 48000.0 << endl;
			
			break;
		}
		
		if (!found)
			cout << "WARNING: " << id_ << " did not find tone for " << i << endl;
	}
	
	starting_tones_ = results;
}

size_t Recording::getTonePlayingWhen(int id) const {
	if (static_cast<unsigned int>(id) >= starting_tones_.size()) {
		cout << "Warning: tone detection failed, id " << id << " size " << starting_tones_.size() << "\n";
		
		return 0;
	}
		
	return starting_tones_.at(id);
}

int Recording::getId() const {
	return id_;
}

const string& Recording::getIp() const {
	return ip_;
}