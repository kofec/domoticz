#include "stdafx.h"
#include "TuyaLan.h"

#include <boost/asio/ip/tcp.hpp>
#include <openssl/crypto.h>
#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <openssl/rand.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <ctime>
#include <memory>
#include <utility>
#include <vector>

namespace
{
constexpr uint32_t TUYA_PREFIX = 0x000055AA;
constexpr uint32_t TUYA_SUFFIX = 0x0000AA55;
constexpr size_t TUYA_HEADER_SIZE = 16;
constexpr size_t TUYA_MAX_PAYLOAD = 64 * 1024;
constexpr auto TUYA_TIMEOUT = std::chrono::seconds(4);
constexpr const char *TUYA_VERSION_33 = "3.3";
constexpr const char *TUYA_VERSION_34 = "3.4";

void Put32(std::vector<uint8_t> &out, uint32_t value)
{
	out.push_back(static_cast<uint8_t>(value >> 24));
	out.push_back(static_cast<uint8_t>(value >> 16));
	out.push_back(static_cast<uint8_t>(value >> 8));
	out.push_back(static_cast<uint8_t>(value));
}

uint32_t Get32(const uint8_t *p)
{
	return (static_cast<uint32_t>(p[0]) << 24) | (static_cast<uint32_t>(p[1]) << 16) | (static_cast<uint32_t>(p[2]) << 8) | p[3];
}

std::string String(const std::vector<uint8_t> &value)
{
	return std::string(value.begin(), value.end());
}

bool AesEcb(const std::vector<uint8_t> &input, const std::string &key, bool encrypt, bool padding, std::vector<uint8_t> &output)
{
	if (key.size() != 16 || (!padding && (input.size() % 16 != 0)))
		return false;
	EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
	if (ctx == nullptr)
		return false;
	bool ok = EVP_CipherInit_ex(ctx, EVP_aes_128_ecb(), nullptr,
		reinterpret_cast<const unsigned char *>(key.data()), nullptr, encrypt ? 1 : 0) == 1;
	if (ok)
		ok = EVP_CIPHER_CTX_set_padding(ctx, padding ? 1 : 0) == 1;
	output.resize(input.size() + EVP_MAX_BLOCK_LENGTH);
	int first = 0;
	int last = 0;
	if (ok)
		ok = EVP_CipherUpdate(ctx, output.data(), &first, input.data(), static_cast<int>(input.size())) == 1;
	if (ok)
		ok = EVP_CipherFinal_ex(ctx, output.data() + first, &last) == 1;
	EVP_CIPHER_CTX_free(ctx);
	if (!ok)
	{
		output.clear();
		return false;
	}
	output.resize(static_cast<size_t>(first + last));
	return true;
}

bool HmacSha256(const std::string &key, const uint8_t *data, size_t length, std::vector<uint8_t> &digest)
{
	digest.resize(EVP_MAX_MD_SIZE);
	unsigned int digestLength = 0;
	if (HMAC(EVP_sha256(), key.data(), static_cast<int>(key.size()), data, length, digest.data(), &digestLength) == nullptr)
		return false;
	digest.resize(digestLength);
	return true;
}

uint32_t Crc32(const uint8_t *data, size_t length)
{
	uint32_t crc = 0xFFFFFFFF;
	for (size_t i = 0; i < length; ++i)
	{
		crc ^= data[i];
		for (int bit = 0; bit < 8; ++bit)
			crc = (crc >> 1) ^ (0xEDB88320 & (0U - (crc & 1U)));
	}
	return crc ^ 0xFFFFFFFF;
}

bool Is34(double version)
{
	return version >= 3.4;
}

std::vector<uint8_t> VersionHeader(double version)
{
	const char *text = Is34(version) ? TUYA_VERSION_34 : TUYA_VERSION_33;
	std::vector<uint8_t> result(text, text + 3);
	result.insert(result.end(), 12, 0);
	return result;
}

bool PackFrame(unsigned int sequence, uint32_t command, const std::vector<uint8_t> &payload, bool useHmac,
	const std::string &key, std::vector<uint8_t> &frame)
{
	const size_t trailerSize = useHmac ? 36 : 8;
	if (payload.size() + trailerSize > TUYA_MAX_PAYLOAD)
		return false;
	frame.clear();
	frame.reserve(TUYA_HEADER_SIZE + payload.size() + trailerSize);
	Put32(frame, TUYA_PREFIX);
	Put32(frame, sequence);
	Put32(frame, command);
	Put32(frame, static_cast<uint32_t>(payload.size() + trailerSize));
	frame.insert(frame.end(), payload.begin(), payload.end());
	if (useHmac)
	{
		std::vector<uint8_t> mac;
		if (!HmacSha256(key, frame.data(), frame.size(), mac) || mac.size() != 32)
			return false;
		frame.insert(frame.end(), mac.begin(), mac.end());
	}
	else
	{
		Put32(frame, Crc32(frame.data(), frame.size()));
	}
	Put32(frame, TUYA_SUFFIX);
	return true;
}

struct TuyaFrame
{
	uint32_t command = 0;
	std::vector<uint8_t> payload;
};

bool UnpackFrame(const std::vector<uint8_t> &frame, bool useHmac, const std::string &key, TuyaFrame &message)
{
	if (frame.size() < TUYA_HEADER_SIZE + (useHmac ? 36 : 8) || Get32(frame.data()) != TUYA_PREFIX ||
		Get32(frame.data() + frame.size() - 4) != TUYA_SUFFIX)
		return false;
	const uint32_t length = Get32(frame.data() + 12);
	const size_t trailerSize = useHmac ? 36 : 8;
	if (length != frame.size() - TUYA_HEADER_SIZE || length < trailerSize)
		return false;
	const size_t contentLength = frame.size() - trailerSize;
	if (useHmac)
	{
		std::vector<uint8_t> expected;
		if (!HmacSha256(key, frame.data(), contentLength, expected) || expected.size() != 32 ||
			CRYPTO_memcmp(expected.data(), frame.data() + contentLength, 32) != 0)
			return false;
	}
	else if (Get32(frame.data() + contentLength) != Crc32(frame.data(), contentLength))
		return false;
	message.command = Get32(frame.data() + 8);
	message.payload.assign(frame.begin() + TUYA_HEADER_SIZE, frame.begin() + contentLength);
	// Standard 55AA replies have a four-byte return code before the data.
	if (message.payload.size() >= 4)
		message.payload.erase(message.payload.begin(), message.payload.begin() + 4);
	return true;
}

class TuyaConnection
{
public:
	TuyaConnection(const std::string &address, unsigned short port) : m_stream()
	{
		m_stream.expires_after(TUYA_TIMEOUT);
		m_stream.connect(address, std::to_string(port));
	}

	bool IsOpen() { return m_stream && m_stream.socket().is_open(); }

	bool Exchange(const std::vector<uint8_t> &request, bool useHmac, const std::string &key, TuyaFrame &response)
	{
		if (!IsOpen())
			return false;
		m_stream.expires_after(TUYA_TIMEOUT);
		m_stream.write(reinterpret_cast<const char *>(request.data()), static_cast<std::streamsize>(request.size()));
		m_stream.flush();
		if (!m_stream)
			return false;

		std::array<uint8_t, TUYA_HEADER_SIZE> header{};
		m_stream.read(reinterpret_cast<char *>(header.data()), static_cast<std::streamsize>(header.size()));
		if (m_stream.gcount() != static_cast<std::streamsize>(header.size()) || Get32(header.data()) != TUYA_PREFIX)
			return false;
		const uint32_t length = Get32(header.data() + 12);
		if (length < (useHmac ? 36 : 8) || length > TUYA_MAX_PAYLOAD)
			return false;
		std::vector<uint8_t> frame(header.begin(), header.end());
		const size_t oldSize = frame.size();
		frame.resize(oldSize + length);
		m_stream.expires_after(TUYA_TIMEOUT);
		m_stream.read(reinterpret_cast<char *>(frame.data() + oldSize), static_cast<std::streamsize>(length));
		if (m_stream.gcount() != static_cast<std::streamsize>(length))
			return false;
		return UnpackFrame(frame, useHmac, key, response);
	}

	bool Send(const std::vector<uint8_t> &request)
	{
		if (!IsOpen())
			return false;
		m_stream.expires_after(TUYA_TIMEOUT);
		m_stream.write(reinterpret_cast<const char *>(request.data()), static_cast<std::streamsize>(request.size()));
		m_stream.flush();
		return static_cast<bool>(m_stream);
	}

private:
	boost::asio::ip::tcp::iostream m_stream;
};

bool MakeCommand(unsigned int sequence, uint32_t command, const std::vector<uint8_t> &rawPayload, double version,
	const std::string &key, std::vector<uint8_t> &frame)
{
	std::vector<uint8_t> payload = rawPayload;
	const bool hasProtocolHeader = command != 3 && command != 4 && command != 5 && command != 9 && command != 10 && command != 16 && command != 18;
	if (Is34(version))
	{
		if (hasProtocolHeader)
		{
			auto header = VersionHeader(version);
			header.insert(header.end(), payload.begin(), payload.end());
			payload.swap(header);
		}
		std::vector<uint8_t> encrypted;
		if (!AesEcb(payload, key, true, true, encrypted))
			return false;
		payload.swap(encrypted);
		return PackFrame(sequence, command, payload, true, key, frame);
	}

	if (command != 3 && command != 4 && command != 5 && command != 9 && command != 16 && command != 18)
	{
		std::vector<uint8_t> encrypted;
		if (!AesEcb(payload, key, true, true, encrypted))
			return false;
		payload.swap(encrypted);
	}
	if (hasProtocolHeader)
	{
		auto header = VersionHeader(version);
		header.insert(header.end(), payload.begin(), payload.end());
		payload.swap(header);
	}
	return PackFrame(sequence, command, payload, false, key, frame);
}

bool DecodeStatus(double version, const std::string &key, std::vector<uint8_t> payload, Json::Value &dps)
{
	if (Is34(version))
	{
		std::vector<uint8_t> plain;
		if (!AesEcb(payload, key, false, true, plain))
			return false;
		payload.swap(plain);
		const auto header = VersionHeader(version);
		if (payload.size() >= header.size() && std::equal(header.begin(), header.end(), payload.begin()))
			payload.erase(payload.begin(), payload.begin() + header.size());
	}
	else
	{
		const auto header = VersionHeader(version);
		if (payload.size() >= header.size() && std::equal(header.begin(), header.end(), payload.begin()))
			payload.erase(payload.begin(), payload.begin() + header.size());
		std::vector<uint8_t> plain;
		if (!AesEcb(payload, key, false, true, plain))
			return false;
		payload.swap(plain);
	}
	Json::Value root;
	Json::CharReaderBuilder builder;
	std::string errors;
	const std::string json = String(payload);
	std::unique_ptr<Json::CharReader> reader(builder.newCharReader());
	if (!reader->parse(json.data(), json.data() + json.size(), &root, &errors))
		return false;
	if (root.isMember("dps"))
		dps = root["dps"];
	else if (root.isMember("data") && root["data"].isObject() && root["data"].isMember("dps"))
		dps = root["data"]["dps"];
	else
		return false;
	return dps.isObject();
}

bool DecodeSessionKeyResponse(const std::string &localKey, const TuyaFrame &response,
	const std::vector<uint8_t> &localNonce, std::vector<uint8_t> &remoteNonce)
{
	if (response.command != 4)
		return false;
	std::vector<uint8_t> plain;
	if (!AesEcb(response.payload, localKey, false, true, plain) || plain.size() < 48)
		return false;
	std::vector<uint8_t> expected;
	if (!HmacSha256(localKey, localNonce.data(), localNonce.size(), expected) || expected.size() != 32 ||
		CRYPTO_memcmp(expected.data(), plain.data() + 16, 32) != 0)
		return false;
	remoteNonce.assign(plain.begin(), plain.begin() + 16);
	return true;
}

bool NegotiateSession(TuyaConnection &connection, unsigned int &sequence, const std::string &localKey, std::string &sessionKey)
{
	std::vector<uint8_t> localNonce(16);
	if (RAND_bytes(localNonce.data(), static_cast<int>(localNonce.size())) != 1)
		return false;
	std::vector<uint8_t> request;
	if (!MakeCommand(sequence++, 3, localNonce, 3.4, localKey, request))
		return false;
	TuyaFrame response;
	if (!connection.Exchange(request, true, localKey, response))
		return false;
	std::vector<uint8_t> remoteNonce;
	if (!DecodeSessionKeyResponse(localKey, response, localNonce, remoteNonce))
		return false;
	std::vector<uint8_t> remoteMac;
	if (!HmacSha256(localKey, remoteNonce.data(), remoteNonce.size(), remoteMac))
		return false;
	request.clear();
	if (!MakeCommand(sequence++, 5, remoteMac, 3.4, localKey, request) || !connection.Send(request))
		return false;
	std::vector<uint8_t> sharedNonce(16);
	for (size_t i = 0; i < sharedNonce.size(); ++i)
		sharedNonce[i] = localNonce[i] ^ remoteNonce[i];
	std::vector<uint8_t> encrypted;
	if (!AesEcb(sharedNonce, localKey, true, false, encrypted) || encrypted.size() != 16)
		return false;
	sessionKey = String(encrypted);
	return true;
}

bool BuildJson(const Json::Value &root, std::vector<uint8_t> &payload)
{
	Json::StreamWriterBuilder builder;
	builder["indentation"] = "";
	const std::string json = Json::writeString(builder, root);
	payload.assign(json.begin(), json.end());
	return !payload.empty();
}

} // namespace

CTuyaLanClient::CTuyaLanClient(std::string deviceId, std::string localKey, double protocolVersion)
	: m_deviceId(std::move(deviceId)), m_localKey(std::move(localKey)), m_protocolVersion(protocolVersion)
{
}

bool CTuyaLanClient::Query(const std::string &address, unsigned short port, Json::Value &dps, std::string &error)
{
	if (m_deviceId.empty() || m_localKey.size() != 16 || (m_protocolVersion != 3.3 && m_protocolVersion != 3.4))
	{
		error = "TinyTuya requires a device ID, 16-character local key and protocol 3.3 or 3.4";
		return false;
	}
	try
	{
		TuyaConnection connection(address, port);
		if (!connection.IsOpen())
		{
			error = "Connection to the Tuya device failed";
			return false;
		}
		std::string key = m_localKey;
		if (Is34(m_protocolVersion) && !NegotiateSession(connection, m_sequence, m_localKey, key))
		{
			error = "Tuya 3.4 session-key negotiation failed";
			return false;
		}
		Json::Value query(Json::objectValue);
		if (Is34(m_protocolVersion))
			query = Json::Value(Json::objectValue);
		else
		{
			query["gwId"] = m_deviceId;
			query["devId"] = m_deviceId;
			query["uid"] = m_deviceId;
			query["t"] = "0";
		}
		std::vector<uint8_t> body;
		if (!BuildJson(query, body) && !Is34(m_protocolVersion))
		{
			error = "Could not encode Tuya status request";
			return false;
		}
		std::vector<uint8_t> request;
		const uint32_t command = Is34(m_protocolVersion) ? 16 : 10;
		if (!MakeCommand(m_sequence++, command, body, m_protocolVersion, key, request))
		{
			error = "Could not encode Tuya status request";
			return false;
		}
		TuyaFrame response;
		if (!connection.Exchange(request, Is34(m_protocolVersion), key, response) ||
			!DecodeStatus(m_protocolVersion, key, response.payload, dps))
		{
			error = "Tuya status response could not be verified or decoded";
			return false;
		}
		return true;
	}
	catch (const std::exception &e)
	{
		error = std::string("Tuya LAN error: ") + e.what();
		return false;
	}
}

bool CTuyaLanClient::SetDP(const std::string &address, unsigned short port, int dp, const Json::Value &value, std::string &error)
{
	if (m_deviceId.empty() || m_localKey.size() != 16 || (m_protocolVersion != 3.3 && m_protocolVersion != 3.4) || dp < 1)
	{
		error = "TinyTuya command configuration is invalid";
		return false;
	}
	try
	{
		TuyaConnection connection(address, port);
		if (!connection.IsOpen())
		{
			error = "Connection to the Tuya device failed";
			return false;
		}
		std::string key = m_localKey;
		if (Is34(m_protocolVersion) && !NegotiateSession(connection, m_sequence, m_localKey, key))
		{
			error = "Tuya 3.4 session-key negotiation failed";
			return false;
		}
		Json::Value root(Json::objectValue);
		if (Is34(m_protocolVersion))
		{
			root["protocol"] = 5;
			root["t"] = static_cast<Json::Int64>(std::time(nullptr));
			root["data"]["dps"][std::to_string(dp)] = value;
		}
		else
		{
			root["devId"] = m_deviceId;
			root["uid"] = m_deviceId;
			root["t"] = "0";
			root["dps"][std::to_string(dp)] = value;
		}
		std::vector<uint8_t> body;
		std::vector<uint8_t> request;
		if (!BuildJson(root, body) || !MakeCommand(m_sequence++, Is34(m_protocolVersion) ? 13 : 7, body, m_protocolVersion, key, request))
		{
			error = "Could not encode Tuya command";
			return false;
		}
		TuyaFrame response;
		if (!connection.Exchange(request, Is34(m_protocolVersion), key, response))
		{
			error = "Tuya command was not acknowledged";
			return false;
		}
		return true;
	}
	catch (const std::exception &e)
	{
		error = std::string("Tuya LAN error: ") + e.what();
		return false;
	}
}
