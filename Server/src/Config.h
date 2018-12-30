#pragma once
#ifndef CONFIG_H
#define CONFIG_H

#include <map>
#include <sstream>
#include <deque>
#include <vector>
#include <iostream>

class Config {
public:
	void parse(const std::string& filename);
	void clear();
	bool has(const std::string& key);

	std::map<std::string, std::deque<std::string>>& internal();

	template<class T>
	T get(const std::string& key, const T& default_value = T()) {
		auto iterator = configs_.find(key);

		if (iterator == configs_.end()) {
			std::cout << "No config value for key " << key << std::endl;

			return default_value;
		}

		std::istringstream stream(iterator->second.front());
		T value;
		stream >> value;

		return value;
	}

	template<class T>
	std::vector<T> getAll(const std::string& key, const std::vector<T>& default_value = std::vector<T>()) {
		auto iterator = configs_.find(key);

		if (iterator == configs_.end()) {
			std::cout << "No config value for key " << key << std::endl;

			return default_value;
		}

		std::vector<T> values;

		for (auto& string_value : iterator->second) {
			std::istringstream stream(string_value);
			T value;
			stream >> value;

			values.push_back(value);
		}

		return values;
	}

private:
	void add(const std::pair<std::string, std::deque<std::string>>& config);

	std::map<std::string, std::deque<std::string>> configs_;
};

#endif