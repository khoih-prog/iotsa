#include <ESP.h>
#include <ESP8266mDNS.h>

#include "Wapp.h"
#include "WappConfigFile.h"
#include "WappWifi.h"

#define WIFI_TIMEOUT 20                  // How long to wait for our WiFi network to appear

//
// Global variables, because other modules need them too.
//
String hostName;  // mDNS hostname, and also used for AP network name if needed.
bool configurationMode;        // True if we have no config, and go into AP mode
config_mode  tempConfigurationMode;    // True if we go into AP mode for a limited time
unsigned long tempConfigurationModeTimeout;  // When we reboot out of temp configuration mode


static void wifiDefaultHostName() {
  hostName = "esp8266-";
  hostName += String(ESP.getChipId());
}

void WappWifiMod::setup() {
  configLoad();
  if (ssid.length() && tempConfigurationMode != TMPC_CONFIG) {
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid.c_str(), ssidPassword.c_str());
    IFDEBUG Serial.println("");
  
    // Wait for connection
    int count = WIFI_TIMEOUT;
    while (WiFi.status() != WL_CONNECTED && count > 0) {
      delay(1000);
      IFDEBUG Serial.print(".");
      count--;
    }
    if (count) {
      // Connection to WiFi network succeeded.
      IFDEBUG Serial.println("");
      IFDEBUG Serial.print("Connected to ");
      IFDEBUG Serial.println(ssid);
      IFDEBUG Serial.print("IP address: ");
      IFDEBUG Serial.println(WiFi.localIP());
      IFDEBUG Serial.print("Hostname ");
      IFDEBUG Serial.println(hostName);
  
      if (MDNS.begin(hostName.c_str())) {
        MDNS.addService("http", "tcp", 80);
        IFDEBUG Serial.println("MDNS responder started");
        haveMDNS = true;
      }
      return;
    }
    IFDEBUG Serial.print("Cannot join ");
    IFDEBUG Serial.println(ssid);
    tempConfigurationMode = TMPC_CONFIG;
  }
  
  // Connection to WiFi network failed, or we are in temp cofiguration mode. Setup our own network.
  if (tempConfigurationMode) {
    tempConfigurationModeTimeout = millis() + 1000*CONFIGURATION_MODE_TIMEOUT;
  }
  configurationMode = true;
  String networkName = "config-" + hostName;
  WiFi.mode(WIFI_AP);
  WiFi.softAP(networkName.c_str());
  IFDEBUG Serial.print("\nCreating softAP for network ");
  IFDEBUG Serial.println(networkName);
  IFDEBUG Serial.print("IP address: ");
  IFDEBUG Serial.println(WiFi.softAPIP());
#if 0
  // Despite reports to the contrary it seems mDNS isn't working in softAP mode
  if (MDNS.begin(hostName.c_str())) {
    MDNS.addService("http", "tcp", 80);
    IFDEBUG Serial.println("MDNS responder started");
    haveMDNS = true;
  }
#endif
}

void
WappWifiMod::handler() {
  LED digitalWrite(led, 1);
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
    if( server.argName(i) == "hostName") {
      hostName = server.arg(i);
      anyChanged = true;
    }
    if (anyChanged) configSave();
  }
  String message = "<html><head><title>WiFi configuration</title></head><body><h1>WiFi configuration</h1>";
  if (anyChanged) {
    message += "<p>Settings saved to EEPROM. <em>Rebooting device to activate new settings.</em></p>";
  }
  message += "<form method='get'>Network: <input name='ssid' value='";
  message += ssid;
  message += "'><br>Password: <input name='ssidPassword' value='";
  message += ssidPassword;
  message += "'><br>Hostname: <input name='hostName' value='";
  message += hostName;
  message += "'><br><input type='submit'></form>";
  message += "</body></html>";
  server.send(200, "text/html", message);
  LED digitalWrite(led, 0);
  if (anyChanged) {
    IFDEBUG Serial.print("Restart in 2 seconds");
    delay(2000);
    ESP.restart();
  }
}

void
WappWifiMod::handlerConfigmode() {
  LED digitalWrite(led, 1);
  bool anyChanged = false;
  for (uint8_t i=0; i<server.args(); i++) {
    if( server.argName(i) == "config") {
      tempConfigurationMode = config_mode(atoi(server.arg(i).c_str()));
      anyChanged = true;
    }
    if (anyChanged) configSave();
  }
  String message = "<html><head><title>WiFi configuration</title></head><body><h1>WiFi configuration</h1>";
  if (anyChanged) {
    if (tempConfigurationMode == TMPC_CONFIG) {
      message += "<p><em>Set configuration mode here, then power-cycle device to activate configuration mode for 2 minutes.</em></p>";
      message += "<p><em>Connect to WiFi network " + hostName + ", device 192.168.4.1 to change settings during that configuration period. </em></p>";
#ifdef WITH_OTA
    } else if (tempConfigurationMode == TMPC_OTA) {
      message += "<p><em>Power-cycle device to activate OTA update mode for 2 minutes.</em></p>";
#endif
    } else {
      message += "<p><em>Settings saved to EEPROM, device will boot to normal operation.</em></p>";
    }
  }
  message += "<form method='get'><input name='config' type='checkbox' value='1'> Enter configuration mode after next reboot.<br>";
#ifdef WITH_OTA
  message += "<form method='get'><input name='config' type='checkbox' value='2'> Enable over-the-air update after next reboot.</br>";
#endif
  message += "<br><input type='submit'></form></body></html>";
  server.send(200, "text/html", message);
  LED digitalWrite(led, 0);
}

void WappWifiMod::serverSetup() {
//  server.on("/hello", std::bind(&WappWifiMod::handler, this));
  if (!configurationMode) {
    server.on("/wificonfig", std::bind(&WappWifiMod::handlerConfigmode, this));
  } else {
    IFDEBUG Serial.println("Enable config mode");
    server.on("/wificonfig", std::bind(&WappWifiMod::handler, this));
  }
}

String WappWifiMod::info() {
  IPAddress x;
  String message = "<p>IP address is ";
  uint32_t ip = WiFi.localIP();
  message += String(ip&0xff) + "." + String((ip>>8)&0xff) + "." + String((ip>>16)&0xff) + "." + String((ip>>24)&0xff);
  if (haveMDNS) {
    message += ", hostname is ";
    message += hostName;
    message += ".local. ";
  } else {
    message += " (no mDNS, so no hostname). ";
  }
  message += "See <a href=\"/wificonfig\">/wificonfig</a> to change network parameters.</p>";
  return message;
}

void WappWifiMod::configLoad() {
  WapConfigFileLoad cf("/data/wifi.cfg");
  int tcm;
  cf.get("mode", tcm, (int)TMPC_CONFIG);
  tempConfigurationMode = (config_mode)tcm;
  cf.get("ssid", ssid, "");
  cf.get("ssidPassword", ssidPassword, "");
  cf.get("hostName", hostName, "");
  if (hostName == "") wifiDefaultHostName();
 
}

void WappWifiMod::configSave() {
  WapConfigFileSave cf("/data/wifi.cfg");
  cf.put("mode", tempConfigurationMode);
  cf.put("ssid", ssid);
  cf.put("ssidPassword", ssidPassword);
  cf.put("hostName", hostName);
  IFDEBUG Serial.println("Saved wifi.cfg");
}

void WappWifiMod::loop() {
  if (tempConfigurationModeTimeout && millis() > tempConfigurationModeTimeout) {
    IFDEBUG Serial.println("Configuration mode timeout. reboot.");
    tempConfigurationMode = TMPC_NORMAL;
    configSave();
    ESP.restart();
  }
  if (haveMDNS) MDNS.update();

}
