#pragma once
#ifndef FILTER_BANK_H
#define FILTER_BANK_H

#include <vector>
#include <cstddef>

enum {
	PARAMETRIC,
	BANDPASS
};

class FilterBank {
public:
	void addBand(int frequency, double q, int type);
	void apply(const std::vector<short>& samples, std::vector<short>& out, const std::vector<std::pair<int, double>>& gains, int fs, bool write = false);
	double gainAt(double frequency, double fs);

private:
	void initializeFiltering(const std::vector<short>& in, std::vector<double>& out, const std::vector<std::pair<int, double>>& gains, int fs);
	void finalizeFiltering(const std::vector<double>& in, std::vector<short>& out);

	class Filter {
	public:
		Filter(int frequency, double q, int type);

		void reset(double gain, int fs);
		void process(const std::vector<double>& in, std::vector<double>& out);
		void disable();

		bool operator==(int frequency);

		int getFrequency() const;
		int getType() const;

		double gainAt(double frequency, double fs);

	private:
		bool enabled_	= false;
		int frequency_	= 0;
		double q_		= 1;
		int type_		= 0;

		std::vector<double> a_;
		std::vector<double> b_;
	};

	std::vector<Filter> filters_;
};


#endif