#include "iotsaRequest.h"
#include <base64.h>
#ifdef ESP32
#include <HTTPClient.h>
#else
#include <ESP8266HTTPClient.h>
#endif

#ifdef ESP32
#define SSL_INFO_NAME "rootCA"
#else
#define SSL_INFO_NAME "fingerprint"
#endif


void IotsaRequest::configLoad(IotsaConfigFileLoad& cf, String& name) {
    cf.get(name+"url", url, "");
    cf.get(name + SSL_INFO_NAME, sslInfo, "");
    cf.get(name + "credentials", credentials, "");
    cf.get(name + "token", token, "");
}

void IotsaRequest::configSave(IotsaConfigFileSave& cf, String& name) {
    cf.put(name + "url", url);
    cf.put(name + SSL_INFO_NAME, sslInfo);
    cf.put(name + "credentials", credentials);
    cf.put(name + token, token);
}

void IotsaRequest::formHandler(String& message, String& text, String& name) {  
    message += "<em>" + text +  "</em><br>\n";
    message += "Activation URL: <input name='" + name +  "url' value='";
    message += url;
    message += "'><br>\n";
#ifdef ESP32
    message += "Root CA cert <i>(https only)</i>: <input name='" + name + "rootCA' value='";
    message += sslInfo;
#else
    message += "Fingerprint <i>(https only)</i>: <input name='" + name +  "fingerprint' value='";
    message += sslInfo;
#endif
    message += "'><br>\n";

    message += "Bearer token <i>(optional)</i>: <input name='" + name + "token' value='";
    message += token;
    message += "'><br>\n";

    message += "Credentials <i>(optional, user:pass)</i>: <input name='" + name + "credentials' value='";
    message += credentials;
    message += "'><br>\n";
}

#ifdef IOTSA_WITH_WEB
bool IotsaRequest::formArgHandler(IotsaWebServer *server, String name) {
  bool any = false;
  String wtdName = name + "url";
  if (server->hasArg(wtdName)) {
    IotsaMod::percentDecode(server->arg(wtdName), url);
    IFDEBUG IotsaSerial.print(wtdName);
    IFDEBUG IotsaSerial.print("=");
    IFDEBUG IotsaSerial.println(url);
    any = true;
  }
  wtdName = name + SSL_INFO_NAME;
  if (server->hasArg(wtdName)) {
    IotsaMod::percentDecode(server->arg(wtdName), sslInfo);
    IFDEBUG IotsaSerial.print(wtdName);
    IFDEBUG IotsaSerial.print("=");
    IFDEBUG IotsaSerial.println(sslInfo);
    any = true;
  }
  wtdName = name + "credentials";
  if (server->hasArg(wtdName)) {
    IotsaMod::percentDecode(server->arg(wtdName), credentials);
    IFDEBUG IotsaSerial.print(wtdName);
    IFDEBUG IotsaSerial.print("=");
    IFDEBUG IotsaSerial.println(credentials);
    any = true;
    }
  wtdName = name + "token";
  if (server->hasArg(wtdName)) {
    IotsaMod::percentDecode(server->arg(wtdName), token);
    IFDEBUG IotsaSerial.print(wtdName);
    IFDEBUG IotsaSerial.print("=");
    IFDEBUG IotsaSerial.println(token);
    any = true;
  }
  return any;
}
#endif // IOTSA_WITH_WEB

bool IotsaRequest::send() {
  bool rv = true;
  HTTPClient http;
  WiFiClient client;
#ifdef ESP32
  WiFiClientSecure secureClient;
#else
  BearSSL::WiFiClientSecure *secureClientPtr = NULL;
#endif

  if (url.startsWith("https:")) {
#ifdef ESP32
    secureClient.setCACert(sslInfo.c_str());
    rv = http.begin(secureClient, url);
#else
    secureClientPtr = new BearSSL::WiFiClientSecure();
    secureClientPtr->setFingerprint(sslInfo.c_str());

    rv = http.begin(*secureClientPtr, url);
#endif
  } else {
    rv = http.begin(client, url);  
  }
  if (!rv) {
#ifndef ESP32
    if (secureClientPtr) delete secureClientPtr;
#endif
    return false;
  }
  if (token != "") {
    http.addHeader("Authorization", "Bearer " + token);
  }

  if (credentials != "") {
  	String cred64 = base64::encode(credentials);
    http.addHeader("Authorization", "Basic " + cred64);
  }
  int code = http.GET();
  if (code >= 200 && code <= 299) {
    IFDEBUG IotsaSerial.print(code);
    IFDEBUG IotsaSerial.print(" OK GET ");
    IFDEBUG IotsaSerial.println(url);
  } else {
    IFDEBUG IotsaSerial.print(code);
    IFDEBUG IotsaSerial.print(" FAIL GET ");
    IFDEBUG IotsaSerial.print(url);
#ifdef ESP32
    IFDEBUG IotsaSerial.print(", RootCA ");
#else
    IFDEBUG IotsaSerial.print(", fingerprint ");
#endif
    IFDEBUG IotsaSerial.println(sslInfo);
    rv = false;
  }
  http.end();
#ifndef ESP32
  if (secureClientPtr) delete secureClientPtr;
#endif
  return rv;
}

#ifdef IOTSA_WITH_API
void IotsaRequest::getHandler(JsonObject& reply) {
  reply["url"] = url;
  reply[SSL_INFO_NAME] = sslInfo;
  reply["hasCredentials"] = credentials != "";
  reply["hasToken"] = token != "";
}

bool IotsaRequest::putHandler(const JsonVariant& request) {
  if (!request.is<JsonObject>()) return false;
  bool any = false;
  const JsonObject& reqObj = request.as<JsonObject>();
  if (reqObj.containsKey("url")) {
    any = true;
    url = reqObj["url"].as<String>();
  }
  if (reqObj.containsKey(SSL_INFO_NAME)) {
    any = true;
    sslInfo = reqObj[SSL_INFO_NAME].as<String>();
  }
  if (reqObj.containsKey("credentials")) {
    any = true;
    credentials = reqObj["credentials"].as<String>();
  }
  if (reqObj.containsKey("token")) {
    any = true;
    token = reqObj["token"].as<String>();
  }
  return any;
}
#endif // IOTSA_WITH_API