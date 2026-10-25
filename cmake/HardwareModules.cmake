#
# Hardware driver selection
#
# By default every hardware driver is built. Configure with
#   -DINCLUDE_HARDWARE_ALL=OFF -DINCLUDE_HARDWARE_<NAME>=ON ...
# to compile and link only the listed drivers, for example a build that only
# needs the system monitor and the pinger (plus what is always built, below):
#   cmake -DINCLUDE_HARDWARE_ALL=OFF -DINCLUDE_HARDWARE_SYSTEM=ON
#         -DINCLUDE_HARDWARE_PINGER=ON ..
#
# Hardware of a type that is not built stays in the database, is not started
# (an error is logged) and the type is not offered when adding hardware.
#
# Always built, because the core uses them: Domoticz remote server, dummy
# hardware (it also serves createdevice), MQTT client gateway (MQTT push derives
# from it), the RFXCOM, P1, Teleinfo, MySensors and Evohome base classes.
# Python plugins follow USE_PYTHON and OpenZWave its own detection; Telldus
# (WITH_TELLDUSCORE) and TE923/Volcraft (WITH_LIBUSB) need both their switch
# and their module.
#
# Each module: dz_hardware_module(<NAME> TYPES <HTYPE_...> SOURCES <files>)
# A source shared by several modules is built when any of them is enabled.
#

option(INCLUDE_HARDWARE_ALL "Build all hardware drivers (OFF: only those with INCLUDE_HARDWARE_<NAME>=ON)" YES)

set(DZ_HW_MODULES)
macro(dz_hardware_module NAME)
  cmake_parse_arguments(_dzhw "" "" "TYPES;SOURCES" ${ARGN})
  list(APPEND DZ_HW_MODULES ${NAME})
  set(DZ_HW_${NAME}_TYPES ${_dzhw_TYPES})
  set(DZ_HW_${NAME}_SOURCES ${_dzhw_SOURCES})
endmacro()

dz_hardware_module(RFXCOM
  TYPES HTYPE_RFXtrx315 HTYPE_RFXtrx433 HTYPE_RFXtrx868 HTYPE_RFXLAN
  SOURCES hardware/RFXComSerial.cpp
          hardware/RFXComTCP.cpp)
dz_hardware_module(P1METER
  TYPES HTYPE_P1SmartMeter HTYPE_P1SmartMeterLAN
  SOURCES hardware/P1MeterSerial.cpp
          hardware/P1MeterTCP.cpp)
dz_hardware_module(YOULESS
  TYPES HTYPE_YouLess
  SOURCES hardware/YouLess.cpp)
dz_hardware_module(TE923
  TYPES HTYPE_TE923
  SOURCES hardware/TE923.cpp
          hardware/TE923Tool.cpp)
dz_hardware_module(VOLCRAFTCO20
  TYPES HTYPE_VOLCRAFTCO20
  SOURCES hardware/VolcraftCO20.cpp)
dz_hardware_module(REGO6XX
  TYPES HTYPE_Rego6XX
  SOURCES hardware/Rego6XXSerial.cpp)
dz_hardware_module(DAVISVANTAGE
  TYPES HTYPE_DavisVantage
  SOURCES hardware/DavisLoggerSerial.cpp)
dz_hardware_module(1WIRE
  TYPES HTYPE_1WIRE
  SOURCES hardware/1Wire.cpp
          hardware/1Wire/1WireByOWFS.cpp
          hardware/1Wire/1WireByKernel.cpp
          hardware/1Wire/1WireCommon.cpp
          hardware/1Wire/1WireForWindows.cpp)
dz_hardware_module(I2C
  TYPES HTYPE_RaspberryBMP085 HTYPE_RaspberryHTU21D HTYPE_RaspberryTSL2561 HTYPE_RaspberryPCF8574 HTYPE_RaspberryBME280 HTYPE_RaspberryMCP23017
  SOURCES hardware/I2C.cpp)
dz_hardware_module(WUNDERGROUND
  TYPES HTYPE_Wunderground
  SOURCES hardware/Wunderground.cpp)
dz_hardware_module(DARKSKY
  TYPES HTYPE_DarkSky
  SOURCES hardware/DarkSky.cpp)
dz_hardware_module(VISUALCROSSING
  TYPES HTYPE_VisualCrossing
  SOURCES hardware/VisualCrossing.cpp)
dz_hardware_module(SYSTEM
  TYPES HTYPE_System
  SOURCES hardware/HardwareMonitor.cpp
          hardware/HardwareMonitorUnix.cpp
          hardware/HardwareMonitorLHM.cpp)
dz_hardware_module(TELLSTICK
  TYPES HTYPE_Tellstick
  SOURCES hardware/Tellstick.cpp
          hardware/TellstickFactory.cpp)
dz_hardware_module(PIFACE
  TYPES HTYPE_PiFace
  SOURCES hardware/PiFace.cpp)
dz_hardware_module(S0METER
  TYPES HTYPE_S0SmartMeterUSB HTYPE_S0SmartMeterTCP
  SOURCES hardware/S0MeterBase.cpp
          hardware/S0MeterSerial.cpp
          hardware/S0MeterTCP.cpp)
dz_hardware_module(OTGW
  TYPES HTYPE_OpenThermGateway HTYPE_OpenThermGatewayTCP
  SOURCES hardware/OTGWBase.cpp
          hardware/OTGWSerial.cpp
          hardware/OTGWTCP.cpp)
dz_hardware_module(TELEINFO
  TYPES HTYPE_TeleinfoMeter HTYPE_TeleinfoMeterTCP
  SOURCES hardware/TeleinfoSerial.cpp
          hardware/TeleinfoTCP.cpp)
dz_hardware_module(LIMITLESS
  TYPES HTYPE_LimitlessLights
  SOURCES hardware/Limitless.cpp)
dz_hardware_module(MOCHAD
  TYPES HTYPE_Mochad
  SOURCES hardware/MochadTCP.cpp)
dz_hardware_module(ENOCEAN
  TYPES HTYPE_EnOceanESP2 HTYPE_EnOceanESP3
  SOURCES hardware/EnOceanEEP.cpp
          hardware/EnOceanESP2.cpp
          hardware/EnOceanESP3.cpp
          hardware/EnOceanRawValue.cpp)
dz_hardware_module(SBFSPOT
  TYPES HTYPE_SBFSpot
  SOURCES hardware/SBFSpot.cpp)
dz_hardware_module(PHILIPSHUE
  TYPES HTYPE_Philips_Hue
  SOURCES hardware/PhilipsHue/PhilipsHue.cpp
          hardware/PhilipsHue/PhilipsHueHelper.cpp
          hardware/PhilipsHue/PhilipsHueSensors.cpp
          hardware/PhilipsHue/PhilipsHueV2Sensors.cpp
          hardware/PhilipsHue/PhilipsHueSSE.cpp)
dz_hardware_module(ICYTHERMOSTAT
  TYPES HTYPE_ICYTHERMOSTAT
  SOURCES hardware/ICYThermostat.cpp)
dz_hardware_module(WOL
  TYPES HTYPE_WOL
  SOURCES hardware/WOL.cpp)
dz_hardware_module(METEOSTICK
  TYPES HTYPE_Meteostick
  SOURCES hardware/Meteostick.cpp)
dz_hardware_module(PVOUTPUT
  TYPES HTYPE_PVOUTPUT_INPUT
  SOURCES hardware/PVOutput_Input.cpp)
dz_hardware_module(TOONTHERMOSTAT
  TYPES HTYPE_TOONTHERMOSTAT
  SOURCES hardware/ToonThermostat.cpp)
dz_hardware_module(HARMONYHUB
  TYPES HTYPE_HARMONY_HUB
  SOURCES hardware/HarmonyHub.cpp)
dz_hardware_module(ECODEVICES
  TYPES HTYPE_ECODEVICES
  SOURCES hardware/EcoDevices.cpp)
dz_hardware_module(EVOHOME
  TYPES HTYPE_EVOHOME_SCRIPT HTYPE_EVOHOME_SERIAL HTYPE_EVOHOME_TCP HTYPE_EVOHOME_WEB
  SOURCES hardware/EvohomeRadio.cpp
          hardware/EvohomeScript.cpp
          hardware/EvohomeSerial.cpp
          hardware/EvohomeTCP.cpp
          hardware/EvohomeWeb.cpp)
dz_hardware_module(MYSENSORS
  TYPES HTYPE_MySensorsUSB HTYPE_MySensorsTCP HTYPE_MySensorsMQTT
  SOURCES hardware/MySensorsSerial.cpp
          hardware/MySensorsTCP.cpp
          hardware/MySensorsMQTT.cpp)
dz_hardware_module(MQTTAUTODISCOVERY
  TYPES HTYPE_MQTTAutoDiscovery
  SOURCES hardware/MQTTAutoDiscover.cpp)
dz_hardware_module(FRITZBOX
  TYPES HTYPE_FRITZBOX
  SOURCES hardware/FritzboxTCP.cpp)
dz_hardware_module(ETH8020
  TYPES HTYPE_ETH8020
  SOURCES hardware/ETH8020.cpp)
dz_hardware_module(RFLINK
  TYPES HTYPE_RFLINKUSB HTYPE_RFLINKTCP HTYPE_RFLINKMQTT
  SOURCES hardware/RFLinkBase.cpp
          hardware/RFLinkSerial.cpp
          hardware/RFLinkTCP.cpp
          hardware/RFLinkMQTT.cpp)
dz_hardware_module(KMTRONIC
  TYPES HTYPE_KMTronicUSB HTYPE_KMTronicTCP HTYPE_KMTronic433 HTYPE_KMTronicUDP
  SOURCES hardware/KMTronicBase.cpp
          hardware/KMTronic433.cpp
          hardware/KMTronicSerial.cpp
          hardware/KMTronicTCP.cpp
          hardware/KMTronicUDP.cpp)
dz_hardware_module(SOLARMAX
  TYPES HTYPE_SOLARMAXTCP
  SOURCES hardware/SolarMaxTCP.cpp)
dz_hardware_module(PINGER
  TYPES HTYPE_Pinger
  SOURCES hardware/Pinger.cpp)
dz_hardware_module(NEST
  TYPES HTYPE_NEST HTYPE_Nest_OAuthAPI
  SOURCES hardware/Nest.cpp
          hardware/NestOAuthAPI.cpp)
dz_hardware_module(TADO
  TYPES HTYPE_Tado
  SOURCES hardware/Tado.cpp)
dz_hardware_module(EVEHICLE
  TYPES HTYPE_Tesla HTYPE_Mercedes
  SOURCES hardware/eVehicles/TeslaApi.cpp
          hardware/eVehicles/MercApi.cpp
          hardware/eVehicles/TestcarApi.cpp
          hardware/eVehicles/eVehicle.cpp)
dz_hardware_module(KODI
  TYPES HTYPE_Kodi
  SOURCES hardware/Kodi.cpp)
dz_hardware_module(NETATMO
  TYPES HTYPE_Netatmo
  SOURCES hardware/Netatmo.cpp)
dz_hardware_module(HTTPPOLLER
  TYPES HTYPE_HTTPPOLLER
  SOURCES hardware/HttpPoller.cpp)
dz_hardware_module(ANNATHERMOSTAT
  TYPES HTYPE_ANNATHERMOSTAT
  SOURCES hardware/AnnaThermostat.cpp)
dz_hardware_module(WINDDELEN
  TYPES HTYPE_WINDDELEN
  SOURCES hardware/Winddelen.cpp)
dz_hardware_module(SATELINTEGRA
  TYPES HTYPE_SatelIntegra
  SOURCES hardware/SatelIntegra.cpp)
dz_hardware_module(LOGITECHMEDIASERVER
  TYPES HTYPE_LogitechMediaServer
  SOURCES hardware/LogitechMediaServer.cpp)
dz_hardware_module(COMM5
  TYPES HTYPE_Comm5TCP HTYPE_Comm5Serial HTYPE_Comm5SMTCP
  SOURCES hardware/Comm5TCP.cpp
          hardware/Comm5Serial.cpp
          hardware/Comm5SMTCP.cpp)
dz_hardware_module(CURRENTCOST
  TYPES HTYPE_CurrentCostMeter HTYPE_CurrentCostMeterLAN
  SOURCES hardware/CurrentCostMeterBase.cpp
          hardware/CurrentCostMeterSerial.cpp
          hardware/CurrentCostMeterTCP.cpp)
dz_hardware_module(SOLAREDGE
  TYPES HTYPE_SolarEdgeAPI
  SOURCES hardware/SolarEdgeAPI.cpp)
dz_hardware_module(NEFITEASY
  TYPES HTYPE_NefitEastLAN
  SOURCES hardware/NefitEasy.cpp)
dz_hardware_module(PANASONICTV
  TYPES HTYPE_PanasonicTV
  SOURCES hardware/PanasonicTV.cpp)
dz_hardware_module(OPENWEBNET
  TYPES HTYPE_OpenWebNetTCP HTYPE_OpenWebNetUSB
  SOURCES hardware/OpenWebNetTCP.cpp
          hardware/OpenWebNetUSB.cpp
          hardware/openwebnet/bt_openwebnet.cpp)
dz_hardware_module(ATAGONE
  TYPES HTYPE_AtagOne
  SOURCES hardware/AtagOne.cpp)
dz_hardware_module(STERBOX
  TYPES HTYPE_Sterbox
  SOURCES hardware/Sterbox.cpp)
dz_hardware_module(RAVEN
  TYPES HTYPE_RAVEn
  SOURCES hardware/RAVEn.cpp)
dz_hardware_module(DENKOVI
  TYPES HTYPE_DenkoviHTTPDevices HTYPE_DenkoviUSBDevices HTYPE_DenkoviTCPDevices
  SOURCES hardware/DenkoviDevices.cpp
          hardware/DenkoviUSBDevices.cpp
          hardware/DenkoviTCPDevices.cpp)
dz_hardware_module(ACCUWEATHER
  TYPES HTYPE_AccuWeather
  SOURCES hardware/AccuWeather.cpp)
dz_hardware_module(BLEBOX
  TYPES HTYPE_BleBox
  SOURCES hardware/BleBox.cpp)
dz_hardware_module(EC3KMETER
  TYPES HTYPE_Ec3kMeterTCP
  SOURCES hardware/Ec3kMeterTCP.cpp)
dz_hardware_module(OPENWEATHERMAP
  TYPES HTYPE_OpenWeatherMap
  SOURCES hardware/OpenWeatherMap.cpp)
dz_hardware_module(DAIKIN
  TYPES HTYPE_Daikin HTYPE_DaikinModbus
  SOURCES hardware/Daikin.cpp
          hardware/DaikinModbus.cpp)
dz_hardware_module(HEOS
  TYPES HTYPE_HEOS
  SOURCES hardware/HEOS.cpp)
dz_hardware_module(MULTIFUN
  TYPES HTYPE_MultiFun
  SOURCES hardware/MultiFun.cpp)
dz_hardware_module(ZIBLUE
  TYPES HTYPE_ZIBLUEUSB HTYPE_ZIBLUETCP
  SOURCES hardware/ZiBlueBase.cpp
          hardware/ZiBlueSerial.cpp
          hardware/ZiBlueTCP.cpp)
dz_hardware_module(YEELIGHT
  TYPES HTYPE_Yeelight
  SOURCES hardware/Yeelight.cpp)
dz_hardware_module(XIAOMIGATEWAY
  TYPES HTYPE_XiaomiGateway
  SOURCES hardware/XiaomiGateway.cpp
          hardware/XiaomiDeviceSupport.cpp)
dz_hardware_module(ARILUX
  TYPES HTYPE_Arilux
  SOURCES hardware/Arilux.cpp)
dz_hardware_module(INCOMFORT
  TYPES HTYPE_IntergasInComfortLAN2RF
  SOURCES hardware/InComfort.cpp)
dz_hardware_module(RELAYNET
  TYPES HTYPE_RelayNet
  SOURCES hardware/RelayNet.cpp)
dz_hardware_module(GPIO
  TYPES HTYPE_RaspberryGPIO HTYPE_SysfsGpio
  SOURCES hardware/Gpio.cpp
          hardware/GpioPin.cpp
          hardware/SysfsGpio.cpp)
dz_hardware_module(RTL433
  TYPES HTYPE_Rtl433
  SOURCES hardware/Rtl433.cpp)
dz_hardware_module(ONKYOAV
  TYPES HTYPE_OnkyoAVTCP
  SOURCES hardware/OnkyoAVTCP.cpp)
dz_hardware_module(USBTIN
  TYPES HTYPE_USBtinGateway
  SOURCES hardware/USBtin.cpp
          hardware/USBtin_MultiblocV8.cpp)
dz_hardware_module(ENPHASE
  TYPES HTYPE_EnphaseAPI
  SOURCES hardware/EnphaseAPI.cpp)
dz_hardware_module(EHOUSE
  TYPES HTYPE_eHouseTCP
  SOURCES hardware/eHouseTCP.cpp
          hardware/eHouse/EhouseEvents.cpp
          hardware/eHouse/EhouseTcpClient.cpp
          hardware/eHouse/EhouseUdpListener.cpp)
dz_hardware_module(ECOCOMPTEUR
  TYPES HTYPE_EcoCompteur
  SOURCES hardware/EcoCompteur.cpp)
dz_hardware_module(HONEYWELL
  TYPES HTYPE_Honeywell
  SOURCES hardware/Honeywell.cpp)
dz_hardware_module(TTNMQTT
  TYPES HTYPE_TTN_MQTT
  SOURCES hardware/TTNMQTT.cpp
          hardware/cayenne_lpp/CayenneLPP_Dec.cpp)
dz_hardware_module(BUIENRADAR
  TYPES HTYPE_BuienRadar
  SOURCES hardware/Buienradar.cpp)
dz_hardware_module(OCTOPRINT
  TYPES HTYPE_OctoPrint
  SOURCES hardware/OctoPrintMQTT.cpp)
dz_hardware_module(METEOROLOGISK
  TYPES HTYPE_Meteorologisk
  SOURCES hardware/Meteorologisk.cpp)
dz_hardware_module(OPENMETEO
  TYPES HTYPE_OpenMeteo
  SOURCES hardware/OpenMeteo.cpp)
dz_hardware_module(AIRCONWITHME
  TYPES HTYPE_AirconWithMe
  SOURCES hardware/AirconWithMe.cpp)
dz_hardware_module(ALFENEVE
  TYPES HTYPE_AlfenEveCharger
  SOURCES hardware/AlfenEve.cpp)
dz_hardware_module(ENEVER
  TYPES HTYPE_EneverPriceFeeds
  SOURCES hardware/Enever.cpp)
dz_hardware_module(MITSUBISHIWF
  TYPES HTYPE_MitsubishiWF
  SOURCES hardware/MitsubishiWF.cpp)
dz_hardware_module(MATTER
  TYPES HTYPE_Matter
  SOURCES hardware/Matter.cpp
          hardware/ASyncWebsocket.cpp)
dz_hardware_module(ENIGMA2
  TYPES HTYPE_Enigma2
  SOURCES hardware/Enigma2.cpp)
dz_hardware_module(TUYALAN
  TYPES HTYPE_TuyaLocal
  SOURCES hardware/TuyaCloud.cpp hardware/TuyaLan.cpp hardware/TuyaLocal.cpp)

set(_dzhw_built)
set(_dzhw_skipped)
set(_dzhw_keep_sources)
set(_dzhw_drop_sources)
set(_dzhw_excluded_types)
foreach(_dzhw ${DZ_HW_MODULES})
  option(INCLUDE_HARDWARE_${_dzhw} "Build hardware driver ${_dzhw}" ${INCLUDE_HARDWARE_ALL})
  IF(INCLUDE_HARDWARE_${_dzhw})
    add_definitions(-DWITH_HW_${_dzhw})
    list(APPEND _dzhw_built ${_dzhw})
    list(APPEND _dzhw_keep_sources ${DZ_HW_${_dzhw}_SOURCES})
  ELSE()
    list(APPEND _dzhw_skipped ${_dzhw})
    list(APPEND _dzhw_drop_sources ${DZ_HW_${_dzhw}_SOURCES})
    list(APPEND _dzhw_excluded_types ${DZ_HW_${_dzhw}_TYPES})
  ENDIF()
endforeach()

IF(_dzhw_drop_sources AND _dzhw_keep_sources)
  list(REMOVE_ITEM _dzhw_drop_sources ${_dzhw_keep_sources})
ENDIF()
IF(_dzhw_drop_sources)
  list(REMOVE_ITEM domoticz_SRCS ${_dzhw_drop_sources})
ENDIF()

IF(_dzhw_skipped)
  list(LENGTH _dzhw_built _dzhw_n)
  message(STATUS "Hardware drivers built (${_dzhw_n}): ${_dzhw_built}")
  list(LENGTH _dzhw_skipped _dzhw_n)
  message(STATUS "Hardware drivers NOT built (${_dzhw_n}): ${_dzhw_skipped}")
ENDIF()

# GPIO support is also switched on by WITH_GPIO, which may come in through the
# compiler flags; without the GPIO driver it has to go.
IF(NOT INCLUDE_HARDWARE_GPIO)
  string(REPLACE "-DWITH_GPIO" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
ENDIF()

# hardware_selection.h: the hardware types left out of this build, used to hide
# them in the "add hardware" list
string(REPLACE ";" ", " _dzhw_types "${_dzhw_excluded_types}")
IF(_dzhw_types)
  set(_dzhw_types "${_dzhw_types}, ")
ENDIF()
file(WRITE ${CMAKE_BINARY_DIR}/hardware_selection.h.tmp
  "// Generated by cmake/HardwareModules.cmake\n"
  "#pragma once\n"
  "#define DZ_HW_EXCLUDED_TYPES ${_dzhw_types}\n")
configure_file(${CMAKE_BINARY_DIR}/hardware_selection.h.tmp ${CMAKE_BINARY_DIR}/hardware_selection.h COPYONLY)
include_directories(${CMAKE_BINARY_DIR})
