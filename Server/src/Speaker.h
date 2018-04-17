#ifndef SPEAKER_H
#define SPEAKER_H

#include <string>
#include <vector>
#include <array>

using MicFrequencyResponse = std::vector<std::pair<std::string, std::vector<double>>>;

enum {
	DSP_MAX_EQ = 12,
	DSP_MIN_EQ = -12,
	DSP_MAX_BANDS = 9
};

class Speaker {
public:
	// For placing speakers
	class SpeakerPlacement {
	public:
		explicit SpeakerPlacement();
		explicit SpeakerPlacement(const std::string& ip);
		
		void setCoordinates(const std::array<double, 3>& coordinates);
		void addDistance(const std::string& ip, double distance);
		
		const std::array<double, 3>& getCoordinates();
		const std::vector<std::pair<std::string, double>>& getDistances();
		const std::string& getIp();
		
	private:
		std::vector<std::pair<std::string, double>> distances_;
		std::array<double, 3> coordinates_;
		
		std::string ip_;
	};
	
	// Basic speaker setters
	void setIP(const std::string& ip);
	void setVolume(double volume);
	void setOnline(bool status);
	
	// EQ setters
	void clearAllEQs();
	void setNextEQ(const std::vector<double>& eq, double score);
	void setNextVolume();
	void setBestVolume();
	
	// Mic setters
	void setFrequencyResponseFrom(const std::string& ip, const std::vector<double>& dbs);
	
	// SpeakerPlacement setters
	void setPlacement(const SpeakerPlacement& placement, int placement_id);
	
	// Basic getters
	const std::string& getIP() const;
	double getVolume() const;
	bool isOnline() const;
	
	// EQ getters
	std::vector<double> getNextEQ();
	std::vector<double> getBestEQ();
	double getBestVolume() const;
	double getNextVolume() const;
	double getBestScore() const;
	double getLoudestBestEQ() const;
	
	// Mic getters
	std::vector<double> getFrequencyResponseFrom(const std::string& ip) const;
	
	// SpeakerPlacement getters
	SpeakerPlacement getPlacement();
	int getPlacementID() const;
	bool hasPlacement() const;
	
	bool operator==(const std::string& ip);
	
private:
	// EQ members
	std::vector<double> correction_eq_;
	std::vector<double> current_best_eq_;
	double score_				= 0;
	double best_speaker_volume_	= 0;
	double correction_volume_	= 0;
	
	// Information about frequency response from other speakers
	MicFrequencyResponse mic_frequency_responses_;
	
	// Basic members
	std::string ip_	= "not set";
	double volume_	= 0;
	bool online_	= false;

	// Placement members
	SpeakerPlacement placement_;
	int last_placement_id_	= -1;
};

#endif