#ifndef CONFIG_H
#define CONFIG_H

#include <map>
#include <sstream>

struct NoConfigException {
};

class Config {
public:
	static void add(const std::pair<std::string, std::string>& config);
	static void parse(const std::string& filename);
	static void clear();
	
	template<class T>
	static T get(const std::string& key) {
		auto iterator = configs_.find(key);
		
		if (iterator == configs_.end())
			throw NoConfigException();
			
		std::istringstream stream(iterator->second);
		T value;
		stream >> value;
		
		return value;
	}
	
private:
	static std::map<std::string, std::string> configs_;
};

#endif