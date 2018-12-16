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
	double lower_limit = band_freq * pow(2, -2 / bw);
	double upper_limit = band_freq * pow(2, 2 / bw);

	//gain = gain * 0.8;

	if ((freq > 200 && freq < 250) || (freq > 1000 && freq < 1050))
		cout << "upper " << upper_limit << " lower " << lower_limit << " freq " << freq << " band_freq " << band_freq << " gain " << gain << endl;

	if (freq < lower_limit || freq > upper_limit)
		return 0;

	if (freq < band_freq) {
		double k = gain / (band_freq - lower_limit);
		double m = -(gain / (band_freq - lower_limit)) * lower_limit;

		return k * freq + m;
	} else if (freq == band_freq) {
		return gain;
	} else {
		double k = gain / (band_freq - upper_limit);
		double m = -(gain / (band_freq - upper_limit)) * upper_limit;

		return k * freq + m;
	}
}

double GainIterator::gainAt(double freq)
{
	double sum = 0;

	for (auto& node : nodes) {
		double gain = gainForBand(node.freq, freq, 1, node.dbGain);

		sum += gain;
	}

	if ((freq > 200 && freq < 250) || (freq > 1000 && freq < 1050))
		cout << "freq " << freq << " gain " << sum << endl;

	return sum;

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
