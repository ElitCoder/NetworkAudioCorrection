#include "WavReader.h"

#include <iostream>
#include <numeric>

#define ERROR(...)	do { fprintf(stderr, "Error: "); fprintf(stderr, __VA_ARGS__); fprintf(stderr, "\n"); exit(1); } while(0)

using namespace std;

void WavReader::read(const string &filename, vector<short>& output) {
	WavHeader	header;
	int			header_size;
	FILE*		file = fopen(filename.c_str(), "r");
	
	if (file == NULL) {
		cout << "Warning: WavReader could not open file\n";
		
		throw exception();
	}
		
	header_size = fread(&header, sizeof(WavHeader), 1, file);
	
	if (header_size <= 0) {
		cout << "Warning: could not read header from WAV\n";
		
		throw exception();
	}
	
	short tmp;
	
	while (fread(&tmp, sizeof(short), 1, file) > 0)
		output.push_back(tmp);
		
	fclose(file);
	
	cout << "Read " << output.size() * sizeof(short) + sizeof(WavHeader) << " bytes from WAV file " << filename << "\n";
}