#pragma once
#ifndef GOERTZEL_H
#define GOERTZEL_H

#include "Localization3D.h"

#include <vector>
#include <string>

namespace Goertzel {
	Localization3DInput runGoertzel(const std::vector<std::string>& ips);
}

float goertzel(int numSamples,int TARGET_FREQUENCY,int SAMPLING_RATE, float* data);
float goertzel(int samples, int frequency, int fs, short* data);

#endif