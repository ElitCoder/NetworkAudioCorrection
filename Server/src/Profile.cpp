#include "Profile.h"

using namespace std;

void Profile::setCutoffs(double low, double high) {
	low_cutoff_ = low;
	high_cutoff_ = high;
}

void Profile::setSteep(double low, double high) {
	steep_low_ = low;
	steep_high_ = high;
}

double Profile::getLowCutOff() const {
	return low_cutoff_;
}

double Profile::getHighCutOff() const {
	return high_cutoff_;
}

double Profile::getSteepLow() const {
	return steep_low_;
}

double Profile::getSteepHigh() const {
	return steep_high_;
}

void Profile::setSpeakerEQ(const vector<double>& frequencies, double q) {
	eq_frequencies_ = frequencies;
	eq_q_ = q;
}

pair<vector<double>, double> Profile::getSpeakerEQ() const {
	return { eq_frequencies_, eq_q_ };
}

void Profile::setMaxEQ(double max) {
	dsp_max_eq_ = max;
}

void Profile::setMinEQ(double min) {
	dsp_min_eq_ = min;
}

double Profile::getMaxEQ() const {
	return dsp_max_eq_;
}

double Profile::getMinEQ() const {
	return dsp_min_eq_;
}

int Profile::getNumEQBands() const {
	return eq_frequencies_.size();
}