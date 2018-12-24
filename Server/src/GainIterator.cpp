/*
    This file is part of EqualizerAPO, a system-wide equalizer.
    Copyright (C) 2015  Jonas Thedering

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along
    with this program; if not, write to the Free Software Foundation, Inc.,
    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

#include <algorithm>
#include <cmath>
#include <iostream>
#include "Base.h"
#include "Config.h"

#include "GainIterator.h"

using namespace std;

GainIterator::GainIterator(const vector<FilterNode>& nodes)
{
	this->nodes = nodes;
	nodeLeft = NULL;
	nodeRight = NULL;
}

static double gainForBand(double band_freq, double freq, double bw, double gain)
{
	gain -= 0;

	double lower_limit = band_freq * pow(2, -0.5 / bw);
	double upper_limit = band_freq * pow(2, 0.5 / bw);
	double zero_lower_limit = band_freq * pow(2, -0.3 / bw);
	double zero_upper_limit = band_freq * pow(2, 0.3 / bw);
	double gh = 0 + gain;// + gain;
	double gl = -6 + gain;// + gain;

	if (freq < band_freq) {
		//double k = gain / (band_freq - lower_limit);
		//double m = -(gain / (band_freq - lower_limit)) * lower_limit;
		double k = (gh - gl) / (pow(log10(band_freq), 2) - pow(log10(lower_limit), 2));
		double m = gh - k * pow(log10(band_freq), 2);

		return k * pow(log10(freq), 2) + m;
	} else if (freq == band_freq) {
		return gh;
	} else {
		double k = (gh - gl) / (pow(log10(band_freq), 2) - pow(log10(upper_limit), 2));
		double m = gh - k * pow(log10(band_freq), 2);

		return k * pow(log10(freq), 2) + m;
	}

#if 0
	double bw_change = pow(10, abs(gain) / 20);
	//double total_change = 4 / bw_change;
	double total_change = 2;
	double lower_limit = band_freq * pow(2, -total_change / bw);
	double upper_limit = band_freq * pow(2, total_change / bw);

	if ((freq > 200 && freq < 250) || (freq > 1000 && freq < 1050))
		cout << "BEFORE upper " << upper_limit << " lower " << lower_limit << " freq " << freq << " band_freq " << band_freq << " gain " << gain << endl;

#if 0
	switch (lround(abs(gain))) {
		case 10: gain = gain > 0 ? 7.5 : -7.5;
			break;
		case 8: gain = gain > 0 ? 6.5: -6.5;
			break;
		case 4: gain = gain > 0 ? 3.2 : -3.2;
			break;
	}
#endif
#if 0
	if (gain > 5) {
		gain = sqrt(gain * 4);
	} else if (gain < -5) {
		gain = -sqrt(-gain * 4);
	}
#endif

	if ((freq > 200 && freq < 250) || (freq > 1000 && freq < 1050))
		cout << "upper " << upper_limit << " lower " << lower_limit << " freq " << freq << " band_freq " << band_freq << " gain " << gain << endl;

	if (freq < lower_limit || freq > upper_limit)
		return 0;

	if (freq < band_freq) {
		//double k = gain / (band_freq - lower_limit);
		//double m = -(gain / (band_freq - lower_limit)) * lower_limit;
		double k = gain / (pow(log10(band_freq), 2) - pow(log10(lower_limit), 2));
		double m = gain - k * pow(log10(band_freq), 2);

		return k * pow(log10(freq), 2) + m;
	} else if (freq == band_freq) {
		return gain;
	} else {
		double k = gain / (pow(log10(band_freq), 2) - pow(log10(upper_limit), 2));
		double m = gain - k * pow(log10(band_freq), 2);

		return k * pow(log10(freq), 2) + m;
	}
#endif
}

static vector<size_t> getBandIndex(double frequency, const vector<double>& limits) {
	vector<size_t> indicies;

	for (size_t i = 0; i < limits.size(); i += 2) {
		auto low = limits.at(i);
		auto high = limits.at(i + 1);

		if (limits.size() > (i + 2))
			high = limits.at(i + 2);

		if (frequency >= low && frequency < high)
			indicies.push_back(i / 2);
	}

	return indicies;
}

double GainIterator::gainAt(double freq)
{
	if (freq < 1)
		return 0;

#if 1
	// Calculate band limits
	vector<double> band_limits;

	for (auto& node : nodes) {
		double width = pow(2.0, 1.0 / (2.0 * Base::config().get<double>("dsp_octave_width")));
		double lower = node.freq / width;
		double upper = node.freq * width;

		//cout << "Calculated width " << width << " with lower " << lower << " and upper " << upper << endl;

		band_limits.push_back(lower);
		band_limits.push_back(upper);
	}

	double sum = 0;

	for (auto& node : nodes) {
		double gain = gainForBand(node.freq, freq, 1, node.dbGain);
		double linear_gain = pow(10, gain / 20);
		sum += linear_gain;
	}

	sum = 20 * log10(sum);

	if ((freq < 50) || (freq > 1000 && freq < 1050))
		cout << "freq " << freq << " gain " << sum << endl;

#if 0
	auto indicies = getBandIndex(freq, band_limits);
	if (!indicies.empty()) {
		if (abs(sum) > abs(nodes.at(indicies.front()).dbGain)) {
			sum = nodes.at(indicies.front()).dbGain;
		}
	}
#endif

#if 0
	if (sum > 10)
		sum = 10;
	else if (sum < -10)
		sum = -10;
#endif

	return sum;
#endif
#if 0
	if ((nodeLeft == NULL && nodeRight == NULL) || (nodeLeft != NULL && freq < nodeLeft->freq))
	{
		FilterNode findNode(freq, 0);
		vector<FilterNode>::iterator it = lower_bound(nodes.begin(), nodes.end(), findNode);
		if (it != nodes.begin())
			nodeLeft = &*(it - 1);
		if (it != nodes.end())
			nodeRight = &*it;

		if (nodeLeft != NULL && nodeRight != NULL)
		{
			logLeft = log(nodeLeft->freq);
			logRightMinusLeft = log(nodeRight->freq) - logLeft;
		}
	}
	else if (nodeRight != NULL && freq > nodeRight->freq)
	{
		vector<FilterNode>::iterator it = nodes.begin() + (nodeRight - &*nodes.begin());
		while (it != nodes.end() && freq > it->freq)
		{
			it++;
		}
		if (it == nodes.end())
			nodeRight = NULL;
		else
			nodeRight = &*it;

		if (it != nodes.begin())
			nodeLeft = &*(it - 1);

		if (nodeLeft != NULL && nodeRight != NULL)
		{
			logLeft = log(nodeLeft->freq);
			logRightMinusLeft = log(nodeRight->freq) - logLeft;
		}
	}

	double dbGain;
	if (nodeLeft == NULL)
	{
		if (nodeRight == NULL)
			dbGain = 0.0;
		else
			dbGain = nodeRight->dbGain;
	}
	else if (nodeRight == NULL)
	{
		dbGain = nodeLeft->dbGain;
	}
	else
	{
		double t = (log(freq) - logLeft) / logRightMinusLeft;
		// to support dbGain == -INF for both nodes
		if (nodeLeft->dbGain == nodeRight->dbGain)
			dbGain = nodeLeft->dbGain;
		else
			dbGain = nodeLeft->dbGain + t * (nodeRight->dbGain - nodeLeft->dbGain);
	}

	return dbGain;
#endif
}
