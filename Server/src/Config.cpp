#include "Config.h"

#include <vector>
#include <fstream>

using namespace std;

void Config::add(const pair<string, deque<string>>& config) {
	configs_[config.first] = config.second;
}

static deque<string> getTokens(string input, char delimiter) {
	istringstream stream(input);
	deque<string> tokens;
	string token;
	
	while (getline(stream, token, delimiter))
		if (!token.empty())
			tokens.push_back(token);
	
	return tokens;
}

void Config::parse(const string& filename) {
	ifstream file(filename);
	
	if (!file.is_open()) {
		cout << "Could not open config " << filename << endl;
		
		return;
	}
	
	string line;
	
	while (getline(file, line)) {
		if (line.empty() || line.front() == '#')
			continue;
		
		auto tokens = getTokens(line, ' ');
		
		// Remove ':' from the setting
		tokens.front().pop_back();
		
		cout << "Set key " << tokens.front() << " to value(s): ";
		
		string key = tokens.front();
		tokens.pop_front();
		
		for (auto& token : tokens)
			cout << token << " ";
		
		cout << endl;
		
		add({ key, tokens });
	}
	
	file.close();
}

void Config::clear() {
	configs_.clear();
}

map<string, deque<string>>& Config::internal() {
	return configs_;
}