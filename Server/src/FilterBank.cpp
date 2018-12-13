#include "FilterBank.h"
#include "Base.h"
#include "Config.h"
#include "GainIterator.h"

#include <cmath>
#include <climits>
#include <algorithm>
#include <iostream>
#include <xmmintrin.h>
#include <cstring>

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
			alpha = sin(w0) * sinh(M_LN2 / 2 * (1.0 / Base::config().get<double>("dsp_octave_width")) * w0 / sin(w0));
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

// Minimum phase spectrum from coefficients
static void mps(fftwf_complex* timeData, fftwf_complex* freqData, fftwf_plan planForward, fftwf_plan planReverse)
{
	const unsigned int filterLength = 16384;
	double threshold = pow(10.0, -100.0 / 20.0);
	float logThreshold = (float)log(threshold);

	for (unsigned i = 0; i < filterLength * 2; i++)
	{
		if (freqData[i][0] < threshold)
			freqData[i][0] = logThreshold;
		else
			freqData[i][0] = log(freqData[i][0]);

		freqData[i][1] = 0;
	}

	fftwf_execute(planReverse);

	for (unsigned i = 0; i < filterLength * 2; i++)
	{
		timeData[i][0] /= filterLength * 2;
		timeData[i][1] /= filterLength * 2;
	}

	for (unsigned i = 1; i < filterLength; i++)
	{
		timeData[i][0] += timeData[filterLength * 2 - i][0];
		timeData[i][1] -= timeData[filterLength * 2 - i][1];

		timeData[filterLength * 2 - i][0] = 0;
		timeData[filterLength * 2 - i][1] = 0;
	}
	timeData[filterLength][1] *= -1;

	fftwf_execute(planForward);

	for (unsigned i = 0; i < filterLength * 2; i++)
	{
		double eR = exp(freqData[i][0]);
		freqData[i][0] = float(eR * cos(freqData[i][1]));
		freqData[i][1] = float(eR * sin(freqData[i][1]));
	}
}

void hcProcessSingle(HConvSingle *filter)
{
#if 0
	int s, n, start, stop, flen;
	float *x_real;
	float *x_imag;
	float *h_real;
	float *h_imag;
	float *y_real;
	float *y_imag;

	flen = filter->framelength;
	x_real = filter->in_freq_real;
	x_imag = filter->in_freq_imag;
	start = filter->steptask[filter->step];
	stop  = filter->steptask[filter->step + 1];
	for (s = start; s < stop; s++)
	{
		n = (s + filter->mixpos) % filter->num_mixbuf;
		y_real = filter->mixbuf_freq_real[n];
		y_imag = filter->mixbuf_freq_imag[n];
		h_real = filter->filterbuf_freq_real[s];
		h_imag = filter->filterbuf_freq_imag[s];
		for (n = 0; n < flen + 1; n++)
		{
			y_real[n] += x_real[n] * h_real[n] -
			             x_imag[n] * h_imag[n];
			y_imag[n] += x_real[n] * h_imag[n] +
			             x_imag[n] * h_real[n];
		}
	}
	filter->step = (filter->step + 1) % filter->maxstep;
#endif

	int s, n, start, stop, flen, flen4;
	__m128 *x4_real;
	__m128 *x4_imag;
	__m128 *h4_real;
	__m128 *h4_imag;
	__m128 *y4_real;
	__m128 *y4_imag;
	float *x_real;
	float *x_imag;
	float *h_real;
	float *h_imag;
	float *y_real;
	float *y_imag;

	flen = filter->framelength;
	x_real = filter->in_freq_real;
	x_imag = filter->in_freq_imag;
	x4_real = (__m128*)x_real;
	x4_imag = (__m128*)x_imag;
	start = filter->steptask[filter->step];
	stop  = filter->steptask[filter->step + 1];
	for (s = start; s < stop; s++)
	{
		n = (s + filter->mixpos) % filter->num_mixbuf;
		y_real = filter->mixbuf_freq_real[n];
		y_imag = filter->mixbuf_freq_imag[n];
		y4_real = (__m128*)y_real;
		y4_imag = (__m128*)y_imag;
		h_real = filter->filterbuf_freq_real[s];
		h_imag = filter->filterbuf_freq_imag[s];
		h4_real = (__m128*)h_real;
		h4_imag = (__m128*)h_imag;
		flen4 = flen / 4;
		for (n = 0; n < flen4; n++)
		{
#ifdef WIN32
			__m128 a = _mm_mul_ps(x4_real[n], h4_real[n]);
			__m128 b = _mm_mul_ps(x4_imag[n], h4_imag[n]);
			__m128 c = _mm_sub_ps(a, b);
			y4_real[n] = _mm_add_ps(y4_real[n], c);
			a = _mm_mul_ps(x4_real[n], h4_imag[n]);
			b = _mm_mul_ps(x4_imag[n], h4_real[n]);
			c = _mm_add_ps(a, b);
			y4_imag[n] = _mm_add_ps(y4_imag[n], c);
#else
			y4_real[n] += x4_real[n] * h4_real[n] -
			              x4_imag[n] * h4_imag[n];
			y4_imag[n] += x4_real[n] * h4_imag[n] +
			              x4_imag[n] * h4_real[n];
#endif
		}
		y_real[flen] += x_real[flen] * h_real[flen] -
		                x_imag[flen] * h_imag[flen];
		y_imag[flen] += x_real[flen] * h_imag[flen] +
		                x_imag[flen] * h_real[flen];
	}
	filter->step = (filter->step + 1) % filter->maxstep;
}

void hcGetSingle(HConvSingle *filter, float *y)
{
	int flen, mpos;
	float *out;
	float *hist;
	int size, n, j;

	flen = filter->framelength;
	mpos = filter->mixpos;
	out  = filter->dft_time;
	hist = filter->history_time;
	for (j = 0; j < flen + 1; j++)
	{
		filter->dft_freq[j][0] = filter->mixbuf_freq_real[mpos][j];
		filter->dft_freq[j][1] = filter->mixbuf_freq_imag[mpos][j];
		filter->mixbuf_freq_real[mpos][j] = 0.0;
		filter->mixbuf_freq_imag[mpos][j] = 0.0;
	}
	fftwf_execute(filter->ifft);
	for (n = 0; n < flen; n++)
	{
		y[n] = out[n] + hist[n];
	}
	size = sizeof(float) * flen;
	memcpy(hist, &(out[flen]), size);
	filter->mixpos = (filter->mixpos + 1) % filter->num_mixbuf;
}

void hcPutSingle(HConvSingle *filter, float *x)
{
	int j, flen, size;

	flen = filter->framelength;
	size = sizeof(float) * flen;
	memcpy(filter->dft_time, x, size);
	memset(&(filter->dft_time[flen]), 0, size);
	fftwf_execute(filter->fft);
	for (j = 0; j < flen + 1; j++)
	{
		filter->in_freq_real[j] = filter->dft_freq[j][0];
		filter->in_freq_imag[j] = filter->dft_freq[j][1];
	}
}

void hcInitSingle(HConvSingle *filter, float *h, int hlen, int flen, int steps)
{
	int i, j, size, num, pos;
	float gain;

	// processing step counter
	filter->step = 0;

	// number of processing steps per audio frame
	filter->maxstep = steps;

	// current frame index
	filter->mixpos = 0;

	// number of samples per audio frame
	filter->framelength = flen;

	// DFT buffer (time domain)
	size = sizeof(float) * 2 * flen;
	filter->dft_time = (float *)fftwf_malloc(size);

	// DFT buffer (frequency domain)
	size = sizeof(fftwf_complex) * (flen + 1);
	filter->dft_freq = (fftwf_complex*)fftwf_malloc(size);

	// input buffer (frequency domain)
	size = sizeof(float) * (flen + 1);
	filter->in_freq_real = (float*)fftwf_malloc(size);
	filter->in_freq_imag = (float*)fftwf_malloc(size);

	// number of filter segments
	filter->num_filterbuf = (hlen + flen - 1) / flen;

	// processing tasks per step
	size = sizeof(int) * (steps + 1);
	filter->steptask = (int *)malloc(size);
	num = filter->num_filterbuf / steps;
	for (i = 0; i <= steps; i++)
		filter->steptask[i] = i * num;
	if (filter->steptask[1] == 0)
		pos = 1;
	else
		pos = 2;
	num = filter->num_filterbuf % steps;
	for (j = pos; j < pos + num; j++)
	{
		for (i = j; i <= steps; i++)
			filter->steptask[i]++;
	}

	// filter segments (frequency domain)
	size = sizeof(float*) * filter->num_filterbuf;
	filter->filterbuf_freq_real = (float**)fftwf_malloc(size);
	filter->filterbuf_freq_imag = (float**)fftwf_malloc(size);
	for (i = 0; i < filter->num_filterbuf; i++)
	{
		size = sizeof(float) * (flen + 1);
		filter->filterbuf_freq_real[i] = (float*)fftwf_malloc(size);
		filter->filterbuf_freq_imag[i] = (float*)fftwf_malloc(size);
	}

	// number of mixing segments
	filter->num_mixbuf = filter->num_filterbuf + 1;

	// mixing segments (frequency domain)
	size = sizeof(float*) * filter->num_mixbuf;
	filter->mixbuf_freq_real = (float**)fftwf_malloc(size);
	filter->mixbuf_freq_imag = (float**)fftwf_malloc(size);
	for (i = 0; i < filter->num_mixbuf; i++)
	{
		size = sizeof(float) * (flen + 1);
		filter->mixbuf_freq_real[i] = (float*)fftwf_malloc(size);
		filter->mixbuf_freq_imag[i] = (float*)fftwf_malloc(size);
		memset(filter->mixbuf_freq_real[i], 0, size);
		memset(filter->mixbuf_freq_imag[i], 0, size);
	}

	// history buffer (time domain)
	size = sizeof(float) * flen;
	filter->history_time = (float *)fftwf_malloc(size);
	memset(filter->history_time, 0, size);

	// FFT transformation plan
	filter->fft = fftwf_plan_dft_r2c_1d(2 * flen, filter->dft_time, filter->dft_freq, FFTW_ESTIMATE|FFTW_PRESERVE_INPUT);

	// IFFT transformation plan
	filter->ifft = fftwf_plan_dft_c2r_1d(2 * flen, filter->dft_freq, filter->dft_time, FFTW_ESTIMATE|FFTW_PRESERVE_INPUT);

	// generate filter segments
	gain = 0.5f / flen;
	size = sizeof(float) * 2 * flen;
	memset(filter->dft_time, 0, size);
	for (i = 0; i < filter->num_filterbuf - 1; i++)
	{
		for (j = 0; j < flen; j++)
			filter->dft_time[j] = gain * h[i * flen + j];
		fftwf_execute(filter->fft);
		for (j = 0; j < flen + 1; j++)
		{
			filter->filterbuf_freq_real[i][j] = filter->dft_freq[j][0];
			filter->filterbuf_freq_imag[i][j] = filter->dft_freq[j][1];
		}
	}
	for (j = 0; j < hlen - i * flen; j++)
		filter->dft_time[j] = gain * h[i * flen + j];
	size = sizeof(float) * ((i + 1) * flen - hlen);
	memset(&(filter->dft_time[hlen - i * flen]), 0, size);
	fftwf_execute(filter->fft);
	for (j = 0; j < flen + 1; j++)
	{
		filter->filterbuf_freq_real[i][j] = filter->dft_freq[j][0];
		filter->filterbuf_freq_imag[i][j] = filter->dft_freq[j][1];
	}
}

void FilterBank::apply(const vector<short>& samples, vector<short>& out, const vector<pair<int, double>>& gains, double fs, bool write) {
	vector<double> normalized;
	initializeFiltering(samples, normalized, gains, fs);

	if (!write)
		return;

	/* Clear outgoing buffer */
	out.clear();

	/* All filters are of the same type */
	if (filters_.empty())
		cout << "WARNING: Empty filter vector\n";

	auto type = filters_.front().getType();

	if (type == PARAMETRIC) {
		/* Apply filters in cascade */
		for (auto& filter : filters_) {
			vector<double> filtered;
			filter.process(normalized, filtered);
			normalized = filtered;
		}
	} else if (type == BANDPASS) {
#if 0
		const unsigned int filterLength = 16384;
		/* Create convolver filter */
		fftwf_make_planner_thread_safe();
		fftwf_complex* timeData = fftwf_alloc_complex(filterLength * 2);
		fftwf_complex* freqData = fftwf_alloc_complex(filterLength * 2);
		fftwf_plan planForward = fftwf_plan_dft_1d(filterLength * 2, timeData, freqData, FFTW_FORWARD, FFTW_ESTIMATE);
		fftwf_plan planReverse = fftwf_plan_dft_1d(filterLength * 2, freqData, timeData, FFTW_BACKWARD, FFTW_ESTIMATE);

		vector<FilterNode> nodes;

		for (auto& param : gains) {
			nodes.push_back(FilterNode(param.first, param.second));
		}

		GainIterator gainIterator(nodes);
		for (unsigned i = 0; i < filterLength; i++)
		{
			double freq = i * 1.0 * fs / (filterLength * 2);
			double dbGain = gainIterator.gainAt(freq);
			float gain = (float)pow(10.0, dbGain / 20.0);

			freqData[i][0] = gain;
			freqData[i][1] = 0;
			freqData[2 * filterLength - i - 1][0] = gain;
			freqData[2 * filterLength - i - 1][1] = 0;
		}

		mps(timeData, freqData, planForward, planReverse);

		fftwf_execute(planReverse);

		for (unsigned i = 0; i < 2 * filterLength; i++)
		{
			timeData[i][0] /= 2 * filterLength;
			timeData[i][1] /= 2 * filterLength;
		}

		for (unsigned i = 0; i < filterLength; i++)
		{
			float factor = (float)(0.5 * (1 + cos(2 * M_PI * i * 1.0 / (2 * filterLength))));
			timeData[i][0] *= factor;
			timeData[i][1] *= factor;
		}

		float* buf = new float[filterLength];
		for (unsigned i = 0; i < filterLength; i++)
		{
			buf[i] = timeData[i][0];
		}

		fftwf_free(timeData);
		fftwf_free(freqData);
		fftwf_destroy_plan(planForward);
		fftwf_destroy_plan(planReverse);

		HConvSingle filters;

		/* Process */
		HConvSingle* filter = &filters;
		hcInitSingle(filter, buf, filterLength, normalized.size(), 1);

		delete[] buf;

		float* inputChannel = new float[normalized.size()];
		float* outputChannel = new float[normalized.size()];

		for (size_t i = 0; i < normalized.size(); i++) {
			inputChannel[i] = normalized.at(i);
			outputChannel[i] = 0;
		}

		hcPutSingle(filter, inputChannel);
		hcProcessSingle(filter);
		hcGetSingle(filter, outputChannel);

		for (size_t i = 0; i < normalized.size(); i++) {
			normalized.at(i) = outputChannel[i];
		}

		delete[] inputChannel;
		delete[] outputChannel;

#if 0
		float* inputChannel = new float[16384];
		float* outputChannel = new float[16384];

		for (size_t i = 0; i < normalized.size() - filterLength; i += filterLength) {
			for (size_t j = i; j < i + filterLength; j++) {
				inputChannel[j - i] = normalized.at(i);
			}
			hcPutSingle(filter, inputChannel);
			hcProcessSingle(filter);
			hcGetSingle(filter, outputChannel);
			/* Copy output to normalized */
			for (size_t j = 0; j < filterLength; j++) {
				normalized.at(i + j) = outputChannel[j];
			}
		}

		delete[] inputChannel;
		delete[] outputChannel;
#endif
#endif
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

double FilterBank::gainAt(double frequency, double fs) {
	double sum = 0;

	// Sum gainAt() for all filters
	for (auto& filter : filters_) {
		double db = filter.gainAt(frequency, fs);
		//double linear = pow(10, db / 10);

		//cout << "freq " << frequency << " db " << db << endl;

		sum += db;
	}

	//sum = 10 * log10(sum);
	return sum;
}

double FilterBank::Filter::gainAt(double frequency, double fs) {
	double omega = 2 * M_PI * frequency / fs;
	double sn = sin(omega / 2.0);
	double phi = sn * sn;
	double b0 = b_[0];
	double b1 = b_[1];
	double b2 = b_[2];
	double a0 = a_[0];
	double a1 = a_[1];
	double a2 = a_[2];

	double dbGain = 10 * log10(pow(b0 + b1 + b2, 2) - 4 * (b0 * b1 + 4 * b0 * b2 + b1 * b2) * phi + 16 * b0 * b2 * phi * phi)
		- 10 * log10(pow(a0 + a1 + a2, 2) - 4 * (a0 * a1 + 4 * a0 * a2 + a1 * a2) * phi + 16 * a0 * a2 * phi * phi);

	return dbGain;
}