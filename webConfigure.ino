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

char hexToChar(char hex[]) {
    char byte = (char)strtol(hex, NULL, 16); // Convert hex string to integer, then cast to char
    return byte;
}

String webStringToAsciString(String in ) {
	char hexBuf[3];hexBuf[2]=0;
	String out = "";
	for (int i = 0; i < in.length(); i++) {
		if (in[i] != '%')
			out += in[i];
		else {
			hexBuf[0]=in[++i];
			hexBuf[1]=in[++i];
			out += hexToChar(hexBuf);
		}
	}
	return out;
}
void writeHtmlPage( WiFiClient &client ) {
	String ssid = getWifiSSID();
	String pswd = getWifiPassword();
	Serial.printf("Return HTML page with SSID = %s and password = %s", ssid.c_str(), pswd.c_str());
	Serial.println();


	client.println("<!DOCTYPE HTML><html><head>");
	client.println("<title>ESP8266 Configuration Form</title>");
	client.println("<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">");
//	client.println("<meta http-equiv=\"refresh\" content=\"10\">");
	client.println("</head><body>");

	client.print("Current SSID: <b>");
	client.printf("%s", ssid.c_str());
	client.println("</b><br>");

	client.println( " Current Password: <b>");
	client.printf("%s", pswd.c_str());
	client.println("</b><br><br />");

	client.println("<form action=\"/get\">");
	client.println("Update: <input type=\"text\" size = \"50\" name=\"creds\"  >");

	client.println("<br /><br>Use the following to commands on the 'Update' line:");

	client.println(
			"<ul>  "
			"<li>Set a new SSID: ssid=<i>new ssid</i></li>  "
			"<li>Set a new password: password=<i>new password</i></li>  "
			"<li>Save and Reboot: reboot (Note: Reboot saves the new setting and reboots the controller so they take effect.)</li>  "
			"<li>Reset: reset (Note: Resetting clears the SSID and password. Reboot is required to save the change.)</li>  "
			"</ul>");
	client.println("</body></html>");
}

String getVal(String &src, String key ) {
//	Serial.print("Looking for "); Serial.print(key);
	String result = "";
	int start = src.indexOf(key);

	if ( start > -1 ) {
		start += key.length();
		char c = src[start];
		while (c > 32 && c < 127) {
			result += c;
		    c = src[++start];
		}
	}
	return result;
}

void readHtmlRsp(WiFiClient &client) {
	String rspData = webStringToAsciString(client.readString());
	String ssid = getVal(rspData, "ssid=");
	String pswd = getVal(rspData, "password=");
	String reset = getVal(rspData, "reset ");
	String reboot = getVal(rspData, "reboot ");

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


