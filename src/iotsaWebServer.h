#ifndef _IOTSAWEBSERVER_H_
#define _IOTSAWEBSERVER_H_
#include "iotsaBuildOptions.h"

//
// There are numerous different webserver implementations with very similar API.
// Attempt to select the correct one to use.
//
#if defined(ESP32) && defined(IOTSA_WITH_ESP32COMPAT) && defined(IOTSA_WITH_HTTPS)
#define IOTSA_WEBSERVER "esp32compatsecure"
#include <ESPWebServerSecure.hpp>
typedef ESPWebServer IotsaWebServer;

#elif defined(ESP32) && defined(IOTSA_WITH_ESP32COMPAT) && defined(IOTSA_WITH_HTTP)
#define IOTSA_WEBSERVER "esp32compat"
#include <ESPWebServer.hpp>
typedef ESPWebServer IotsaWebServer;

#elif defined(ESP32) && defined(IOTSA_WITH_HTTP) && 1
#define IOTSA_WEBSERVER "esp32webserver"
#include <WebServer.h>
typedef WebServer IotsaWebServer;

#elif defined(ESP32) && defined(IOTSA_WITH_HTTP) && 0
#define IOTSA_WEBSERVER "esp32webserverold"
#include <ESP32WebServer.h>
typedef WebServer IotsaWebServer;

#elif !defined(ESP32) && defined(IOTSA_WITH_HTTPS)
#define IOTSA_WEBSERVER "esp8266webserversecure"
#include <ESP8266WebServerSecure.h>
typedef axTLS::ESP8266WebServerSecure IotsaWebServer;

#elif !defined(ESP32) && defined(IOTSA_WITH_HTTPS)
#define IOTSA_WEBSERVER "esp8266webserver"
#include <ESP8266WebServer.h>
typedef ESP8266WebServer IotsaWebServer;

#endif

#if !defined(IOTSA_WEBSERVER) && defined(IOTSA_WITH_HTTP_OR_HTTPS)
#error "Cannot determine WebServer implementation to use"
#endif

class IotsaApplication;

class IotsaWebServerMixin {
friend class IotsaApplication;
public:
  IotsaWebServerMixin(IotsaApplication* _app);
#ifdef IOTSA_WITH_HTTP_OR_HTTPS
  IotsaWebServer *server;
protected:
  IotsaApplication* app;
  void webServerSetup();
  void webServerLoop();
  void webServerNotFoundHandler();
#endif
#ifdef IOTSA_WITH_WEB
  void webServerRootHandler();
#endif
};

#endif
