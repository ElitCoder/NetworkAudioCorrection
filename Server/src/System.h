#pragma once
#ifndef SYSTEM_H
#define SYSTEM_H

#include "Handle.h"
#include "Speaker.h"

// libnessh
#include <libnessh/SSHMaster.h>

#include <vector>

// Contains SSH connections to all speakers and current speaker settings
class System {
public:
	SSHOutput runScript(const std::vector<std::string>& ips, const std::vector<std::string>& scripts, bool temporary_connection = false);
	bool sendFile(const std::vector<std::string>& ips, const std::string& from, const std::string& to, bool overwrite = true);
	bool getFile(const std::vector<std::string>& ips, const std::vector<std::string>& from, const std::vector<std::string>& to);
	
	bool getRecordings(const std::vector<std::string>& ips);
	bool checkConnection(const std::vector<std::string>& ips);

	Speaker& getSpeaker(const std::string& ip);
	std::vector<Speaker*> getSpeakers(const std::vector<std::string>& ips);
	
private:
	Speaker& addSpeaker(Speaker& speaker);
	
	std::vector<Speaker> speakers_;
	SSHMaster ssh_;
};

#endif