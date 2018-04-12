#ifndef CONFIG_H
#define CONFIG_H

#include <unordered_map>
#include <sstream>
#include <iostream>

struct NoConfigException {
};

class Config {
public:
	void add(const std::pair<std::string, std::string>& config);
	void parse(const std::string& filename);
	void clear();
	
	template<class T>
	T get(const std::string& key) {
		auto iterator = configs_.find(key);
		
		if (iterator == configs_.end()) {
			std::cout << "WARNING: Key " << key << " not found\n";
			
			throw NoConfigException();
		}
			
		std::istringstream stream(iterator->second);
		T value;
		stream >> value;
		
		return value;
	}
	
private:
	std::unordered_map<std::string, std::string> configs_;
};

#endif