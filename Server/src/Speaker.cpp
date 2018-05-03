#include "Speaker.h"
#include "Base.h"
#include "System.h"

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

void Speaker::setVolume(double volume) {
	volume_ = volume;
}

vector<double> Speaker::getNextEQ() {
	auto num_bands = Base::system().getSpeakerProfile().getNumEQBands();
	
	if (correction_eq_.empty())
		return vector<double>(num_bands, 0);
		
	return correction_eq_;
}

double Speaker::getLoudestBestEQ() const {
	if (current_best_eq_.empty())
		return 0;
	
	// Only check up to 1.5 kHz since the compressor/limiter won't bother with attenuating frequencies above that
	// Index 4 represents 2000 kHz
	return *max_element(current_best_eq_.begin(), current_best_eq_.begin() + 4);
	
	//return *max_element(current_best_eq_.begin(), current_best_eq_.end());
}

template<class T>
static T getMean(const vector<T>& container) {
	double sum = 0;
	
	for(const auto& element : container)
		sum += element;
		
	return sum / (double)container.size();
}

static double correctMaxEQ(vector<double>& eq) {
	double total_mean_change = 0;
	auto min_eq = Base::system().getSpeakerProfile().getMinEQ();
	auto max_eq = Base::system().getSpeakerProfile().getMaxEQ();
	
	for (int i = 0; i < 1000; i++) {
		double mean_db = getMean(eq);
		
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

void Speaker::addCustomerEQ(const vector<double>& eq) {
	for (size_t i = 0; i < eq.size(); i++)
		current_best_eq_.at(i) += eq.at(i);
		
	best_speaker_volume_ += correctMaxEQ(current_best_eq_);
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
	auto num_bands = Base::system().getSpeakerProfile().getNumEQBands();
	
	if (current_best_eq_.empty())
		return vector<double>(num_bands, 0);
		
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
	cout << "Speaker::setBestVolume() = " << best_speaker_volume_ << endl;
	
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
	
	auto num_bands = Base::system().getSpeakerProfile().getNumEQBands();
	
	if (correction_eq_.empty())
		correction_eq_ = vector<double>(num_bands, 0);
		
	for (size_t i = 0; i < eq.size(); i++)
		correction_eq_.at(i) += eq.at(i);
	
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