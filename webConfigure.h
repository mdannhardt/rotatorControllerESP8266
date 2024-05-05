#ifndef WEBCONFIGURE_H_
#define WEBCONFIGURE_H_

#include <WiFiClient.h>
#include <EEPROM.h>

bool isConfigured(void);
void clearEeprom(void);

String getWifiSSID(void);
void setWiFiSSID( String &ssid);
String getWifiPassword(void);
void setWiFiPassword( String &pswd);

void writeHtmlPage( WiFiClient &client );
void readHtmlRsp(WiFiClient &client);

#endif
