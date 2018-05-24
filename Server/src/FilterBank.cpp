#include "FilterBank.h"

#include <cmath>
#include <climits>
#include <algorithm>
#include <iostream>

using namespace std;

/* Filter */

FilterBank::Filter::Filter(int frequency, double q) {
	frequency_ = frequency;
	q_ = q;
}

void FilterBank::Filter::reset(double gain, int fs) {
	enabled_ = true;
	
	double w0 = (2 * M_PI * (double)frequency_) / fs;
	double A = pow(10, (gain / 40));
	double alpha = sin(w0)/(2 * A * q_);
	
	double a0 = 1 + alpha/A;
    double a1 = -2 * cos(w0);
    double a2 = 1 - alpha / A;
    double b0 = (1 + alpha * A);
    double b1 = -(2 * cos(w0));
    double b2 = (1 - alpha * A);
	
	a_.clear();
	b_.clear();
	
	a_.push_back(a0);
	a_.push_back(a1);
	a_.push_back(a2);
	b_.push_back(b0);
	b_.push_back(b1);
	b_.push_back(b2);
}

void FilterBank::Filter::process(const vector<double>& in, vector<double>& out) {
	if (!enabled_)
		cout << "Warning: using filter which is not enabled\n";
	
	out.clear();
	
	for (int i = 0; i < (int)in.size(); i++) {
		double tmp = 0.0;
		
		for (int j = 0; j < (int)b_.size(); j++) {
			if (i - j < 0)
				continue;
				
			tmp += b_.at(j) * in.at(i - j);
		}
		
		for (int j = 1; j < (int)a_.size(); j++) {
			if (i - j < 0)
				continue;
				
			tmp -= a_.at(j) * out.at(i - j);
		}
		
		tmp /= a_.front();
		out.push_back(tmp);
	}
}

void FilterBank::Filter::disable() {
	enabled_ = false;
}

bool FilterBank::Filter::operator==(int frequency) {
	return frequency == frequency_;
}

int FilterBank::Filter::getFrequency() const {
	return frequency_;
}

/* FilterBank */

void FilterBank::addBand(int frequency, double q) {
	filters_.emplace_back(frequency, q);
}

void FilterBank::apply(const vector<short>& samples, vector<short>& out, const vector<pair<int, double>>& gains, int fs) {
	out.clear();
	
	// Convert to [0, 1] samples
	vector<double> normalized;
	
	for (auto& sample : samples)
		normalized.push_back((double)sample / (double)SHRT_MAX);
	
	// Disable filters
	for (auto& filter : filters_)
		filter.disable();
		
	for (auto& gain : gains) {
		auto iterator = find(filters_.begin(), filters_.end(), gain.first);
		
		if (iterator != filters_.end())
			iterator->reset(gain.second, fs);
	}
	
	for (auto& filter : filters_) {
		vector<double> filtered;
		filter.process(normalized, filtered);
		normalized = filtered;
	}
	
	// Convert to linear again
	for (auto& sample : normalized)
		out.push_back(lround(sample * (double)SHRT_MAX));
}