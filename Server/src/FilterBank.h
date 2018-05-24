#pragma once
#ifndef FILTER_BANK_H
#define FILTER_BANK_H

#include <vector>
#include <cstddef>

class FilterBank {
public:
	void addBand(int frequency, double q);
	void apply(const std::vector<short>& samples, std::vector<short>& out, const std::vector<std::pair<int, double>>& gains, int fs);

private:
	class Filter {
	public:
		Filter(int frequency, double q);
		
		void reset(double gain, int fs);
		void process(const std::vector<double>& in, std::vector<double>& out);
		void disable();
		
		bool operator==(int frequency);
		
		int getFrequency() const;
		
	private:
		bool enabled_	= false;
		int frequency_	= 0;
		double q_		= 1;
		
		std::vector<double> a_;
		std::vector<double> b_;
		
		/*
		double a0;
		double a1;
		double a2;
		double b1;
		double b2;
		
		double z1;
		double z2;
		*/
	};
	
	std::vector<Filter> filters_;
};


#endif