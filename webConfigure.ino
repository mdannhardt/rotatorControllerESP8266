#include "webConfigure.h"
#include "rotatorControllerESP8266.h"
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <DNSServer.h>
#include <Wire.h>
#include <EEPROM.h>

ESP8266WebServer server(80);

int NewTargetBearing = 0;

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
		for (int i = 0; i < 32 && EEPROM.read(i) != 0; ++i) {
			ssid += char(EEPROM.read(i));
		}
//		Serial.print("Retrieved SSID: ");
//		Serial.println(ssid);
	}
	return ssid;
}

void setWiFiSSID( String &ssid) {
    int i;
//	Serial.print("Setting SSID: ");
//	Serial.println(ssid);

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
		for (int i = 32; i < 96 && EEPROM.read(i) != 0; ++i) {
			password += char(EEPROM.read(i));
		}
//		Serial.print("Retrieved Password: ");
//		Serial.println(password);
	}
	return password;
}


void setWiFiPassword( String &pswd) {
    int i;
//	Serial.print("Setting Password: ");
//	Serial.println(pswd);
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

  html += "<style>";
  html += "  body { font-family: Arial, sans-serif; }";

  html += "  .bearing { font-size: 48px; font-weight: bold; text-align: center; color: #0066cc; }";

  html += "  .button-grid {";
  html += "    display: grid;";
  html += "    grid-template-columns: repeat(3, 1fr);";
  html += "    gap: 10px;";
  html += "    max-width: 300px;";
  html += "  }";

  html += "  .bearing-button {";
  html += "    background-color: #4CAF50;";
  html += "    border: none;";
  html += "    color: white;";
  html += "    padding: 15px 0;";
  html += "    text-align: center;";
  html += "    text-decoration: none;";
  html += "    display: inline-block;";
  html += "    font-size: 16px;";
  html += "    margin: 4px 2px;";
  html += "    cursor: pointer;";
  html += "    border-radius: 5px;";
  html += "  }";

  html += "  .bearing-button.red {";
  html += "    background-color: #ff0000;";
  html += "  }";
  html += "</style>";

  html += "<script>";

  html += "function updateBearing() {";
  html += "  var xhttp = new XMLHttpRequest();";
  html += "  xhttp.onreadystatechange = function() {";
  html += "    if (this.readyState == 4 && this.status == 200) {";
  html += "      document.getElementById('bearing').innerHTML = this.responseText;";
  html += "    }";
  html += "  };";
  html += "  xhttp.open('GET', '/bearing', true);";
  html += "  xhttp.send();";
  html += "}";

  html += "function setSSID() {";
  html += "  var ssid = document.getElementById('ssidInput').value;";
  html += "  var xhttp = new XMLHttpRequest();";
  html += "  xhttp.open('POST', '/ssid', true);";
  html += "  xhttp.setRequestHeader('Content-type', 'application/x-www-form-urlencoded');";
  html += "  xhttp.send('ssid=' + ssid);";
  html += "}";

  html += "function setPassword() {";
  html += "  var password = document.getElementById('passwordInput').value;";
  html += "  var xhttp = new XMLHttpRequest();";
  html += "  xhttp.open('POST', '/password', true);";
  html += "  xhttp.setRequestHeader('Content-type', 'application/x-www-form-urlencoded');";
  html += "  xhttp.send('password=' + password);";
  html += "}";

  html += "function reset() {";
  html += "  var xhttp = new XMLHttpRequest();";
  html += "  xhttp.open('POST', '/reset', true);";
  html += "  xhttp.setRequestHeader('Content-type', 'application/x-www-form-urlencoded');";
  html += "  xhttp.send('reset');";
  html += "}";

  html += "function setNewBearing() {";
  html += "  var newBearing = document.getElementById('newBearingInput').value;";
  html += "  var xhttp = new XMLHttpRequest();";
  html += "  xhttp.open('POST', '/newBearing', true);";
  html += "  xhttp.setRequestHeader('Content-type', 'application/x-www-form-urlencoded');";
  html += "  xhttp.send('newBearing=' + newBearing);";
  html += "}";

  html += "function setCardnal(value) {";
  html += "  var xhttp = new XMLHttpRequest();";
  html += "  xhttp.onreadystatechange = function() {";
  html += "    if (this.readyState == 4 && this.status == 200) {";
  html += "      location.reload();"; // Add this line to force a page reload
  html += "    }";
  html += "  };";
  html += "  xhttp.open('POST', '/newBearing', true);";
  html += "  xhttp.setRequestHeader('Content-type', 'application/x-www-form-urlencoded');";
  html += "  xhttp.send('newBearing=' + value);";
  html += "}";

  html += "setInterval(updateBearing, 100000);";

  html += "</script>";

  html += "</head><body>";

  html += "<b>Set Bearing ";
  html += "<input type='number' id='newBearingInput' step='5' value='" + String(NewTargetBearing) + "'>";
  html += "<button onclick='setNewBearing()'>Go</button></b>";

  html += "<br /><div class='button-grid'>";
  html += "<button class='bearing-button' onclick='setCardnal(315)'>NW</button>";
  html += "<button class='bearing-button' onclick='setCardnal(1)'>North</button>";
  html += "<button class='bearing-button' onclick='setCardnal(45)'>NE</button>";
  html += "<button class='bearing-button' onclick='setCardnal(270)'>West</button>";
  html += "<button class='bearing-button red' onclick='setCardnal(28)'>STOP</button>";
  html += "<button class='bearing-button' onclick='setCardnal(90)'>East</button>";
  html += "<button class='bearing-button' onclick='setCardnal(225)'>SW</button>";
  html += "<button class='bearing-button' onclick='setCardnal(180)'>South</button>";
  html += "<button class='bearing-button' onclick='setCardnal(135)'>SE</button>";
  html += "</div>";


  html += "<h2>Current Bearing</h2>";
  html += "<p class='bearing' id='bearing'></p>";

  html += "<br /><br /><br>Set WiFi Router SSID and Password: ";
  html += "<input type='text' id='ssidInput' value='" + getWifiSSID() + "' oninput='setSSID(this.value)'>";
  html += "<input type='text' id='passwordInput' value='" + getWifiPassword() + "' oninput='setPassword(this.value)'>";
  html += "<button onclick='reset()'>Save and Reboot</button></br>";

  html += "</body></html>";
  server.send(200, "text/html", html);
}

extern String buildCurrentBearingString();
void handleCurrentBearing() {
	//  Serial.println("handleCurrentBearing()");
    server.send(200, "text/plain", buildCurrentBearingString());
}

void handleSSID() {
//  Serial.println("handleSSID()");
  if (server.hasArg("ssid")) {
    String ssid = server.arg("ssid");
    setWiFiSSID(ssid);
    server.send(200, "text/plain", "SSID updated to " + String(ssid));
  } else {
    server.send(400, "text/plain", "Missing SSID parameter");
  }
}

void handlePassword() {
  Serial.println("handlePassword()");
  if (server.hasArg("password")) {
    String password = server.arg("password");
    setWiFiPassword(password);
    server.send(200, "text/plain", "Password updated to " + String(password));
  } else {
    server.send(400, "text/plain", "Missing SSID parameter");
  }
}

void handleReset() {
  Serial.println("handleReset()");
  if (server.hasArg("reset")) {
    server.send(200, "text/plain", "Resetting");
    saveEeprom();
    ESP.reset();
  } else {
    server.send(400, "text/plain", "Missing SSID parameter");
  }
}

extern CMD setNewBearing(int newBearing);
extern void rotate();
void handleSetBearing() {
  Serial.println("handleSetBearing()");
  if (server.hasArg("newBearing")) {
    int bearing = server.arg("newBearing").toInt();
    if ( setNewBearing(bearing) == SET_BEARING) {
    	rotate();
    	NewTargetBearing = bearing;
    }
    server.send(200, "text/plain", "New bearing updated to " + String(bearing));
  } else {
    server.send(400, "text/plain", "Missing newBearing parameter");
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
	server.on("/ssid", handleSSID);
	server.on("/reset", handleReset);
	server.on("/password", handlePassword);
	server.on("/bearing", handleCurrentBearing);
	server.on("/newBearing", handleSetBearing);

	server.begin();
	Serial.println("Server started");
}


