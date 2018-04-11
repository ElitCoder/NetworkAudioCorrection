#include "Speaker.h"

#include <algorithm>
#include <iostream>
#include <cmath>

using namespace std;

const string& Speaker::getIP() const {
	return ip_;
}

void Speaker::setOnline(bool status) {
	online_ = status;
}

bool Speaker::operator==(const string& ip) {
	return ip_ == ip;
}

void Speaker::setIP(const string& ip) {
	ip_ = ip;
}

bool Speaker::isOnline() const {
	return online_;
}

void Speaker::setPlacement(const SpeakerPlacement& placement, int placement_id) {
	placement_ = placement;
	last_placement_id_ = placement_id;
}

bool Speaker::hasPlacement() const {
	return last_placement_id_ >= 0;
}

int Speaker::getPlacementID() const {
	return last_placement_id_;
}

Speaker::SpeakerPlacement Speaker::getPlacement() {
	return placement_;
}

void Speaker::setVolume(int volume) {
	volume_ = volume;
}

vector<double> Speaker::getNextEQ() {
	return correction_eq_;
}

template<class T>
static T getMean(const vector<T>& container) {
	double sum = 0;
	
	for(const auto& element : container)
		sum += element;
		
	return sum / container.size();
}

static double correctMaxEQ(vector<double>& eq) {
	double mean_db = getMean(eq);
	
	for (auto& setting : eq)
		setting -= mean_db;
	
	for (auto& setting : eq) {
		if (setting < DSP_MIN_EQ)
			setting = DSP_MIN_EQ;
		else if (setting > DSP_MAX_EQ)
			setting = DSP_MAX_EQ;
	}
	
	return mean_db;
}

static void printEQ(const string& ip, const vector<double>& eq, const string& name) {
	cout << "EQ (" << ip << " : " << name << "): ";
	
	for (size_t i = 0; i < eq.size(); i++) {
		cout << eq.at(i);
		
		if (i + 1 < eq.size())
			cout << ",";
	}
		
	cout << "\n";
}

double Speaker::getBestScore() const {
	return score_;
}

vector<double> Speaker::getBestEQ() {
	return current_best_eq_;
}

void Speaker::clearAllEQs() {
	correction_eq_.clear();
	current_best_eq_.clear();
	correction_volume_ = volume_;
	score_ = 0;
	best_speaker_volume_ = volume_;
	mic_frequency_responses_.clear();
}

double Speaker::getBestVolume() const {
	return best_speaker_volume_;
}

void Speaker::setBestVolume() {
	volume_ = best_speaker_volume_;
}

void Speaker::setNextVolume() {
	volume_ = correction_volume_;
}

double Speaker::getNextVolume() const {
	return correction_volume_;
}

template<class T>
static vector<T> vectorDifference(const vector<T>& first, const vector<T>& second) {
	if (first.size() != second.size()) {
		cout << "WARNING: vectorDifference failed due to not same size vectors\n";
		
		return vector<T>();
	}
		
	vector<T> difference;
	
	for (size_t i = 0; i < first.size(); i++)
		difference.push_back(first.at(i) - second.at(i));
		
	return difference;
}

// Returns current EQ
void Speaker::setNextEQ(const vector<double>& eq, double score) {
	printEQ(ip_, correction_eq_, "current");
	printEQ(ip_, eq, "input");
	
	// Only update best EQ if the score is better
	if (score > score_) {
		current_best_eq_ = correction_eq_;
		score_ = score;
		best_speaker_volume_ = correction_volume_;
	}
	
	if (correction_eq_.empty()) {
		correction_eq_ = eq;
	} else {
		for (size_t i = 0; i < eq.size(); i++)
			correction_eq_.at(i) += eq.at(i);
	}
	
	correction_volume_ = volume_ + correctMaxEQ(correction_eq_);
	
	printEQ(ip_, correction_eq_, "next");
	
	cout << "Old volume: " << volume_ << endl;
	cout << "New volume: " << correction_volume_ << endl;
}

double Speaker::getVolume() const {
	return volume_;
}

void Speaker::setFrequencyResponseFrom(const string& ip, const vector<double>& dbs) {
	auto iterator = find_if(mic_frequency_responses_.begin(), mic_frequency_responses_.end(), [&ip] (auto& peer) {
		return peer.first == ip;
	});
	
	if (iterator == mic_frequency_responses_.end())
		mic_frequency_responses_.push_back({ ip, dbs });
	else
		(*iterator) = { ip, dbs };
		
	cout << "Frequency response from (" << ip << ") to microphone (" << ip_ << "):\t ";
	for_each(dbs.begin(), dbs.end(), [] (auto& value) { cout << value << " "; });
	cout << endl;
}

vector<double> Speaker::getFrequencyResponseFrom(const string& ip) const {
	auto iterator = find_if(mic_frequency_responses_.begin(), mic_frequency_responses_.end(), [&ip] (auto& peer) {
		return peer.first == ip;
	});
	
	if (iterator == mic_frequency_responses_.end())
		return vector<double>();
	else
		return iterator->second;
}

/*
	SpeakerPlacement
*/

Speaker::SpeakerPlacement::SpeakerPlacement() {}

Speaker::SpeakerPlacement::SpeakerPlacement(const string& ip) {
	ip_ = ip;
}

void Speaker::SpeakerPlacement::setCoordinates(const array<double, 3>& coordinates) {
	coordinates_ = coordinates;
}

void Speaker::SpeakerPlacement::addDistance(const string& ip, double distance) {
	auto iterator = find_if(distances_.begin(), distances_.end(), [&ip] (const pair<string, double>& peer) { return peer.first == ip; });
	
	if (iterator == distances_.end())
		distances_.push_back({ ip, distance });
	else
		(*iterator) = { ip, distance };
}

const array<double, 3>& Speaker::SpeakerPlacement::getCoordinates() {
	return coordinates_;
}

const vector<pair<string, double>>& Speaker::SpeakerPlacement::getDistances() {
	return distances_;
}

const string& Speaker::SpeakerPlacement::getIp() {
	return ip_;
}