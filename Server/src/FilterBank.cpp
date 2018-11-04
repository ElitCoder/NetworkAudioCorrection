#include "FilterBank.h"
#include "Base.h"
#include "Config.h"

#include <cmath>
#include <climits>
#include <algorithm>
#include <iostream>

using namespace std;

FilterBank::Filter::Filter(int frequency, double q, int type) {
	frequency_ = frequency;
	q_ = q;
	type_ = type;
}

void FilterBank::Filter::reset(double gain, int fs) {
	enabled_ = true;

	double w0 = 2.0 * M_PI * (double)frequency_ / (double)fs;
	double A;
	double alpha;
	double a0, a1, a2, b0, b1, b2;

	switch (type_) {
		case PARAMETRIC:
			A = pow(10, gain / 40);
			alpha = sin(w0) / (2 * A * q_);
			break;

		case BANDPASS:
			A = pow(10, gain / 20);
			alpha = sin(w0) * sinh(M_LN2 / 2 * (1.0 / Base::config().get<double>("dsp_eq_bw")) * w0 / sin(w0));
			break;

		default: cout << "ERROR: Filter type not specified";
			return;
	}

	if (type_ == PARAMETRIC) {
		a0 = 1 + alpha/A;
		a1 = -2 * cos(w0);
		a2 = 1 - alpha / A;
		b0 = (1 + alpha * A);
		b1 = -(2 * cos(w0));
		b2 = (1 - alpha * A);
	} else if (type_ == BANDPASS) {
		a0 = 1 + alpha;
		a1 = -2 * cos(w0);
		a2 = 1 - alpha;
		b0 = alpha;
		b1 = 0;
		b2 = -alpha;
	}

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

	for (size_t i = 0; i < in.size(); i++) {
		double x_0, x_1, x_2;
		double y_1, y_2;

		x_0 = in[i];

		if (i < 2) {
			x_2 = 0;
			y_2 = 0;

			if (i < 1) {
				x_1 = 0;
				y_1 = 0;
			} else {
				x_1 = in[i - 1];
				y_1 = out[i - 1];
			}
		} else {
			x_1 = in[i - 1];
			x_2 = in[i - 2];
			y_1 = out[i - 1];
			y_2 = out[i - 2];
		}

		double y = (b_[0] / a_[0]) * x_0 + (b_[1] / a_[0]) * x_1 + (b_[2] / a_[0]) * x_2
										 - (a_[1] / a_[0]) * y_1 - (a_[2] / a_[0]) * y_2;

		out.push_back(y);
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

int FilterBank::Filter::getType() const {
	return type_;
}

void FilterBank::addBand(int frequency, double q, int type) {
	filters_.emplace_back(frequency, q, type);
}

void FilterBank::initializeFiltering(const vector<short>& in, vector<double>& out, const vector<pair<int, double>>& gains, int fs) {
	for (auto& sample : in)
		out.push_back((double)sample / (double)SHRT_MAX);

	// Disable filters
	for (auto& filter : filters_)
		filter.disable();

	for (auto& gain : gains) {
		auto iterator = find(filters_.begin(), filters_.end(), gain.first);

		if (iterator != filters_.end())
			iterator->reset(gain.second, fs);
	}
}

void FilterBank::finalizeFiltering(const vector<double>& in, vector<short>& out) {
	// Convert to linear again
	for (auto& sample : in)
		out.push_back(lround(sample * (double)SHRT_MAX));
}

void FilterBank::apply(const vector<short>& samples, vector<short>& out, const vector<pair<int, double>>& gains, int fs) {
	vector<double> normalized;
	initializeFiltering(samples, normalized, gains, fs);

	/* Clear outgoing buffer */
	out.clear();

	/* All filters are of the same type */
	auto type = filters_.front().getType();

	if (type == PARAMETRIC) {
		/* Apply filters in cascade */
		for (auto& filter : filters_) {
			vector<double> filtered;
			filter.process(normalized, filtered);
			normalized = filtered;
		}
	} else if (type == BANDPASS) {
		/* Apply filters in parallel */
		vector<vector<double>> out_samples(filters_.size(), vector<double>());

		#pragma omp parallel for
		for (size_t i = 0; i < filters_.size(); i++) {
			filters_.at(i).process(normalized, out_samples.at(i));
		}

		/* Clear normalized */
		normalized = vector<double>(normalized.size(), 0);

		for (size_t j = 0; j < out_samples.size(); j++) {
			auto& filtered = out_samples.at(j);
			auto gain = gains.at(j).second;

			double linear_gain = pow(10.0, ((gain - 3.0) / 20.0));

			#pragma omp parallel for
			for (size_t i = 0; i < normalized.size(); i++) {
				normalized.at(i) += linear_gain * filtered.at(i);
			}
		}
	}

	finalizeFiltering(normalized, out);
}