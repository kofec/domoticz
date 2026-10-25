#pragma once

#include <json/json.h>
#include <string>

// Small synchronous TinyTuya LAN client for protocol 3.3 and 3.4.
// A client instance represents one transaction; 3.4 negotiates a fresh
// session key on each connection, so the long-term local key is never used
// as the working key after negotiation.
class CTuyaLanClient
{
public:
	CTuyaLanClient(std::string deviceId, std::string localKey, double protocolVersion);

	bool Query(const std::string &address, unsigned short port, Json::Value &dps, std::string &error);
	bool SetDP(const std::string &address, unsigned short port, int dp, const Json::Value &value, std::string &error);

private:
	std::string m_deviceId;
	std::string m_localKey;
	double m_protocolVersion;
	unsigned int m_sequence = 1;
};
