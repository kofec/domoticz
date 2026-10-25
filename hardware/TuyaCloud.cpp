#include "stdafx.h"
#include "TuyaCloud.h"

#include "../httpclient/HTTPClient.h"

#include <openssl/evp.h>
#include <openssl/hmac.h>

#include <algorithm>
#include <chrono>
#include <cctype>
#include <memory>
#include <sstream>
#include <utility>

namespace
{
std::string Hex(const unsigned char *data, size_t length, const bool upper)
{
	static constexpr char LOWER[] = "0123456789abcdef";
	static constexpr char UPPER[] = "0123456789ABCDEF";
	const char *digits = upper ? UPPER : LOWER;
	std::string result;
	result.reserve(length * 2);
	for (size_t i = 0; i < length; ++i)
	{
		result.push_back(digits[data[i] >> 4]);
		result.push_back(digits[data[i] & 0x0F]);
	}
	return result;
}

std::string Sha256(const std::string &value)
{
	unsigned char digest[EVP_MAX_MD_SIZE]{};
	unsigned int length = 0;
	if (EVP_Digest(value.data(), value.size(), digest, &length, EVP_sha256(), nullptr) != 1)
		return {};
	return Hex(digest, length, false);
}

std::string Hmac(const std::string &key, const std::string &value)
{
	unsigned char digest[EVP_MAX_MD_SIZE]{};
	unsigned int length = 0;
	if (HMAC(EVP_sha256(), key.data(), static_cast<int>(key.size()),
		reinterpret_cast<const unsigned char *>(value.data()), value.size(), digest, &length) == nullptr)
		return {};
	return Hex(digest, length, true);
}

std::string JsonString(const Json::Value &value, const char *name)
{
	return value.isMember(name) && value[name].isString() ? value[name].asString() : std::string();
}

std::string PathSegment(const std::string &value)
{
	static constexpr char DIGITS[] = "0123456789ABCDEF";
	std::string encoded;
	for (const unsigned char c : value)
	{
		if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~')
			encoded.push_back(static_cast<char>(c));
		else
		{
			encoded.push_back('%');
			encoded.push_back(DIGITS[c >> 4]);
			encoded.push_back(DIGITS[c & 0x0F]);
		}
	}
	return encoded;
}

} // namespace

CTuyaCloudClient::CTuyaCloudClient(std::string region, std::string clientId, std::string clientSecret)
	: m_region(std::move(region)), m_clientId(std::move(clientId)), m_clientSecret(std::move(clientSecret))
{
	std::transform(m_region.begin(), m_region.end(), m_region.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
	if (m_region == "eu")
		m_host = "https://openapi.tuyaeu.com";
	else if (m_region == "us")
		m_host = "https://openapi.tuyaus.com";
	else if (m_region == "cn")
		m_host = "https://openapi.tuyacn.com";
	else
		m_host = "https://openapi.tuyaeu.com";
}

bool CTuyaCloudClient::ParseSuccessfulResponse(const std::string &response, Json::Value &root, std::string &error) const
{
	Json::CharReaderBuilder builder;
	std::string parseError;
	std::unique_ptr<Json::CharReader> reader(builder.newCharReader());
	if (!reader->parse(response.data(), response.data() + response.size(), &root, &parseError))
	{
		error = "Tuya Cloud returned invalid JSON";
		return false;
	}
	if (!root.isObject() || !root.isMember("success") || !root["success"].asBool())
	{
		const std::string message = JsonString(root, "msg");
		std::string code;
		if (root.isMember("code"))
		{
			if (root["code"].isString())
				code = root["code"].asString();
			else if (root["code"].isInt() || root["code"].isUInt())
				code = std::to_string(root["code"].asInt64());
		}
		error = "Tuya Cloud request failed";
		if (!code.empty())
			error += " (" + code + ")";
		if (!message.empty())
			error += ": " + message;
		return false;
	}
	return true;
}

bool CTuyaCloudClient::Request(const std::string &path, std::string &response, std::string &error,
	const std::string &method, const std::string &body)
{
	if (m_clientId.empty() || m_clientSecret.empty() || m_host.empty())
	{
		error = "TinyTuya Cloud credentials or region are missing";
		return false;
	}
	if (path != "/v1.0/token?grant_type=1" &&
		(m_token.empty() || std::chrono::steady_clock::now() >= m_tokenExpiresAt) && !Authenticate(error))
		return false;

	const auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(
		std::chrono::system_clock::now().time_since_epoch()).count();
	const std::string timestamp = std::to_string(milliseconds);
	const bool tokenRequest = m_token.empty();
	const std::string bodyHash = Sha256(body);
	if (bodyHash.empty())
	{
		error = "Could not calculate Tuya request digest";
		return false;
	}
	std::string signingHeaders;
	std::string signatureInput;
	if (tokenRequest)
	{
		// Match TinyTuya's current post-2021 signing format, including its
		// secret signature header on the token request.
		signingHeaders = "secret:" + m_clientSecret + "\n";
		signatureInput = m_clientId + timestamp + method + "\n" + bodyHash + "\n" + signingHeaders + "\n/" + path.substr(1);
	}
	else
	{
		if (method != "GET")
			signingHeaders = "content-type:application/json\n";
		signatureInput = m_clientId + m_token + timestamp + method + "\n" + bodyHash + "\n" + signingHeaders + "\n/" + path.substr(1);
	}
	const std::string signature = Hmac(m_clientSecret, signatureInput);
	if (signature.empty())
	{
		error = "Could not sign Tuya Cloud request";
		return false;
	}
	std::vector<std::string> headers = {
		"client_id: " + m_clientId,
		"sign: " + signature,
		"t: " + timestamp,
		"sign_method: HMAC-SHA256",
		"mode: cors",
	};
	if (tokenRequest)
	{
		headers.push_back("secret: " + m_clientSecret);
		headers.push_back("Signature-Headers: secret");
	}
	else
	{
		headers.push_back("access_token: " + m_token);
		if (method != "GET")
		{
			headers.push_back("Content-Type: application/json");
			headers.push_back("Signature-Headers: content-type");
		}
	}

	bool ok = false;
	if (method == "GET")
		ok = HTTPClient::GET(m_host + path, headers, response);
	else if (method == "POST")
		ok = HTTPClient::POST(m_host + path, body, headers, response);
	if (!ok)
	{
		error = "Could not connect to Tuya Cloud";
		return false;
	}
	return true;
}

bool CTuyaCloudClient::Authenticate(std::string &error)
{
	std::string response;
	if (!Request("/v1.0/token?grant_type=1", response, error))
		return false;
	Json::Value root;
	if (!ParseSuccessfulResponse(response, root, error) || !root.isMember("result") || !root["result"].isObject())
		return false;
	m_token = JsonString(root["result"], "access_token");
	if (m_token.empty())
	{
		error = "Tuya Cloud response did not contain an access token";
		return false;
	}
	int expiresIn = 7200;
	if (root["result"].isMember("expire_time") && root["result"]["expire_time"].isInt())
		expiresIn = root["result"]["expire_time"].asInt();
	m_tokenExpiresAt = std::chrono::steady_clock::now() + std::chrono::seconds(std::max(60, expiresIn - 60));
	return true;
}

bool CTuyaCloudClient::GetDevices(const std::string &searchDeviceId, std::vector<TuyaCloudDevice> &devices, std::string &error)
{
	devices.clear();
	std::string response;
	if (!Request("/v1.0/devices/" + PathSegment(searchDeviceId), response, error))
		return false;
	Json::Value root;
	if (!ParseSuccessfulResponse(response, root, error) || !root.isMember("result") || !root["result"].isObject())
		return false;
	const std::string uid = JsonString(root["result"], "uid");
	if (uid.empty())
	{
		error = "Tuya Search DeviceID did not resolve to an account UID";
		return false;
	}
	if (!Request("/v1.0/users/" + PathSegment(uid) + "/devices", response, error) || !ParseSuccessfulResponse(response, root, error))
		return false;
	const Json::Value &result = root["result"];
	const Json::Value *list = nullptr;
	if (result.isArray())
		list = &result;
	else if (result.isObject() && result["list"].isArray())
		list = &result["list"];
	if (list == nullptr)
	{
		error = "Tuya Cloud device list has an unexpected format";
		return false;
	}
	for (const auto &item : *list)
	{
		if (!item.isObject())
			continue;
		TuyaCloudDevice device;
		device.id = JsonString(item, "id");
		device.name = JsonString(item, "name");
		device.category = JsonString(item, "category");
		device.productId = JsonString(item, "product_id");
		device.localKey = JsonString(item, "local_key");
		device.ip = JsonString(item, "ip");
		device.raw = item;
		if (!device.id.empty())
			devices.push_back(std::move(device));
	}
	return true;
}

bool CTuyaCloudClient::GetSpecification(const std::string &deviceId, Json::Value &result, std::string &error)
{
	std::string response;
	if (!Request("/v1.0/iot-03/devices/" + PathSegment(deviceId) + "/specification", response, error))
		return false;
	Json::Value root;
	if (!ParseSuccessfulResponse(response, root, error) || !root.isMember("result"))
		return false;
	result = root["result"];
	return result.isObject();
}

bool CTuyaCloudClient::GetStatus(const std::string &deviceId, Json::Value &result, std::string &error)
{
	std::string response;
	if (!Request("/v1.0/iot-03/devices/" + PathSegment(deviceId) + "/status", response, error))
		return false;
	Json::Value root;
	if (!ParseSuccessfulResponse(response, root, error) || !root.isMember("result"))
		return false;
	result = root["result"];
	return result.isArray() || result.isObject();
}

bool CTuyaCloudClient::SendCommand(const std::string &deviceId, const std::string &code, const Json::Value &value, std::string &error)
{
	if (code.empty())
	{
		error = "Tuya command code is empty";
		return false;
	}
	Json::Value root(Json::objectValue);
	Json::Value command(Json::objectValue);
	command["code"] = code;
	command["value"] = value;
	root["commands"].append(command);
	Json::StreamWriterBuilder builder;
	builder["indentation"] = "";
	const std::string body = Json::writeString(builder, root);
	std::string response;
	if (!Request("/v1.0/iot-03/devices/" + PathSegment(deviceId) + "/commands", response, error, "POST", body))
		return false;
	Json::Value result;
	return ParseSuccessfulResponse(response, result, error);
}
