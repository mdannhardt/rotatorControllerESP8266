#ifndef WEBCONFIGURE_H_
#define WEBCONFIGURE_H_

#define DNS_PORT 53

bool isConfigured(void);
void clearEeprom(void);
void serverLoop(void);
String generateEspName (void);
void createWiFiAP(void);

String getWifiSSID(void);
void setWiFiSSID( String &ssid);
String getWifiPassword(void);
void setWiFiPassword( String &pswd);

#endif
