#include "WavReader.h"

#include <iostream>
#include <numeric>
#include <fstream>

using namespace std;

void WavReader::read(const string &filename, vector<short>& output) {
	ifstream file(filename);
	
	if (!file.is_open()) {
		cout << "Error: could not open file " << filename << endl;
		
		throw exception();
	} else
	
	cout << "Opened WAV " << filename << " for reading\n";
	
	file.seekg(0, ios_base::end);
	size_t size = file.tellg();
	file.seekg(0, ios_base::beg);
	
	// Read header
	WavHeader header;
	file.read((char*)&header, sizeof(WavHeader));
	cout << "Read WAV header (" << sizeof(WavHeader) << " bytes)\n";
	
	// Read file, excluding the header
	output = vector<short>((size - sizeof(WavHeader)) / sizeof(short));
	file.read((char*)&output[0], size - sizeof(WavHeader));
	cout << "Read WAV file (" << output.size() * sizeof(short) << " bytes)\n";
	
	file.close();
}

void WavReader::write(const string& filename, const vector<short>& input, const string& copy_header) {
	ofstream file(filename);
	
	if (!file.is_open()) {
		cout << "Error: could not open file " << filename << endl;
		
		throw exception();
	}
	
	cout << "Opened WAV " << filename << " for writing\n";
	
	WavHeader header;
	
	if (!copy_header.empty()) {
		cout << "Requesting reading output WAV header from " << copy_header << endl;
		
		ifstream header_file(copy_header);
		
		if (!header_file.is_open()) {
			cout << "Error: could not open file " << copy_header << endl;
			
			throw exception();
		}
		
		header_file.read((char*)&header, sizeof(WavHeader));
		cout << "Read WAV header (" << sizeof(WavHeader) << " bytes)\n";
		
		header_file.close();
	} else {
		cout << "Warning: not reading header from another WAV is not supported right now\n";
		
		throw exception();
	}
	
	// Write header
	file.write((const char*)&header, sizeof(WavHeader));
	cout << "Wrote WAV header (" << sizeof(WavHeader) << " bytes)\n";
	
	const auto& data = input.data();
	
	// Write file
	file.write((const char*)&data[0], input.size() * sizeof(short));
	cout << "Wrote WAV file (" << input.size() * sizeof(short) << " bytes)\n";
	
	file.close();
}