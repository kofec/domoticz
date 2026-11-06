#include "stdafx.h"
#include "TuyaLocal.h"

#include "TuyaLan.h"
#include "hardwaretypes.h"
#include "../main/Logger.h"
#include "../main/SQLHelper.h"
#include "../main/mainworker.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cstdio>
#include <utility>

namespace
{
constexpr int TUYA_LOCAL_UNIT_SWITCH = 1;
}

CTuyaLocal::CTuyaLocal(const int ID, std::string address, const unsigned short port, std::string deviceId, std::string localKey,
	const int pollSeconds, const int protocol, const int switchDp)
	: m_address(std::move(address)), m_port(port ? port : 6668), m_deviceId(std::move(deviceId)), m_localKey(std::move(localKey)),
	  m_pollSeconds(pollSeconds), m_protocol(protocol == 34 ? 3.4 : 3.3), m_switchDp(switchDp)
{
	m_HwdID = ID;
	if (m_pollSeconds < 5)
		m_pollSeconds = 30;
	if (m_switchDp < 1)
		m_switchDp = 1;
}

CTuyaLocal::CTuyaLocal(const int ID, std::string accessId, std::string accessSecret, std::string extra, const int region,
	const int cloudPollSeconds, const int localRefreshSeconds, const int debugLevel)
	: m_accessId(std::move(accessId)), m_accessSecret(std::move(accessSecret)), m_pollSeconds(cloudPollSeconds),
	  m_region(region), m_localRefreshSeconds(localRefreshSeconds), m_debugLevel(debugLevel), m_protocol(3.3), m_switchDp(1), m_cloudConfigured(true)
{
	m_HwdID = ID;
	Json::Value settings;
	Json::CharReaderBuilder builder;
	std::string errors;
	std::unique_ptr<Json::CharReader> reader(builder.newCharReader());
	if (reader->parse(extra.data(), extra.data() + extra.size(), &settings, &errors) && settings.isObject())
		m_searchDeviceId = settings.isMember("search_device_id") && settings["search_device_id"].isString()
			? settings["search_device_id"].asString() : std::string();
	if (m_pollSeconds < 60)
		m_pollSeconds = 900;
}

bool CTuyaLocal::StartHardware()
{
	if (m_cloudConfigured)
	{
		if (m_accessId.empty() || m_accessSecret.empty() || m_searchDeviceId.empty() || m_region < 0 || m_region > 2)
		{
			Log(LOG_ERROR, "TinyTuya requires a region, Access ID, Access Secret and Search DeviceID");
			return false;
		}
		static const char *regions[] = { "eu", "us", "cn" };
		m_cloud = std::make_unique<CTuyaCloudClient>(regions[m_region], m_accessId, m_accessSecret);
		std::vector<TuyaCloudDevice> devices;
		std::string error;
		if (!m_cloud->GetDevices(m_searchDeviceId, devices, error))
		{
			Log(LOG_ERROR, "TinyTuya Cloud device discovery failed: %s", error.c_str());
			return false;
		}
		for (const auto &device : devices)
		{
			if (device.category != "cz")
				continue;
			Json::Value specification;
			if (!m_cloud->GetSpecification(device.id, specification, error))
			{
				Log(LOG_ERROR, "TinyTuya could not read switch capabilities for %s: %s", device.name.c_str(), error.c_str());
				continue;
			}
			std::string switchCode;
			if (specification["functions"].isArray())
			{
				for (const auto &function : specification["functions"])
				{
					if (!function.isObject() || !function["code"].isString())
						continue;
					const std::string code = function["code"].asString();
					if (code == "switch" || code == "switch_1" || code == "switch_on")
					{
						switchCode = code;
						break;
					}
				}
			}
			if (!switchCode.empty())
				m_cloudDevices.push_back({ device, MakeDomoticzId(device.id), switchCode });
		}
		if (m_cloudDevices.empty())
		{
			Log(LOG_ERROR, "TinyTuya Cloud returned no LAN switch in the initial native category (cz)");
			return false;
		}
		Log(LOG_STATUS, "TinyTuya Cloud discovered %u account devices and %u supported switches (category cz)",
			static_cast<unsigned int>(devices.size()), static_cast<unsigned int>(m_cloudDevices.size()));
		m_address = m_cloudDevices.front().cloud.ip;
		m_port = 6668;
		m_deviceId = m_cloudDevices.front().cloud.id;
		m_localKey = m_cloudDevices.front().cloud.localKey;
	}
	if (!m_cloudConfigured && (m_address.empty() || m_deviceId.empty() || m_localKey.size() != 16))
	{
		Log(LOG_ERROR, "TinyTuya LAN requires an address, device ID and 16-character local key");
		return false;
	}
	RequestStart();
	CreateDevices();
	m_thread = std::make_shared<std::thread>(&CTuyaLocal::Do_Work, this);
	SetThreadNameInt(m_thread->native_handle());
	m_bIsStarted = true;
	sOnConnected(this);
	return true;
}

bool CTuyaLocal::StopHardware()
{
	if (m_thread)
	{
		RequestStop();
		m_thread->join();
		m_thread.reset();
	}
	m_bIsStarted = false;
	return true;
}

void CTuyaLocal::Do_Work()
{
	Log(LOG_STATUS, "TinyTuya worker started for %s:%u (protocol %.1f, cloud poll %d s, LAN refresh %d s)",
		m_address.c_str(), m_port, m_protocol, m_pollSeconds, m_localRefreshSeconds);
	while (!IsStopRequested(1000))
	{
		m_LastHeartbeat = mytime(nullptr);
		if (--m_secondsToPoll <= 0)
		{
			m_secondsToPoll = m_pollSeconds;
			if (m_cloudConfigured)
				PollCloud();
			else
				PollLan();
		}
		if (m_cloudConfigured && m_localRefreshSeconds > 0 && --m_secondsToLocalRefresh <= 0)
		{
			m_secondsToLocalRefresh = m_localRefreshSeconds;
			PollLan();
		}
	}
	Log(LOG_STATUS, "TinyTuya LAN worker stopped");
}

void CTuyaLocal::CreateDevices()
{
	if (m_cloudConfigured)
	{
		for (const auto &device : m_cloudDevices)
		{
			const auto rows = m_sql.safe_query("SELECT ID FROM DeviceStatus WHERE (HardwareID==%d) AND (DeviceID=='%q') AND (Unit==%d)",
				m_HwdID, device.domoticzId.c_str(), TUYA_LOCAL_UNIT_SWITCH);
			if (!rows.empty())
				continue;
			m_sql.InsertDevice(m_HwdID, 0, device.domoticzId.c_str(), TUYA_LOCAL_UNIT_SWITCH, pTypeGeneralSwitch, sSwitchTypeAC,
				STYPE_OnOff, 0, "0", device.cloud.name, 12, 255, 1);
		}
		return;
	}
	const auto rows = m_sql.safe_query("SELECT ID FROM DeviceStatus WHERE (HardwareID==%d) AND (Unit==%d)", m_HwdID, TUYA_LOCAL_UNIT_SWITCH);
	if (!rows.empty())
		return;
	char deviceId[20];
	snprintf(deviceId, sizeof(deviceId), "%04X%04X", m_HwdID & 0xFFFF, TUYA_LOCAL_UNIT_SWITCH);
	m_sql.InsertDevice(m_HwdID, 0, deviceId, TUYA_LOCAL_UNIT_SWITCH, pTypeGeneralSwitch, sSwitchTypeAC, STYPE_OnOff,
		0, "0", m_Name + " - Power", 12, 255, 1);
}

bool CTuyaLocal::IsSwitchOn(const Json::Value &value) const
{
	if (value.isBool())
		return value.asBool();
	if (value.isInt() || value.isUInt() || value.isInt64() || value.isUInt64())
		return value.asInt64() != 0;
	if (value.isString())
	{
		std::string text = value.asString();
		std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
		return text == "true" || text == "on" || text == "1";
	}
	return false;
}

std::string CTuyaLocal::MakeDomoticzId(const std::string &deviceId)
{
	uint32_t hash = 2166136261U;
	for (const unsigned char c : deviceId)
	{
		hash ^= c;
		hash *= 16777619U;
	}
	char result[9];
	snprintf(result, sizeof(result), "%08X", hash);
	return result;
}

int32_t CTuyaLocal::MakeCommandId(const std::string &domoticzId)
{
	return static_cast<int32_t>(std::strtoul(domoticzId.c_str(), nullptr, 16));
}

void CTuyaLocal::UpdateSwitch(const std::string &deviceId, const std::string &deviceName, const bool on)
{
	const auto rows = m_sql.safe_query("SELECT DeviceID, Type, SubType, nValue, sValue, Name FROM DeviceStatus WHERE (HardwareID==%d) AND (DeviceID=='%q') AND (Unit==%d)",
		m_HwdID, deviceId.c_str(), TUYA_LOCAL_UNIT_SWITCH);
	if (rows.empty() || (atoi(rows[0][3].c_str()) == (on ? 1 : 0)))
		return;
	const std::string value = on ? "1" : "0";
	std::string devname = rows[0][5]; // UpdateValue takes the name by non-const reference
	const uint64_t rowId = m_sql.UpdateValue(m_HwdID, 0, rows[0][0].c_str(), TUYA_LOCAL_UNIT_SWITCH,
		static_cast<unsigned char>(atoi(rows[0][1].c_str())), static_cast<unsigned char>(atoi(rows[0][2].c_str())),
		12, 255, on ? 1 : 0, value.c_str(), devname, true, deviceName.c_str());
	if (rowId != static_cast<uint64_t>(-1))
		m_mainworker.sOnDeviceReceived(m_HwdID, rowId, devname, nullptr);
}

void CTuyaLocal::Poll()
{
	if (m_cloudConfigured)
		PollCloud();
	else
		PollLan();
}

void CTuyaLocal::PollCloud()
{
	if (!m_cloud)
		return;
	for (const auto &device : m_cloudDevices)
	{
		Json::Value status;
		std::string error;
		if (!m_cloud->GetStatus(device.cloud.id, status, error))
		{
			Log(LOG_ERROR, "TinyTuya Cloud status query failed for %s: %s", device.cloud.name.c_str(), error.c_str());
			continue;
		}
		if (!status.isArray())
			continue;
		for (const auto &item : status)
		{
			if (!item.isObject() || !item.isMember("code") || !item["code"].isString() || !item.isMember("value"))
				continue;
			const std::string code = item["code"].asString();
			if (code == device.switchCode)
			{
				UpdateSwitch(device.domoticzId, device.cloud.name, IsSwitchOn(item["value"]));
				break;
			}
		}
	}
}

void CTuyaLocal::PollLan()
{
	const auto pollDevice = [this](const std::string &id, const std::string &key, const std::string &address,
		const std::string &dbId, const std::string &name) {
		CTuyaLanClient client(id, key, m_protocol);
		Json::Value dps;
		std::string error;
		if (!client.Query(address, m_port, dps, error))
		{
			Log(LOG_ERROR, "TinyTuya LAN query failed for %s: %s", name.c_str(), error.c_str());
			return;
		}
		const std::string dp = std::to_string(m_switchDp);
		if (!dps.isMember(dp))
		{
			Log(LOG_ERROR, "TinyTuya response from %s does not contain configured switch DP %s", name.c_str(), dp.c_str());
			return;
		}
		UpdateSwitch(dbId, name, IsSwitchOn(dps[dp]));
	};
	if (m_cloudConfigured)
	{
		for (const auto &device : m_cloudDevices)
			pollDevice(device.cloud.id, device.cloud.localKey, device.cloud.ip, device.domoticzId, device.cloud.name);
		return;
	}
	pollDevice(m_deviceId, m_localKey, m_address, m_deviceId, m_Name);
}

bool CTuyaLocal::WriteToHardware(const char *pdata, const unsigned char /*length*/)
{
	const tRBUF *packet = reinterpret_cast<const tRBUF *>(pdata);
	const _tGeneralSwitch *command = nullptr;
	bool value;
	if (packet->ICMND.packettype == pTypeGeneralSwitch)
	{
		command = reinterpret_cast<const _tGeneralSwitch *>(pdata);
		if (command->unitcode != TUYA_LOCAL_UNIT_SWITCH)
			return false;
		if (command->cmnd == gswitch_sOn || command->cmnd == gswitch_sGroupOn)
			value = true;
		else if (command->cmnd == gswitch_sOff || command->cmnd == gswitch_sGroupOff)
			value = false;
		else
			return false;
	}
	else
		return false;

	if (m_cloudConfigured)
	{
		const auto device = std::find_if(m_cloudDevices.begin(), m_cloudDevices.end(), [command](const ManagedDevice &candidate) {
			return MakeCommandId(candidate.domoticzId) == command->id;
		});
		if (device == m_cloudDevices.end())
			return false;
		std::string error;
		if (!m_cloud || !m_cloud->SendCommand(device->cloud.id, device->switchCode, Json::Value(value), error))
		{
			Log(LOG_ERROR, "TinyTuya Cloud command failed for %s: %s", device->cloud.name.c_str(), error.c_str());
			return false;
		}
		UpdateSwitch(device->domoticzId, device->cloud.name, value);
		m_secondsToPoll = 1;
		return true;
	}
	CTuyaLanClient client(m_deviceId, m_localKey, m_protocol);
	std::string error;
	if (!client.SetDP(m_address, m_port, m_switchDp, Json::Value(value), error))
	{
		Log(LOG_ERROR, "TinyTuya LAN command failed: %s", error.c_str());
		return false;
	}
	UpdateSwitch(m_deviceId, m_Name, value);
	m_secondsToPoll = 1;
	return true;
}
