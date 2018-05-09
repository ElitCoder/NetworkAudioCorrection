#pragma once
#ifndef RECORDING_H
#define RECORDING_H

#include <string>
#include <vector>

class Recording {
public:
	Recording(const std::string& ip, int id);
	
	std::vector<short>& getData();
	void addDistance(int id, double distance);
	void setDistance(int id, double distance);
	double getDistance(int id);
	int getId() const;
	
	void findStartingTones(int num_recordings, int play_time, int idle_time);
	size_t getTonePlayingWhen(int id) const;
	
	const std::string& getIp() const;
	
private:
	std::string ip_;
	int id_;
	std::vector<short> data_;
	std::vector<size_t> starting_tones_;
	std::vector<std::pair<int, double>> distances_;
};

#endif