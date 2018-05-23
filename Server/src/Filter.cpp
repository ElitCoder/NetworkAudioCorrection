#include "Filter.h"

#include <cmath>
#include <iostream>

using namespace std;

void Filter::apply(const vector<short>& samples, vector<short>& out, const vector<double>& gains, int fs) {
	out = samples;
	
	// TODO: Write this as applying the filter and summing up instead
	for (size_t i = 0; i < frequencies_.size(); i++)
		applyBand(out, i, gains.at(i), fs);
}

void Filter::addBand(int frequency, double q) {
	frequencies_.push_back(frequency);
	qs_.push_back(q);
}

void Filter::applyBand(vector<short>& samples, int band, double gain, int fs) {	
	double normalize;
	double b1;
	double b2;
	double a0;
	double a1;
	double a2;
	
	double Q = qs_.at(band);
	double V = pow(10.0, fabs(gain) / 20.0);
    double K = tan(M_PI * frequencies_.at(band) / (double)fs);
	
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
	
	vector<short> out;
	out.reserve(samples.size());
	
	double z1 = 0.0;
	double z2 = 0.0;
	
	for (size_t i = 0; i < samples.size(); i++) {		
		double y = samples.at(i) * a0 + z1;
		
		z1 = samples.at(i) * a1 + z2 - b1 * y;
    	z2 = samples.at(i) * a2 - b2 * y;
		
		out.push_back(lround(y));
	}
	
	samples = out;
}