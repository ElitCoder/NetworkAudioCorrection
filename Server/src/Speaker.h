#ifndef SPEAKER_H
#define SPEAKER_H

#include <string>
#include <vector>
#include <array>

enum {
	DSP_MAX_EQ = 12,
	DSP_MIN_EQ = -12
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
	
	void setIP(const std::string& ip);
	void setCorrectionEQ(std::vector<double> eq, double score);
	void setVolume(int volume);
	void setMicVolume(int volume);
	void setMicBoost(int boost);
	void setOnline(bool status);
	void setPlacement(const SpeakerPlacement& placement, int placement_id);
	void setFrequencyResponseFrom(const std::string& ip, const std::vector<double>& dbs);
	void clearAllEQs();
	void setBestVolume();
	void setCorrectionVolume();
	void setBandSensitive(int band_index, bool status);
	void setLastChange(const std::vector<double>& dbs, const std::vector<double>& correction);
	void resetLastEQChange(int band_index);
	void preventEQChange(int band_index);
	
	const std::string& getIP() const;
	int getPlacementID() const;
	SpeakerPlacement& getPlacement();
	bool isOnline() const;
	bool hasPlacement() const;
	std::vector<int> getCorrectionEQ();
	std::vector<int> getBestEQ();
	int getBestVolume() const;
	double getBestScore() const;
	std::vector<double> getFrequencyResponseFrom(const std::string& ip) const;
	int getCurrentVolume() const;
	int getCorrectionVolume() const;
	bool isFirstRun() const;
	bool isBandSensitive(int band_index) const;
	std::vector<double> getLastEQChange() const;
	bool isBlockedEQ(int band_index) const;
	
	std::pair<std::vector<double>, std::vector<double>> getLastChange() const;

	bool operator==(const std::string& ip);
	
private:
	// Correction EQ - make it sound better
	std::vector<double> correction_eq_;
	std::vector<double> current_best_eq_;
	double score_ = 0.0;
	int best_speaker_volume_ = 0;
	int correction_volume_ = 0;
	bool first_run_ = true;
	std::vector<bool> blocked_eq_ = std::vector<bool>(9, false);
	
	std::vector<double> last_eq_change_ = std::vector<double>(9, 0);
	std::vector<double> last_change_dbs_;
	std::vector<double> last_correction_;
	std::vector<bool> sensitive_band_ = std::vector<bool>(9, false);
	
	// Information about frequency response from other speakers
	std::vector<std::pair<std::string, std::vector<double>>> mic_frequency_responses_;
	
	std::string ip_	= "not set";
	int volume_		= 0;
	int mic_volume_	= 0;
	int mic_boost_	= 0;
	bool online_	= false;

	SpeakerPlacement placement_;
	int last_placement_id_	= -1;
};

#endif