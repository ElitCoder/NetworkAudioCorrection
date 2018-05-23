#pragma once
#ifndef FILTER_H
#define FILTER_H

#include <vector>

class Filter {
public:
	void addBand(int frequency, double q);
	void apply(const std::vector<short>& samples, std::vector<short>& out, const std::vector<double>& gains, int fs);
	
private:
	void applyBand(std::vector<short>& samples, int band, double gain, int fs);
	
	std::vector<int> frequencies_;
	std::vector<double> qs_;
};

#endif