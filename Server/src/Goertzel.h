#pragma once
#ifndef GOERTZEL_H
#define GOERTZEL_H

#include "Localization3D.h"

#include <vector>
#include <string>

namespace Goertzel {
	Localization3DInput runGoertzel(const std::vector<std::string>& ips);
}

double goertzel(int numSamples,float TARGET_FREQUENCY,int SAMPLING_RATE, short* data);

#endif