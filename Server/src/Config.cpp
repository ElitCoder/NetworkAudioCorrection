#include "Config.h"

#include <vector>
#include <fstream>
#include <iostream>

using namespace std;

map<string, string> Config::configs_;

void Config::add(const pair<string, string>& config) {
	configs_[config.first] = config.second;
}

static vector<string> getTokens(string input, char delimiter) {
	istringstream stream(input);
	vector<string> tokens;
	string token;
	
	while (getline(stream, token, delimiter))
		if (!token.empty())
			tokens.push_back(token);
	
	return tokens;
}

void Config::parse(const string& filename) {
	ifstream file(filename);
	
	if (!file.is_open()) {
		cout << "Warning: could not open config\n";
		
		return;
	}
	
	string line;
	
	while (getline(file, line)) {
		if (line.empty() || line.front() == '#')
			continue;
		
		auto tokens = getTokens(line, ' ');
		
		// Remove ':' from the setting
		tokens.front().pop_back();
		
		cout << "Set key " << tokens.front() << " to value " << tokens.back() << endl;
		
		add({ tokens.front(), tokens.back() });
	}
	
	file.close();
}

void Config::clear() {
	configs_.clear();
}