#pragma once
#ifndef NAC_PROFILE_H
#define NAC_PROFILE_H

#include <vector>

// Just keep cut-off frequencies for now
class Profile {
public:
	void setCutoffs(double low, double high);
	void setSteep(double steep);
	
	double getLowCutOff() const;
	double getHighCutOff() const;
	double getSteep() const;
	
	std::pair<std::vector<double>, double> getSpeakerEQ() const;
	void setSpeakerEQ(const std::vector<double>& frequencies, double q);
	
	void setMaxEQ(double max);
	void setMinEQ(double min);
	
	double getMaxEQ() const;
	double getMinEQ() const;
	
	int getNumEQBands() const;
	
private:
	std::vector<double> eq_frequencies_;
	double eq_q_		= 1;
	
	// Points where -3 dB is met
	double low_cutoff_	= 0;
	double high_cutoff_	= 0;
	
	// Steep -12 dB per octave outside of cut-offs
	double steep_		= -96;
	
	double dsp_max_eq_	= 0;
	double dsp_min_eq_	= 0;
};
#endif