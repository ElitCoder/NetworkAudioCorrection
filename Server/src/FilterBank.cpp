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
	
	//cout << "Added filter with frequency " << frequency << " and Q " << q_ << endl;
}

void FilterBank::Filter::reset(double gain, int fs) {
	//cout << "Calculating with f0 " << frequency_ << " Q " << q_ << " gain " << gain << " fs " << fs << endl;
	enabled_ = true;
	
	double w0, gain_linear, A, alpha, a0;
    w0 = (2 * M_PI * (double)frequency_) / fs;
    gain_linear = pow(10, (0 / 20));

    A = pow(10, (gain / 40));
    alpha = sin(w0)/(2 * A * q_);

    /* [0] = b0, [1] = b1, [2] = b2, [3] = a1, [4] = a2 */
    a0 = 1 + alpha/A;
    double a1 = -2 * cos(w0);
    double a2 = 1 - alpha / A;
    double b0 = (1 + alpha * A);// * gain_linear;
    double b1 = -(2 * cos(w0));// * gain_linear;
    double b2 = (1 - alpha * A);// * gain_linear;
	
	/*
	a1 /= a0;
	a2 /= a0;
	b0 /= a0;
	b1 /= a0;
	b2 /= a0;
	*/
	
	/*
	normalize(coeffs, a0);
	coeffs->a1 = coeffs->a1/a0;
    coeffs->a2 = coeffs->a2/a0;
    coeffs->b0 = coeffs->b0/a0;
    coeffs->b1 = coeffs->b1/a0;
    coeffs->b2 = coeffs->b2/a0;
	*/

    /* invert */
    //a1 = -a1;
    //a2 = -a2;
	
	a_.clear();
	b_.clear();
	
	a_.push_back(a0);
	a_.push_back(a1);
	a_.push_back(a2);
	b_.push_back(b0);
	b_.push_back(b1);
	b_.push_back(b2);
	
	#if 0
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
	#endif
}

void FilterBank::Filter::process(const vector<double>& in, vector<double>& out) {
	if (!enabled_)
		cout << "Warning: using filter which is not enabled\n";
	
	out.clear();
	
	#if 0
	cout << "a0 " << a_.at(0) << endl;
	cout << "a1 " << a_.at(1) << endl;
	cout << "a2 " << a_.at(2) << endl;
	cout << "b0 " << b_.at(0) << endl;
	cout << "b1 " << b_.at(1) << endl;
	cout << "b2 " << a_.at(2) << endl;
	
	double last = 0.0;
	double last_last = 0.0;
	
	for (size_t i = 0; i < in.size(); i++) {
		double y =	b_.at(0) * in.at(i);
		
		if (i > 0)
			y +=	b_.at(1) * in.at(i - 1);
			
		if (i > 1)
			y +=	b_.at(2) * in.at(i - 2);
			
		y -= 		a_.at(1) * last;
		y -=		a_.at(2) * last_last;
		
		y /= a_.at(0);
		
		last_last = last;
		last = y;
		
		out.push_back(y);
		
		//if (i < 10)
		//	cout << "value " << y << endl;
	}
	#endif
	
	for (int i = 0; i < (int)in.size(); i++) {
		double tmp = 0.0;
		//out.at(i) = 0.0;
		
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
		//out.at(i) = tmp;
		out.push_back(tmp);
	}
	
	#if 0
	double y = sample * a0 + z1;
	
	z1 = sample * a1 + z2 - b1 * y;
	z2 = sample * a2 - b2 * y;
	
	return y;
	#endif
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
	
	//cout << "samples " << samples.size() << endl;
	// Convert to [0, 1] samples
	vector<double> normalized;
	//normalized.resize(samples.size());
	
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
		//cout << "Applying filter " << filter.getFrequency() << endl;
		//cout << "Last before: " << normalized.at(normalized.size() / 2) << " " << samples.at(normalized.size() / 2) << endl;
		//cout << "Size " << normalized.size() << endl;
		
		vector<double> filtered;
		filter.process(normalized, filtered);
		normalized = filtered;
		
		//cout << "Last after: " << normalized.at(normalized.size() / 2) << " " << samples.at(normalized.size() / 2) << endl;
		//cout << "Size " << normalized.size() << endl;
	}
	
	// Convert to linear again
	for (auto& sample : normalized)
		out.push_back(lround(sample * (double)SHRT_MAX));
	
	#if 0
	for (auto& sample : samples) {
		// Apply all filters
		double normalized = (double)sample / (double)SHRT_MAX;
		
		for (auto& filter : filters_)
			normalized = filter.process(normalized);
			
		out.push_back(lround(normalized * (double)SHRT_MAX));
	}
	#endif
}