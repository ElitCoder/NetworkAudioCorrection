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
	
	double normalize;
	
	double Q = q_;
	double V = pow(10.0, fabs(gain) / 20.0); // THIS WAS 20.0
    double K = tan(M_PI * (double)frequency_ / (double)fs);
	
	if (gain >= 0) {
        normalize = 1 / (1 + 1 / Q * K + K * K);
        a0 = (1 + V / Q * K + K * K) * normalize;
        a1 = 2 * (K * K - 1) * normalize;
        a2 = (1 - V / Q * K + K * K) * normalize;
        b1 = a1;
        b2 = (1 - 1 / Q * K + K * K) * normalize;
    } else {
        normalize = 1 / (1 + V / Q * K + K * K);
        a0 = (1 + 1 / Q * K + K * K) * normalize;
        a1 = 2 * (K * K - 1) * normalize;
        a2 = (1 - 1 / Q * K + K * K) * normalize;
        b1 = a1;
        b2 = (1 - V / Q * K + K * K) * normalize;
    }
	
	z1 = 0.0;
	z2 = 0.0;
}

double FilterBank::Filter::process(double sample) {
	if (!enabled_)
		cout << "Warning: using filter which is not enabled\n";
		
	double y = sample * a0 + z1;
	
	z1 = sample * a1 + z2 - b1 * y;
	z2 = sample * a2 - b2 * y;
	
	return y;
}

void FilterBank::Filter::disable() {
	enabled_ = false;
}

bool FilterBank::Filter::operator==(int frequency) {
	return frequency == frequency_;
}

/* FilterBank */

void FilterBank::addBand(int frequency, double q) {
	filters_.emplace_back(frequency, q);
}

void FilterBank::apply(const vector<short>& samples, vector<short>& out, const vector<pair<int, double>>& gains, int fs) {
	out.clear();
	
	// Disable filters
	for (auto& filter : filters_)
		filter.disable();
		
	for (auto& gain : gains) {
		auto iterator = find(filters_.begin(), filters_.end(), gain.first);
		
		if (iterator != filters_.end())
			iterator->reset(gain.second, fs);
	}
	
	for (auto& sample : samples) {
		// Apply all filters
		double normalized = (double)sample / (double)SHRT_MAX;
		
		for (auto& filter : filters_)
			normalized = filter.process(normalized);
			
		out.push_back(lround(normalized * (double)SHRT_MAX));
	}
}