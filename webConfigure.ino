#include "webConfigure.h"
#include <EEPROM.h>

bool isConfigured(void) {
	return (EEPROM.read(0) != 255 || EEPROM.read(32) != 255);
}

void clearEeprom() {
    Serial.println("clearing eeprom");
    for (int i = 0; i < 96; ++i) {
      EEPROM.write(i, 0);
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
		Serial.print("Retrieved SSID: ");
		Serial.println(ssid);
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
		Serial.print("Retrieved Password: ");
		Serial.println(password);
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


void writeHtmlPage( WiFiClient &client ) {
	String ssid = getWifiSSID();
	String pswd = getWifiPassword();
	Serial.printf("Return HTML page with SSID = %s and password = %s", ssid.c_str(), pswd.c_str());
	Serial.println();


	client.println("<!DOCTYPE HTML><html><head>");
	client.println("<title>ESP Input Form</title>");
	client.println("<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">");
//	client.println("<meta http-equiv=\"refresh\" content=\"10\">");
	client.println("</head><body>");

	client.print("Current SSID: ");
	client.printf("%s", ssid.c_str());
	client.println("<br>");

	client.println( " Current Password: ");
	client.printf("%s", pswd.c_str());
	client.println("<br>");

	client.println("<form action=\"/get\">");
	client.println("Update: <input type=\"text\" name=\"creds\"  >");

	client.println("<br>Use the following to commands on the 'Update' line:");

	client.println(
			"<ul>  "
			"<li>Set a new SSID: SSID=\"new ssid\"</li>  "
			"<li>Set a new password: Password=\"new password\"</li>  "
			"<li>Refresh the SSID and password: Refresh (Note: after entering a new SSID or password, you need to refresh to see the new info.)</li>  "
			"<li>Save and Reboot: Reboot=true (Note: a reboot saves the new setting and reboots the controller so they take effect.)</li>  "
			"<li>Reset: Reset=true (Note: Resetting clears the SSID and password. Reboot is required to save the change.)</li>  "
			"</ul>");
	client.println("</body></html>");
}

String getVal(String &src, String key ) {
//	Serial.print("Looking for "); Serial.print(key);
	String result = "";
	int start = src.indexOf(key);
//	Serial.printf(" from index %d",start+key.length()); Serial.println("");

	if ( start > -1 ) {
		start += key.length();
		char c = src[start];
//		Serial.printf(" First char is: %c", c); Serial.println("");
		while (c > 32 && c < 127) {
			result += c;
//			Serial.println(c);
		    c = src[++start];
		}
	}
	return result;
}

void readHtmlRsp(WiFiClient &client) {
	String rspData = client.readString();
	String ssid = getVal(rspData, "SSID%3D");
	String pswd = getVal(rspData, "Password%3D");
	String reset = getVal(rspData, "Reset");
	String reboot = getVal(rspData, "Reboot");

	Serial.println("Full Resp: " + rspData);

	if ( ssid.length() > 0 ) {
		Serial.println("SSID = " + ssid);
		setWiFiSSID(ssid);
	}
	if ( pswd.length() > 0 ) {
		Serial.println("Password = " + pswd);
		setWiFiPassword(pswd);
	}

	if ( reboot.length() > 0) {
		saveEeprom();
		ESP.reset();
	}

	if ( reset.length() > 0)
		clearEeprom();

	client.flush();
}


