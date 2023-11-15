/**
 * rotatorControllerESP8266
 *
 * Project using the ESP8266 WiFi MCU and the QMC5883L 3-axis magnetic sensor to implement a rotator controller.
 *
 * The project connects to a WiFi access point, opens a UDP port and listens for incoming command packets.
 *
 * Command packets are used to set a bearing, read a bearing and stop.
 *
 * On receiving a set bearing command, the program determine the rotation direction, CW or CCW, and sets the proper
 * digital outputs to run the rotator motor. While rotating, the program monitors the current bearing to determine
 * when to stop while also monitoring for a stuck rotation condition.
 *
 * 1.0.0 2023-11-12 Original release.
 *
 */

#include "rotatorControllerESP8266.h"

#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <Wire.h>

#define USE_DECLINATION

//#define COMPASS_OFFICAL // if defined, use QMC5883LCompass library, otherwise use QMC5883L library
#ifdef COMPASS_OFFICAL
#include <QMC5883LCompass.h>
#else
#include <QMC5883L.h>
#endif

const char *version = "BuddiHex Rotator Firmware version 1.0.0";

const char *ssid = "HexRotator";
const char *password = "12345678";

// Program constants
const int ROTATE_CW = 15;  // GPIO-15 of NodeMCU esp8266 connecting to IN1 of L293D;
const int ROTATE_CCW = 13; // GPIO-13 of NodeMCU esp8266 connecting to IN2 of L293D

const unsigned long STUCK_TIME = 3000; // msecs w/o rotation to declare stuck
const unsigned long STUCK_DEG = 5;     // Min degrees that are expected to rotate through w/in STUCK_TIME
const int MAX_UDP_PACKET_SZ = 255;

// String representations of all possible commands (used in messages)
String cmdWord[MAX_CMDS] = { "NONE", "PARK", "STOP", "SET_BEARING", "GET_PST_BEARING", "GET_BEARING", "CAL_DECL"};

boolean rotating = false; // true means the antenna is being commanded to rotate
boolean clockwise = true; // direction of rotation. Clockwise means degrees are increasing, 0 to 359. Valid only if rotation==true
boolean stuck = false;    // true after a stuck rotator detected. Cleared on next bearing command.
int targetBearing;        // in degrees.
unsigned long timeStart;  // used to check for stuck rotator
int lastBearing;          // used to check for stuck rotator

#ifdef COMPASS_OFFICAL
QMC5883LCompass compass;
#else
QMC5883L compass;
#endif

WiFiUDP Udp;
unsigned int localUdpPort = 4210;        // local port to listen on
char incomingPacket[MAX_UDP_PACKET_SZ];  // buffer for incoming packets
char replyPacket[MAX_UDP_PACKET_SZ];     // a reply string to send back

#ifdef USE_DECLINATION
int Declination = -11;
#endif

int Azimuth = 0;
int getAzimuth()
{
//	Serial.print("compass.getAzimuth() = ");
#ifdef COMPASS_OFFICAL
	Azimuth = compass.getAzimuth();
#else
	if ( compass.ready() )
		Azimuth = compass.readHeading();
	else
		return Azimuth;
#endif

#ifdef USE_DECLINATION
	if ( Azimuth + Declination < 0 )
		Azimuth = 360 + (Azimuth + Declination);
	else if ( Azimuth + Declination > 360 )
		Azimuth = Azimuth - 360;
	else
		Azimuth += Declination;
#endif

	return Azimuth;
}

/**
 * Transform the N=359/0 and S=180/179 into 0 to 350 with S-N-S being 0-180-360
 */
int transform( int in )
{
	if ( in >= 180 && in < 360 )
		in -= 180;
	else
		in += 180;

	return in;
}

void connectWiFi() {
	Serial.println();
	Serial.println();
	Serial.print("Connecting to ");
	Serial.println(ssid);

	WiFi.begin(ssid, password);

	while (WiFi.status() != WL_CONNECTED) {
		delay(500);
		Serial.print(".");
	}
	Serial.println("");
	Serial.println("WiFi connected");

	// Open a UDP port to listen on.
	Udp.begin(localUdpPort);
	Serial.printf("Now listening at IP %s, UDP port %d\n",
			WiFi.localIP().toString().c_str(), localUdpPort);
	Serial.println(); // no trailing \n from the above printf() ?
}

void setup() {
	Serial.begin(115200);
	delay(10);
	Wire.begin();

	compass.init();

	Serial.printf("\n%s", version);
	Serial.println();

#ifndef COMPASS_OFFICAL
	// One time calibration
	Serial.print("Calibration check..");
	int r = 0;
	int16_t x,y,z,t;
	while (r==0) {
		r = compass.readRaw(&x,&y,&z,&t);
		Serial.print(".");
		delay(100);
	}
	Serial.printf("Complete. %d, %d, %d, %d\n", x,y,z,t);
#endif

	connectWiFi();

	// Declaring L293D Motor Controller control pins as Output
	pinMode(ROTATE_CW, OUTPUT);
	pinMode(ROTATE_CCW, OUTPUT);
}

void loop() {
	CMD command = NONE;

	if (rotating == true)
		checkRotateComplete();

	if (WiFi.status() != WL_CONNECTED)
		connectWiFi();

	int packetSize = Udp.parsePacket();
	if (packetSize) {
		// receive incoming UDP packets
//		Serial.printf("Received %d bytes from %s, port %d\n", packetSize,
//				Udp.remoteIP().toString().c_str(), Udp.remotePort());
		int len = Udp.read(incomingPacket, MAX_UDP_PACKET_SZ);
		if (len > 0) {
			incomingPacket[len] = 0; // terminate
		}

		command = processCommand(String(incomingPacket));
	}

	String cmdRsp = "";

	switch (command) {
	case NONE:
		// fall through
	case PARK: // ignore
		return;
	case STOP:
		rotateStop();
		break;
	case GET_PST_BEARING:
		cmdRsp = "AZ:" + String(getAzimuth());
		cmdRsp += ".0\r";
		break;
	case GET_BEARING:
		cmdRsp = "GET_BEARING:" + String(getAzimuth());
		if (rotating == true)
			cmdRsp += clockwise ? ":CW" : ":CCW";
		if ( stuck == true )
			cmdRsp += ":Stuck";
		break;
	case SET_BEARING:
		rotate();
		break;
	case CAL_DECL:
		calculate_declination();
		break;
	default:
		cmdRsp = "ERR:BUG:bad command in loop()!" + String(incomingPacket);
		break;
	}

	// send back a reply, to the IP address and port we got the packet from
	if ( cmdRsp.length() > 0 ) {
		cmdRsp.toCharArray(replyPacket, MAX_UDP_PACKET_SZ, 0);
		Udp.beginPacket(Udp.remoteIP(), Udp.remotePort());
		int rc = Udp.write(replyPacket);
		Udp.endPacket();
	}
}

/*
 * Stop the antenna from rotating
 */
void rotateStop() {
	digitalWrite(LED_BUILTIN, LOW);
	rotating = false; // done
	digitalWrite(ROTATE_CW, LOW);
	digitalWrite(ROTATE_CCW, LOW);

	Serial.println("Rotate stop");
}

/*
 * Select the rotation direction and start the rotation
 */
void rotate() {
	// Make sure stopped
	rotateStop();

	int currentBearing = getAzimuth();

	// Determine rotation
	clockwise = transform(targetBearing) > transform(currentBearing) ? true : false;

	// Set the digital outputs to start rotating in the desired direction
	digitalWrite(ROTATE_CW, !clockwise);
	digitalWrite(ROTATE_CCW, clockwise);
	digitalWrite(LED_BUILTIN, HIGH);
	rotating = true;

	if ( clockwise )
		Serial.printf("Rotate CW to %d\n", targetBearing);
	else
		Serial.printf("Rotate CCW to %d\n", targetBearing);
	Serial.println(); // no trailing \n from the above printf() ?

	// Setup to check for a stuck rotator
	timeStart = millis();
	lastBearing = getAzimuth();
}

/*
 * Check to see if the target bearing has been reached and if so stop
 * the rotation.
 */
void checkRotateComplete() {

	// get the current bearing
	int currentBearing = getAzimuth();

	// Consider the target reached once the rotator bearing has pasted the target.
	if ( clockwise && transform(currentBearing) > transform(targetBearing))
		rotateStop();
	else if (!clockwise && transform(currentBearing) < transform(targetBearing))
		rotateStop();

	// Or passing over Southern heading or somehow goes in wrong direction;
	if ( clockwise && transform(currentBearing) + STUCK_DEG < transform(lastBearing))
		rotateStop();
	else if (!clockwise && transform(lastBearing) + STUCK_DEG < transform(currentBearing))
		rotateStop();

	// If still rotating, check if stuck
	if (rotating) {
		unsigned long timeCurrent = millis();
		if (timeStart > timeCurrent) // roll over. Just rest and ignore this interval
			timeStart = timeCurrent;
		if (timeCurrent - timeStart > STUCK_TIME) {
			// time to check for rotation
			if (abs(currentBearing - lastBearing) < STUCK_DEG) {
				rotateStop(); // stuck
				stuck = true;
				Serial.println("ERR:Stuck rotator!");
			} else {
				// still moving. reset timer and current location
				lastBearing = currentBearing;
				timeStart = timeCurrent;
			}
		}
	}
}

/*
 * Check and set a new bearing
 */
CMD setNewBearing(int bearing)
{
	if (bearing < 0 || bearing > 360)
		Serial.println("ERR:New bearing degrees out of range.");
	else {
		targetBearing = bearing;
		stuck = false;
		return SET_BEARING;
	}
	return NONE;
}

/*
 * Process / parse a command
 */
CMD processCommand(String inputString) {
	int locInx, tmpTarget;

	//
	// Check for commands directly from N1MM
	//
	if ( (locInx = inputString.indexOf("<goazi>")) != -1) {
		locInx += String("<goazi>").length();
		return setNewBearing( getData(inputString.substring(locInx)));
	}
	else if ( inputString.indexOf("<N1MMRotor><stop>") != -1)
		return STOP;

	//
	// Check for commands directly from a PST application
	//
	if ( (locInx = inputString.indexOf("<AZIMUTH>")) != -1) {
		locInx += String("<AZIMUTH>").length();
		return setNewBearing( getData(inputString.substring(locInx)));
	}
	else if ( inputString.indexOf("<PST><STOP>") != -1)
		return STOP;
	else if ( inputString.indexOf("<PST><PARK>") != -1) {
		targetBearing = 0;
		return SET_BEARING;
	}
	else if (inputString.startsWith("<PST>AZ?"))
		return GET_PST_BEARING;

	//
	// Check for commands for Windows app
	//
	else if (inputString.startsWith("GET_BEARING"))
		return GET_BEARING;
	else if (inputString.startsWith("CAL_DECL"))
		return CAL_DECL;
	else {
		// unknown command
		Serial.print("?:");
		Serial.println(inputString);
	}

	return NONE;
}

/*
 * Extract the data from the passed string and return it as an int.
 * Data begins after the ':' character and continues to string end
 * or first non numeric character.
 */
int getData(String inputString) {
	int data = 0;
	int len = inputString.length();
	for (int i = 0; i < len; i++) {
		int val = inputString[i] - '0';
		if (val < 0 || val > 9)
			return data; // ignore remaining
		data = data * 10 + val;
	}
	return data;
}

/*
 * Calculate a new declination. The assumption is that the rotator is actually pointing true North.
 */
void calculate_declination()
{
	// get the current bearing
	int currentBearing;

#ifdef COMPASS_OFFICAL
	currentBearing = compass.getAzimuth();
#else
	if ( compass.ready() )
		currentBearing = compass.readHeading();
#endif

	if ( currentBearing < 180 )
		Declination = -currentBearing;
	else
		Declination = currentBearing - 360;

	Serial.printf("Current bearing: %d. New Declination: %d.", currentBearing, Declination);
	Serial.println();
}

