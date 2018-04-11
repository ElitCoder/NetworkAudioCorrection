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

static short getRMS(const vector<short>& data, size_t start, size_t end) {
	unsigned long long sum = 0;
	
	for (size_t i = start; i < end; i++)
		sum += (data.at(i) * data.at(i));
		
	sum /= (end - start);
	
	return sqrt(sum);
}

void Recording::findStartingTones(int num_recordings, const int N, double threshold, double reducing, int frequency, int total_play_time_frames, int idle_time) {
	double dft;
	double orig_threshold = threshold;
	size_t current_i = idle_time * 48000 - 0.5 * (double)48000; // Should start at g_playingLength - 0.5 sec
	size_t frame_ending = ((double)idle_time - 0.5) * 48000 + total_play_time_frames;
	
	cout << "Checking first tone at " << current_i / (double)48000 << " to " << frame_ending / (double)48000 << "\n";
	
	// Check noise level
	short noise = getRMS(data_, 0, (1.5 * 48000));
	double noise_dft = goertzel(1.5 * 48000, frequency, 48000, data_.data()) / static_cast<double>(SHRT_MAX);
	
	cout << "Noise level: " << noise << endl;
	cout << "Noise goertzel: " << noise_dft << endl;
	
	for (int i = 0; i < num_recordings; i++) {
		threshold = orig_threshold;
		
		while (threshold > 0) {
			size_t delta_current = current_i;
			bool found = false;
			
			for (; delta_current < frame_ending; delta_current++) {
				dft = goertzel(N, frequency, 48000, data_.data() + delta_current) / static_cast<double>(SHRT_MAX);
				
				if (dft > threshold) {
					starting_tones_.push_back(delta_current);
					//current_i = (total_play_time_frames * (i + 2)) + 0.5 * (double)48000;
					//frame_ending = 48000 + (total_play_time_frames * (i + 2)) + 1.5 * (double)48000;
					current_i = ((double)idle_time - 0.5) * 48000 + total_play_time_frames * (i + 1);
					frame_ending = ((double)idle_time - 0.5) * 48000 + total_play_time_frames * (i + 2);
					
					//cout << "delta_current " << delta_current << endl;
					
					if (i + 1 != num_recordings)
						cout << "Found tone, checking " << current_i / (double)48000 << " to " << frame_ending / (double)48000 << " now\n";
					
					found = true;
					break;
				}
			}
			
			if (found)
				break;
				
			threshold -= reducing;	
		}
	}
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