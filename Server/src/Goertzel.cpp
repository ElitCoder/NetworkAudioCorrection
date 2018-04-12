#include "Goertzel.h"
#include "Recording.h"
#include "WavReader.h"
#include "Base.h"

#include <iostream>
#include <cmath>

using namespace std;

static const int FREQ_N = 16;
static const int FREQ_FREQ = 4000;
static const double FREQ_REDUCING = 0.001;
static const double FREQ_THRESHOLD = 0.01;

// Taken from git/SO
double goertzel(int numSamples,float TARGET_FREQUENCY,int SAMPLING_RATE, short* data)
{
    int     k,i;
    float   floatnumSamples;
    float   omega,sine,cosine,coeff,q0,q1,q2,magnitude,real,imag;

    float   scalingFactor = numSamples / 2.0;

    floatnumSamples = (float) numSamples;
    k = (int) (0.5 + ((floatnumSamples * TARGET_FREQUENCY) / (float)SAMPLING_RATE));
    omega = (2.0 * M_PI * k) / floatnumSamples;
    sine = sin(omega);
    cosine = cos(omega);
    coeff = 2.0 * cosine;
    q0=0;
    q1=0;
    q2=0;

    for(i=0; i<numSamples; i++)
    {
        q0 = coeff * q1 - q2 + data[i];
        q2 = q1;
        q1 = q0;
    }

    // calculate the real and imaginary results
    // scaling appropriately
    real = (q1 - q2 * cosine) / scalingFactor;
    imag = (q2 * sine) / scalingFactor;

    magnitude = sqrtf(real*real + imag*imag);
    return magnitude;
}

static double calculateDistance(Recording& master, Recording& recording) {
	long long r12 = recording.getTonePlayingWhen(master.getId());
	long long p1 = master.getTonePlayingWhen(master.getId());
	long long r21 = master.getTonePlayingWhen(recording.getId());
	long long p2 = recording.getTonePlayingWhen(recording.getId());
	
	// TODO: remove this, it's unnecessary
	// T12 = Tp + Dt
	// T21 = Tp - Dt
	double T12 = r12 - p1;
	double T21 = r21 - p2;
	
	double Dt = -(static_cast<double>(T21) - static_cast<double>(T12)) / 2;
	
	double Tp1 = T12 - Dt;
	double Tp2 = T21 + Dt;
	
	double Tp = (Tp1 + Tp2) / 2;
	double Tp_sec = Tp / 48000;
	
	return abs(Tp_sec * 343);
}

static void analyzeSound(const vector<string>& filenames, const vector<string>& ips, vector<Recording>& recordings) {
	for (size_t i = 0; i < filenames.size(); i++) {
		string filename = filenames.at(i);
		
		recordings.push_back(Recording(ips.at(i), i));
		
		Recording& recording = recordings.back();
		WavReader::read(filename, recording.getData());
		
		if (recording.getData().empty())
			return;
				
		recording.findStartingTones(filenames.size(), FREQ_N, FREQ_THRESHOLD, FREQ_REDUCING, FREQ_FREQ, (Base::config().get<int>("play_time") + 1) * 48000, Base::config().get<int>("idle_time"));
	}
}

static Localization3DInput runDistanceCalculation(const vector<string>& ips, vector<Recording>& recordings) {
	Localization3DInput output;
	
	for (size_t i = 0; i < recordings.size(); i++) {
		Recording& master = recordings.at(i);
		
		for (size_t j = 0; j < recordings.size(); j++) {
			if (j == i) {
				master.addDistance(i, 0);
				
				continue;
			}
			
			Recording& recording = recordings.at(j);
			double distance = calculateDistance(master, recording);
			
			cout << "Calculated distance " << master.getIp() << " -> " << recording.getIp() << " : " << distance << " m\n";
			
			master.addDistance(j, distance);
		}
	}
	
	// Build output
	for (size_t i = 0; i < recordings.size(); i++) {
		string ip = ips.at(i);
		vector<double> distances;
		
		for (size_t j = 0; j < recordings.size(); j++) {
			distances.push_back(recordings.at(i).getDistance(j));
		}
		
		output.push_back({ ip, distances });
	}
	
	return output;
}

namespace Goertzel {
	Localization3DInput runGoertzel(const vector<string>& ips) {
		vector<string> filenames;
		vector<Recording> recordings;
		
		for (auto& ip : ips) {
			string filename = "results/cap";
			filename += ip;
			filename += ".wav";
			
			filenames.push_back(filename);
		}
		
		analyzeSound(filenames, ips, recordings);
		
		if (recordings.empty())
			return Localization3DInput();
		
		return runDistanceCalculation(ips, recordings);
	}
}