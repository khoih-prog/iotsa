#include <ESP.h>
#include <user_interface.h>
#ifdef ESP32
#include <ESPmDNS.h>
#include <SPIFFS.h>
#include <esp_log.h>
#else
#include <ESP8266mDNS.h>
#endif
#include <FS.h>

#include "iotsa.h"
#include "iotsaConfigFile.h"
#include "iotsaWifi.h"


void IotsaWifiMod::setup() {
  configLoad();
  if (app.status) app.status->showStatus();

  // Try and connect to an existing Wifi network, if known
  if (ssid.length()) {
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid.c_str(), ssidPassword.c_str());
    WiFi.setAutoConnect(false);
    WiFi.setAutoReconnect(false);
    IFDEBUG IotsaSerial.println("");
  
    // Wait for connection
    int count = WIFI_TIMEOUT;
    while (WiFi.status() != WL_CONNECTED && count > 0) {
      delay(1000);
      IFDEBUG IotsaSerial.print(".");
      count--;
    }
    if (count) {
      // Connection to WiFi network succeeded.
      IFDEBUG IotsaSerial.println("");
      IFDEBUG IotsaSerial.print("Connected to ");
      IFDEBUG IotsaSerial.println(ssid);
      IFDEBUG IotsaSerial.print("IP address: ");
      IFDEBUG IotsaSerial.println(WiFi.localIP());
      IFDEBUG IotsaSerial.print("Hostname ");
      IFDEBUG IotsaSerial.println(iotsaConfig.hostName);
      
      WiFi.setAutoReconnect(true);

      if (MDNS.begin(iotsaConfig.hostName.c_str())) {
        MDNS.addService("http", "tcp", 80);
        MDNS.addService("iotsa", "tcp", 80);
        IFDEBUG IotsaSerial.println("MDNS responder started");
        haveMDNS = true;
      }
      if (iotsaConfig.configurationMode) {
        iotsaConfig.configurationModeEndTime = millis() + 1000*iotsaConfig.configurationModeTimeout;
        IFDEBUG IotsaSerial.print("tempConfigMode=");
        IFDEBUG IotsaSerial.print((int)iotsaConfig.configurationMode);
        IFDEBUG IotsaSerial.print(", timeout at ");
        IFDEBUG IotsaSerial.println(iotsaConfig.configurationModeEndTime);
      }
      if (app.status) app.status->showStatus();
      return;
    }
    iotsaConfig.wifiPrivateNetworkMode = true;
    privateNetworkModeReason = WiFi.status();
    IFDEBUG IotsaSerial.print("Cannot join ");
    IFDEBUG IotsaSerial.print(ssid);
    IFDEBUG IotsaSerial.print(", status=");
    IFDEBUG IotsaSerial.println(privateNetworkModeReason);
  }
  if (app.status) app.status->showStatus();
  
  // Make sure we also end configurationMode on a private network
  if (iotsaConfig.configurationMode) {
    iotsaConfig.configurationModeEndTime = millis() + 1000*iotsaConfig.configurationModeTimeout;
  	IFDEBUG IotsaSerial.print("tempConfigMode=");
  	IFDEBUG IotsaSerial.print((int)iotsaConfig.configurationMode);
  	IFDEBUG IotsaSerial.print(", timeout at ");
  	IFDEBUG IotsaSerial.println(iotsaConfig.configurationModeEndTime);
  }
  iotsaConfig.wifiPrivateNetworkMode = true; // xxxjack
  String networkName = "config-" + iotsaConfig.hostName;
  WiFi.mode(WIFI_AP);
  WiFi.softAP(networkName.c_str());
  WiFi.setAutoConnect(false);
  WiFi.setAutoReconnect(false);
  IFDEBUG IotsaSerial.print("\nCreating softAP for network ");
  IFDEBUG IotsaSerial.println(networkName);
  IFDEBUG IotsaSerial.print("IP address: ");
  IFDEBUG IotsaSerial.println(WiFi.softAPIP());
#if 0
  // Despite reports to the contrary it seems mDNS isn't working in softAP mode
  if (MDNS.begin(hostName.c_str())) {
    MDNS.addService("http", "tcp", 80);
    IFDEBUG IotsaSerial.println("MDNS responder started");
    haveMDNS = true;
  }
#endif
  if (app.status) app.status->showStatus();
}

void
IotsaWifiMod::handler() {
  bool anyChanged = false;
  for (uint8_t i=0; i<server.args(); i++){
    if( server.argName(i) == "ssid") {
      ssid = server.arg(i);
      anyChanged = true;
    }
    if( server.argName(i) == "ssidPassword") {
      ssidPassword = server.arg(i);
      anyChanged = true;
    }
    if (anyChanged) {
    	configSave();
  }
  String message = "<html><head><title>WiFi configuration</title></head><body><h1>WiFi configuration</h1>";
  if (anyChanged) {
    message += "<p>Settings saved to EEPROM. <em>Rebooting device to activate new settings.</em></p>";
  }
  message += "<form method='get'>Network: <input name='ssid' value='";
  message += htmlEncode(ssid);
  message += "'><br>Password: <input name='ssidPassword' value='";
  message += htmlEncode(ssidPassword);
  message += "'><br><input type='submit'></form>";
  message += "</body></html>";
  server.send(200, "text/html", message);
  if (anyChanged) {
    if (app.status) app.status->showStatus();
    IFDEBUG IotsaSerial.print("Restart in 2 seconds");
    delay(2000);
    ESP.restart();
  }
}

void
IotsaWifiMod::handlerNormalMode() {
  bool anyChanged = false;
  for (uint8_t i=0; i<server.args(); i++) {
  }
  if (anyChanged) {
	if (app.status) app.status->showStatus();
	configSave();
  }
  String message = "<html><head><title>WiFi configuration</title></head><body><h1>WiFi configuration</h1>";
  if (iotsaConfig.nextConfigurationMode == IOTSA_MODE_CONFIG) {
  	message += "<p><em>Power-cycle device within " + String((iotsaConfig.nextConfigurationModeEndTime-millis())/1000) + " seconds to activate configuration mode for " + String(iotsaConfig.configurationModeTimeout) + " seconds.</em></p>";
  	message += "<p><em>Connect to WiFi network config-" + htmlEncode(iotsaConfig.hostName) + ", device 192.168.4.1 to change settings during that configuration period. </em></p>";
  } else if (iotsaConfig.nextConfigurationMode == IOTSA_MODE_OTA) {
  	message += "<p><em>Power-cycle device within " + String((iotsaConfig.nextConfigurationModeEndTime-millis())/1000) + "seconds to activate OTA mode for " + String(iotsaConfig.configurationModeTimeout) + " seconds.</em></p>";
  } else if (iotsaConfig.nextConfigurationMode == IOTSA_MODE_FACTORY_RESET) {
  	message += "<p><em>Power-cycle device within " + String((iotsaConfig.nextConfigurationModeEndTime-millis())/1000) + "seconds to reset to factory settings.</em></p>";
  }
  message += "<form method='get'><input name='config' type='checkbox' value='1'> Enter configuration mode after next reboot.<br>";
  if (app.otaEnabled()) {
	  message += "<input name='config' type='checkbox' value='2'> Enable over-the-air update after next reboot.</br>";
  }
  message += "<input name='factoryreset' type='checkbox' value='1'> Factory-reset and clear all files. <input name='iamsure' type='checkbox' value='1'> Yes, I am sure.</br>";
  message += "<br><input type='submit'></form></body></html>";
  server.send(200, "text/html", message);
}

bool IotsaWifiMod::getHandler(const char *path, JsonObject& reply) {
  reply["hostName"] = iotsaConfig.hostName;
  reply["rebootTimeout"] = iotsaConfig.configurationModeTimeout;
  if (iotsaConfig.wifiPrivateNetworkMode) {
    reply["ssid"] = ssid;
    reply["ssidPassword"] = ssidPassword;
  } else {
    if (iotsaConfig.nextConfigurationMode) {
      reply["requestedMode"] = iotsaConfig.nextConfigurationMode;
      reply["timeout"] = (iotsaConfig.nextConfigurationModeEndTime - millis())/1000;
    }
  }
  return true;
}

bool IotsaWifiMod::putHandler(const char *path, const JsonVariant& request, JsonObject& reply) {
  bool anyChanged = false;
  JsonObject& reqObj = request.as<JsonObject>();
  if (iotsaConfig.wifiPrivateNetworkMode) {
    if (reqObj.containsKey("ssid")) {
      ssid = reqObj.get<String>("ssid");
      anyChanged = true;
    }
    if (reqObj.containsKey("ssidPassword")) {
      ssid = reqObj.get<String>("ssidPassword");
      anyChanged = true;
    }
    if (reqObj.containsKey("hostName")) {
      ssid = reqObj.get<String>("hostName");
      anyChanged = true;
    }
    if (reqObj.containsKey("rebootTimeout")) {
      ssid = reqObj.get<int>("rebootTimeout");
      anyChanged = true;
    }
  } else {
    if (reqObj.containsKey("requestedMode")) {
      iotsaConfig.nextConfigurationMode = config_mode(reqObj.get<int>("requestedMode"));
      anyChanged = iotsaConfig.nextConfigurationMode != config_mode(0);
      if (anyChanged) {
        iotsaConfig.nextConfigurationModeEndTime = millis() + iotsaConfig.configurationModeTimeout*1000;
        reply["requestedMode"] = iotsaConfig.nextConfigurationMode;
        reply["timeout"] = (iotsaConfig.nextConfigurationModeEndTime - millis())/1000;
      }
    }
  }
  if (anyChanged) configSave();
  return anyChanged;
}

void IotsaWifiMod::serverSetup() {
//  server.on("/hello", std::bind(&IotsaWifiMod::handler, this));
  if (iotsaConfig.wifiPrivateNetworkMode) {
    server.on("/wificonfig", std::bind(&IotsaWifiMod::handlerConfigMode, this));
  } else {
    server.on("/wificonfig", std::bind(&IotsaWifiMod::handlerNormalMode, this));
  }
  api.setup("/api/wificonfig", true, true);
}

String IotsaWifiMod::info() {
  IPAddress x;
  String message = "<p>IP address is ";
  uint32_t ip = WiFi.localIP();
  if (ip == 0) {
  	ip = WiFi.softAPIP();
  }
  message += String(ip&0xff) + "." + String((ip>>8)&0xff) + "." + String((ip>>16)&0xff) + "." + String((ip>>24)&0xff);
  if (haveMDNS) {
    message += ", hostname is ";
    message += htmlEncode(iotsaConfig.hostName);
    message += ".local. ";
  } else {
    message += " (no mDNS, so no hostname). ";
  }
  message += "See <a href=\"/wificonfig\">/wificonfig</a> to change network parameters.</p>";
  if (iotsaConfig.configurationMode) {
  	message += "<p>In configuration mode " + String((int)iotsaConfig.configurationMode) + "(reason: " + String(privateNetworkModeReason) + "), will timeout in " + String((iotsaConfig.configurationModeEndTime-millis())/1000) + " seconds.</p>";
  } else if (iotsaConfig.nextConfigurationMode) {
  	message += "<p>Configuration mode " + String((int)iotsaConfig.nextConfigurationMode) + " requested, enable by rebooting within " + String((iotsaConfig.nextConfigurationModeEndTime-millis())/1000) + " seconds.</p>";
  } else if (iotsaConfig.configurationModeEndTime) {
  	message += "<p>Strange, no configuration mode but timeout is " + String(iotsaConfig.configurationModeEndTime-millis()) + "ms.</p>";
  }
  message += "<p>Last boot " + String((int)millis()/1000) + " seconds ago, reason ";
  rst_info *rip = ESP.getResetInfoPtr();
  static const char *reasons[] = {
    "power",
    "hardwareWatchdog",
    "exception",
    "softwareWatchdog",
    "softwareReboot",
    "deepSleepAwake",
    "externalReset"
  };
  const char *reason = "unknown";
  if ((int)rip->reason < sizeof(reasons)/sizeof(reasons[0])) {
    reason = reasons[(int)rip->reason];
  }
  message += reason;
  message += " ("+String((int)rip->reason)+").</p>";
  return message;
}

void IotsaWifiMod::configLoad() {
  IotsaConfigFileLoad cf("/config/wifi.cfg");
  int tcm;
  cf.get("mode", tcm, (int)IOTSA_MODE_NORMAL);
  iotsaConfig.configurationMode = (config_mode)tcm;
  cf.get("ssid", ssid, "");
  cf.get("ssidPassword", ssidPassword, "");
  cf.get("hostName", iotsaConfig.hostName, "");
  cf.get("rebootTimeout", iotsaConfig.configurationModeTimeout, CONFIGURATION_MODE_TIMEOUT);
  if (iotsaConfig.hostName == "") wifiDefaultHostName();
 
}

void IotsaWifiMod::configSave() {
  IotsaConfigFileSave cf("/config/wifi.cfg");
  cf.put("mode", iotsaConfig.nextConfigurationMode); // Note: nextConfigurationMode, which will be read as configurationMode
  cf.put("ssid", ssid);
  cf.put("ssidPassword", ssidPassword);
  cf.put("hostName", iotsaConfig.hostName);
  cf.put("rebootTimeout", iotsaConfig.configurationModeTimeout);
  IFDEBUG IotsaSerial.println("Saved wifi.cfg");
}

void IotsaWifiMod::loop() {
  if (iotsaConfig.configurationModeEndTime && millis() > iotsaConfig.configurationModeEndTime) {
    IFDEBUG IotsaSerial.println("Configuration mode timeout. reboot.");
    iotsaConfig.configurationMode = IOTSA_MODE_NORMAL;
    iotsaConfig.configurationModeEndTime = 0;
    ESP.restart();
  }
  if (iotsaConfig.nextConfigurationModeEndTime && millis() > iotsaConfig.nextConfigurationModeEndTime) {
    IFDEBUG IotsaSerial.println("Next configuration mode timeout. Clearing.");
    iotsaConfig.nextConfigurationMode = IOTSA_MODE_NORMAL;
    iotsaConfig.nextConfigurationModeEndTime = 0;
    configSave();
  }
#ifndef ESP32
  // mDNS happens asynchronously on ESP32
  if (haveMDNS) MDNS.update();
#endif
  // xxxjack
  if (!iotsaConfig.wifiPrivateNetworkMode) {
  	// Should be in normal mode, check that we have WiFi
  	static int disconnectedCount = 0;
  	if (WiFi.status() == WL_CONNECTED) {
  		if (disconnectedCount) {
  			IFDEBUG IotsaSerial.println("Wifi reconnected");
  		}
  		disconnectedCount = 0;
	} else {
		if (disconnectedCount == 0) {
			IFDEBUG IotsaSerial.println("Wifi connection lost");
		}
		disconnectedCount++;
		if (disconnectedCount > 60000) {
			IFDEBUG IotsaSerial.println("Wifi connection lost too long. Reboot.");
			ESP.restart();
		}
	}
  }
}
