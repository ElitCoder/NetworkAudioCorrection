#pragma once
#ifndef NAC_PROFILE_H
#define NAC_PROFILE_H

#include "FilterBank.h"

#include <vector>

// Just keep cut-off frequencies for now
class Profile {
public:
	void setCutoffs(double low, double high);
	void setSteep(double low, double high);
	
	double getLowCutOff() const;
	double getHighCutOff() const;
	double getSteepLow() const;
	double getSteepHigh() const;
	
	std::pair<std::vector<double>, double> getSpeakerEQ() const;
	void setSpeakerEQ(const std::vector<double>& frequencies, double q);
	
	void setMaxEQ(double max);
	void setMinEQ(double min);
	
	double getMaxEQ() const;
	double getMinEQ() const;
	
	int getNumEQBands() const;
	
	Profile invert() const;
	
	int getFrequencyIndex(int frequency) const;
	
	FilterBank& getFilter();
	
private:
	std::vector<double> eq_frequencies_;
	double eq_q_		= 1;
	
	// Points where -3 dB is met
	double low_cutoff_	= 0;
	double high_cutoff_	= 0;
	
	double steep_low_	= -6;
	double steep_high_	= -6;
	
	double dsp_max_eq_	= 0;
	double dsp_min_eq_	= 0;
	
	FilterBank filter_;
};
#endif