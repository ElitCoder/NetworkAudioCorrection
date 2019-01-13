#pragma once
#ifndef FILTER_BANK_H
#define FILTER_BANK_H

#include <vector>
#include <cstddef>
#include <fftw3.h>

enum {
	PARAMETRIC,
	BANDPASS,
	LOW_SHELF,
	HIGH_SHELF
};

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

class FilterBank {
public:
	void addBand(int frequency, double q, int type);
	void apply(const std::vector<short>& samples, std::vector<short>& out, const std::vector<std::pair<int, double>>& gains, double fs, bool write = false);
	double gainAt(double frequency, double fs);
	bool hasFastMode() const;

private:
	void initializeFiltering(const std::vector<short>& in, std::vector<double>& out, const std::vector<std::pair<int, double>>& gains, int fs);
	void finalizeFiltering(const std::vector<double>& in, std::vector<short>& out);
	void applyFilters(std::vector<double>& normalized, double fs);

	std::vector<Filter> filters_;
};

typedef struct str_HConvSingle
{
	int step;			// processing step counter
	int maxstep;			// number of processing steps per audio frame
	int mixpos;			// current frame index
	int framelength;		// number of samples per audio frame
	int *steptask;			// processing tasks per step
	float *dft_time;		// DFT buffer (time domain)
	fftwf_complex *dft_freq;	// DFT buffer (frequency domain)
	float *in_freq_real;		// input buffer (frequency domain)
	float *in_freq_imag;		// input buffer (frequency domain)
	int num_filterbuf;		// number of filter segments
	float **filterbuf_freq_real;	// filter segments (frequency domain)
	float **filterbuf_freq_imag;	// filter segments (frequency domain)
	int num_mixbuf;			// number of mixing segments
	float **mixbuf_freq_real;	// mixing segments (frequency domain)
	float **mixbuf_freq_imag;	// mixing segments (frequency domain)
	float *history_time;		// history buffer (time domain)
	fftwf_plan fft;			// FFT transformation plan
	fftwf_plan ifft;		// IFFT transformation plan
} HConvSingle;

#endif