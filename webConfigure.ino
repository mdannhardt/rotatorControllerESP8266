#include "webConfigure.h"
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <DNSServer.h>
#include <Wire.h>
#include <EEPROM.h>

ESP8266WebServer server(80);

float setpoint = 25.0; // Default setpoint


bool isConfigured(void) {
	return (EEPROM.read(0) != 255 || EEPROM.read(32) != 255);
}

void clearEeprom() {
    Serial.println("clearing eeprom");
    for (int i = 0; i < 96; ++i) {
      EEPROM.write(i, 255);
    }
}

void saveEeprom(void) {
	EEPROM.commit();
}

String getWifiSSID() {
	String ssid = "no_cfg";
	if (EEPROM.read(0) != 255) {
		ssid = "";
		for (int i = 0; i < 32; ++i) {
			ssid += char(EEPROM.read(i));
		}
//		Serial.print("Retrieved SSID: ");
//		Serial.println(ssid);
	}
	return ssid;
}

void setWiFiSSID( String &ssid) {
    int i;
	Serial.print("Setting SSID: ");
	Serial.println(ssid);

    for (i=0; i < 32; i++)
      EEPROM.write(i, 0);
    for (i = 0; i < ssid.length(); i++) {
      EEPROM.write(i, ssid[i]);
//    Serial.print("Wrote: ");
//    Serial.println(ssid[i]);
    }
}

String getWifiPassword() {
	String password = "no_cfg";
	if (EEPROM.read(32) != 255) {
		password = "";
		for (int i = 32; i < 96; ++i) {
			password += char(EEPROM.read(i));
		}
//		Serial.print("Retrieved Password: ");
//		Serial.println(password);
	}
	return password;
}


void setWiFiPassword( String &pswd) {
    int i;
	Serial.print("Setting Password: ");
	Serial.println(pswd);
    for (i=32; i < 96; i++)
      EEPROM.write(i, 0);
    for (int j = 0, i = 32; j < pswd.length(); j++, i++) {
      EEPROM.write(i, pswd[j]);

//      Serial.print("Wrote: ");
//      Serial.println(pswd[j]);
    }
}

void serverLoop() {
  server.handleClient();
}

void handleRoot() {
  Serial.println("handleRoot()");
  String html = "<html><head>";
  //html += "<meta http-equiv='refresh' content='1000'>";
  html += "<style>";
  html += "  body { font-family: Arial, sans-serif; }";
  html += "  .temperature { font-size: 48px; font-weight: bold; color: #0066cc; }";
  html += "</style>";
  html += "<script>";
  html += "function updateTemperature() {";
  html += "  var xhttp = new XMLHttpRequest();";
  html += "  xhttp.onreadystatechange = function() {";
  html += "    if (this.readyState == 4 && this.status == 200) {";
  html += "      document.getElementById('temperature').innerHTML = this.responseText;";
  html += "    }";
  html += "  };";
  html += "  xhttp.open('GET', '/temperature', true);";
  html += "  xhttp.send();";
  html += "}";
  html += "function setSetpoint() {";
  html += "  var setpoint = document.getElementById('setpointInput').value;";
  html += "  var xhttp = new XMLHttpRequest();";
  html += "  xhttp.open('POST', '/setpoint', true);";
  html += "  xhttp.setRequestHeader('Content-type', 'application/x-www-form-urlencoded');";
  html += "  xhttp.send('setpoint=' + setpoint);";
  html += "}";
  html += "function setCardnal(value) {";
  html += "  var xhttp = new XMLHttpRequest();";
  html += "  xhttp.onreadystatechange = function() {";
  html += "    if (this.readyState == 4 && this.status == 200) {";
  html += "      location.reload();"; // Add this line to force a page reload
  html += "    }";
  html += "  };";
  html += "  xhttp.open('POST', '/setpoint', true);";
  html += "  xhttp.setRequestHeader('Content-type', 'application/x-www-form-urlencoded');";
  html += "  xhttp.send('setpoint=' + value);";
//  html += "  xhttp.open('POST', '/fanspeed', true);";
//  html += "  xhttp.setRequestHeader('Content-type', 'application/x-www-form-urlencoded');";
//  html += "  xhttp.send('fanspeed=' + value);";
  html += "}";
  html += "setInterval(updateTemperature, 1000);";
  html += "</script>";
  html += "</head><body>";
  html += "<h2>Set Temperature</h2>";
  html += "<input type='number' id='setpointInput' step='1' value='" + String(setpoint) + "' oninput='setSetpoint(this.value)'>";
  //html += "<h2>Set Fan Speed</h2>";
  html += "<button onclick='setCardnal(0)'>North</button>";
  html += "<button onclick='setCardnal(90)'>East</button>";
  html += "<button onclick='setCardnal(180)'>South</button>";
  html += "<button onclick='setCardnal(270)'>West</button>";
  html += "<h2>Current Temperature</h2>";
  html += "<p class='temperature' id='temperature'></p>";
  html += "</body></html>";
  server.send(200, "text/html", html);
}

void handleTemperature() {
  float temperature = setpoint;
  if (isnan(temperature)) {
    server.send(200, "text/plain", "Failed to read temperature");
  } else {
    server.send(200, "text/plain", String(temperature) + " C");
  }
}

void handleSetpoint() {
  if (server.hasArg("setpoint")) {
    setpoint = server.arg("setpoint").toFloat();
    server.send(200, "text/plain", "Setpoint updated to " + String(setpoint));
  } else {
    server.send(400, "text/plain", "Missing setpoint parameter");
  }
}


// Functions to support the ESP8266 acting as DNS and connection AP
static DNSServer DNS;

/*
 * Construct the ESP board name that is used to ID on the network. The format is:
 * ESP-XXYYZZ where XX, YY and ZZ are the last three hex characters of the device's MAC
 * address.
 *
 * Save the name in the
 */
String generateEspName ()
{
	uint8_t mac[WL_MAC_ADDR_LENGTH];
	WiFi.macAddress(mac);
	char macStr[18] = { 0 };

	sprintf(macStr, "%02X%02X%02X", mac[3], mac[4], mac[5]);
	return "ESP-" + String(macStr);
}

void createWiFiAP() {
	Serial.println();

	IPAddress local_IP(192,168,4,1);
	IPAddress gateway(192,168,4,9);
	IPAddress subnet(255,255,255,0);

	// create access point

	Serial.print("Setting soft-AP configuration ... ");
	Serial.println(
			WiFi.softAPConfig(local_IP, gateway, subnet) ? "Ready" : "Failed!");

	Serial.print("Setting soft-AP ... ");
	Serial.println(WiFi.softAP(generateEspName()) ? "Ready" : "Failed!");

	Serial.print("Soft-AP IP address = ");
	Serial.println(WiFi.softAPIP());

	// start dns server
	if (!DNS.start(DNS_PORT, generateEspName(), WiFi.softAPIP()))
		Serial.printf("\n failed to start dns service \n");

	// Start the server
	server.on("/", handleRoot);
	server.on("/temperature", handleTemperature);
	server.on("/setpoint", handleSetpoint);

	server.begin();
	Serial.println("Server started");
}


