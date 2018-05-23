#pragma once
#ifndef FILTER_H
#define FILTER_H

#include <vector>

class FilterBank {
public:
	void addBand(int frequency, double q);
	void apply(const std::vector<short>& samples, std::vector<short>& out, const std::vector<std::pair<int, double>>& gains, int fs, size_t start = 0, size_t stop = 0);

private:
	class Filter {
	public:
		Filter(int frequency, double q);
		
		void reset(double gain, int fs);
		double process(double sample);
		void disable();
		
		bool operator==(int frequency);
		
	private:
		bool enabled_	= false;
		int frequency_	= 0;
		double q_		= 1;
		
		double a0;
		double a1;
		double a2;
		double b1;
		double b2;
		
		double z1;
		double z2;
	};
	
	std::vector<Filter> filters_;
};


#endif