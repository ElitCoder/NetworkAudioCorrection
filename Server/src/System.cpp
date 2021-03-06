#include "System.h"
#include "Speaker.h"
#include "Base.h"
#include "Config.h"

// libcurlpp
#include <curlpp/cURLpp.hpp>
#include <curlpp/Options.hpp>
#include <curlpp/Easy.hpp>

#include <algorithm>
#include <sstream>
#include <thread>

using namespace std;

static void enableSSH(const string& ip) {
	cURLpp::Cleanup clean;
	string disable_string = "http://";
	disable_string += ip;
	disable_string += "/axis-cgi/admin/param.cgi?action=update&Network.SSH.Enabled=yes";
	
	cURLpp::Easy request;
	ostringstream stream;
	
	request.setOpt(cURLpp::options::Url(disable_string.c_str())); 
	request.setOpt(cURLpp::options::UserPwd(string("root:pass")));
	request.setOpt(cURLpp::options::HttpAuth(CURLAUTH_ANY));
	request.setOpt(cURLpp::options::WriteStream(&stream));
	request.setOpt(cURLpp::options::Timeout(10));
	
	try {
		request.perform();
	} catch (cURLpp::RuntimeError& exception) {
		// GET request failed
		cout << "Warning: request timed out in trying to set SSH enable\n";
		cout << "Request: " << disable_string << endl;
		cout << "cURLpp error: " << exception.what() << endl;
		
		return;
	}
	
	if (stream.str() != "OK")
		cout << "ERROR in enableSSH()\n";
}

Speaker& System::addSpeaker(Speaker& speaker) {
	cout << "Using backup addSpeaker()\n";
	cout << "Adding & connecting speaker " << speaker.getIP() << endl;
	
	if (!Base::config().get<bool>("enable_testing")) {
		// Enable SSH
		enableSSH(speaker.getIP());
	
		// Open connection to Speaker if it's offline
		speaker.setOnline(ssh_.connect(speaker.getIP(), "pass"));
		
		if (!speaker.isOnline())
			cout << "Warning: speaker " << speaker.getIP() << " is not online\n";
	} else {
		// Set to online for testing
		speaker.setOnline(true);
	}
	
	// Add speaker to list
	speakers_.push_back(speaker);
	
	// Return reference to pushed back speaker
	return speakers_.back();
}

// Batch check for connectivity
bool System::checkConnection(const vector<string>& ips) {
	auto speakers = getSpeakers(ips);
	
	for (auto* speaker : speakers)
		if (!speaker->isOnline())
			return false;

	return true;
}

SSHOutput System::runScript(const vector<string>& ips, const vector<string>& scripts, bool temporary_connection) {
	if (temporary_connection) {
		// Do this scripting for a new temporary connection
		return System().runScript(ips, scripts);
	}
	
	// Make sure all speakers are connected
	checkConnection(ips);
	
	for (size_t i = 0; i < ips.size(); i++) {
		cout << "SSH: running script (" << ips.at(i) << ")\n**************\n";
		cout << scripts.at(i) << "**************\n\n";
	}
	
	cout << "Running SSH commands... " << flush;
	
	ssh_.setSetting(SETTING_ENABLE_SSH_OUTPUT_VECTOR_STYLE, true);
	auto outputs = ssh_.command(ips, scripts);
	ssh_.setSetting(SETTING_ENABLE_SSH_OUTPUT_VECTOR_STYLE, false);
	
	// TODO: Add option to print outputs here
	
	if (outputs.empty())
		cout << "ERROR\n";
	else
		cout << "done\n";
		
	return outputs;
}

vector<Speaker*> System::getSpeakers(const vector<string>& ips) {
	vector<string> not_connected;
	
	for (auto& ip : ips) {
		auto iterator = find(speakers_.begin(), speakers_.end(), ip);
		
		if (iterator == speakers_.end())
			not_connected.push_back(ip);
	}
	
	if (!not_connected.empty()) {		
		thread* enable_ssh_threads = new thread[not_connected.size()];
		
		for (size_t i = 0; i< not_connected.size(); i++)
			enable_ssh_threads[i] = thread(enableSSH, ref(not_connected.at(i)));
			
		for (size_t i = 0; i < not_connected.size(); i++)
			enable_ssh_threads[i].join();
			
		delete[] enable_ssh_threads;
		
		auto result = ssh_.connectResult(not_connected, "pass");
		
		for (size_t i = 0; i < result.size(); i++) {
			Speaker speaker;
			speaker.setIP(not_connected.at(i));
			speaker.setOnline(result.at(i));
			
			speakers_.push_back(speaker);
		}
	}
	
	vector<Speaker*> speakers;
	
	for (auto& ip : ips)
		speakers.push_back(&getSpeaker(ip));
		
	return speakers;
}

Speaker& System::getSpeaker(const string& ip) {
	auto iterator = find(speakers_.begin(), speakers_.end(), ip);
	
	if (iterator == speakers_.end()) {
		Speaker speaker;
		speaker.setIP(ip);
		
		return addSpeaker(speaker);
	} else {
		return *iterator;
	}
}

bool System::sendFile(const vector<string>& ips, const string& from, const string& to, bool overwrite) {
	checkConnection(ips);
	
	cout << "Sending file " << from << " -> " << to << "... " << flush;
	auto status = ssh_.transferRemote(ips, vector<string>(ips.size(), from), vector<string>(ips.size(), to), overwrite);
	cout << (status ? "done\n" : "ERROR\n");
	
	return status;
}

bool System::getFile(const vector<string>& ips, const vector<string>& from, const vector<string>& to) {
	checkConnection(ips);
	
	for (size_t i = 0; i < ips.size(); i++) {
		cout << "Retrieving (" << ips.at(i) << ") " << from.at(i) << " -> " << to.at(i) << endl;
	}
	
	cout << "Retrieving files from SSH... " << flush;
	auto status = ssh_.transferLocal(ips, from, to, true);
	cout << (status ? "done\n" : "ERROR\n") << flush;
	
	return status;
}

bool System::getRecordings(const vector<string>& ips) {
	vector<string> from;
	vector<string> to;
	
	for (auto& ip : ips) {
		from.push_back("/tmp/cap" + ip + ".wav");
		to.push_back("results");
	}
	
	return getFile(ips, from, to);
}

void System::setSpeakerProfile(const Profile& profile) {
	speaker_profile_ = profile;
}

void System::setMicrophoneProfile(const Profile& profile) {
	microphone_profile_ = profile;
}

const Profile& System::getSpeakerProfile() const {
	return speaker_profile_;
}

Profile& System::getSpeakerProfile() {
	return speaker_profile_;
}

const Profile& System::getMicrophoneProfile() const {
	return microphone_profile_;
}