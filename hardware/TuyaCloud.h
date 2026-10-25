#pragma once

#include <json/json.h>
#include <chrono>
#include <string>
#include <vector>

struct TuyaCloudDevice
{
	std::string id;
	std::string name;
	std::string category;
	std::string productId;
	std::string localKey;
	std::string ip;
	Json::Value raw;
};

// Small OpenAPI client used by the native TinyTuya hardware. It follows the
// same account lookup as TinyTuya Cloud: seed device -> UID -> account devices.
class CTuyaCloudClient
{
public:
	CTuyaCloudClient(std::string region, std::string clientId, std::string clientSecret);

	bool GetDevices(const std::string &searchDeviceId, std::vector<TuyaCloudDevice> &devices, std::string &error);
	bool GetSpecification(const std::string &deviceId, Json::Value &result, std::string &error);
	bool GetStatus(const std::string &deviceId, Json::Value &result, std::string &error);
	bool SendCommand(const std::string &deviceId, const std::string &code, const Json::Value &value, std::string &error);

private:
	bool Authenticate(std::string &error);
	bool Request(const std::string &path, std::string &response, std::string &error, const std::string &method = "GET", const std::string &body = "");
	bool ParseSuccessfulResponse(const std::string &response, Json::Value &root, std::string &error) const;
	std::string m_region;
	std::string m_clientId;
	std::string m_clientSecret;
	std::string m_host;
	std::string m_token;
	std::chrono::steady_clock::time_point m_tokenExpiresAt{};
};
