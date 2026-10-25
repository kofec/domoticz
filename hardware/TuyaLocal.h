#pragma once

#include "DomoticzHardware.h"
#include "TuyaCloud.h"

#include <atomic>
#include <cstdint>
#include <memory>
#include <thread>
#include <vector>

class CTuyaLocal : public CDomoticzHardwareBase
{
public:
	CTuyaLocal(int ID, std::string address, unsigned short port, std::string deviceId, std::string localKey, int pollSeconds, int protocol, int switchDp);
	CTuyaLocal(int ID, std::string accessId, std::string accessSecret, std::string extra, int region, int cloudPollSeconds, int localRefreshSeconds, int debugLevel);
	~CTuyaLocal() override = default;

	bool WriteToHardware(const char *pdata, unsigned char length) override;

private:
	struct ManagedDevice
	{
		TuyaCloudDevice cloud;
		std::string domoticzId;
		std::string switchCode;
	};

	bool StartHardware() override;
	bool StopHardware() override;
	void Do_Work();
	void Poll();
	void PollCloud();
	void PollLan();
	void CreateDevices();
	void UpdateSwitch(const std::string &deviceId, const std::string &deviceName, bool on);
	bool IsSwitchOn(const Json::Value &value) const;
	static std::string MakeDomoticzId(const std::string &deviceId);
	static int32_t MakeCommandId(const std::string &domoticzId);

	std::string m_address;
	unsigned short m_port;
	std::string m_deviceId;
	std::string m_localKey;
	std::string m_accessId;
	std::string m_accessSecret;
	std::string m_searchDeviceId;
	int m_pollSeconds;
	int m_region = 0;
	int m_localRefreshSeconds = 0;
	int m_debugLevel = 0;
	std::unique_ptr<CTuyaCloudClient> m_cloud;
	std::vector<ManagedDevice> m_cloudDevices;
	double m_protocol;
	int m_switchDp;
	bool m_cloudConfigured = false;
	std::atomic<int> m_secondsToPoll{ 1 };
	std::atomic<int> m_secondsToLocalRefresh{ 1 };
	std::shared_ptr<std::thread> m_thread;
};
