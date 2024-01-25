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
 * 1.0.0 2023-12-12 Original release.
 * 1.0.1 2024-01-24 Fix cable wrap at S end issues.
 *
 */

/* ----------------- WiFi Configuration --------------------------------------------------
 *
 * Un-comment the #define PROVIDE_AP to configure the ESP8266 to offer a DNS hotspot to
 *             which computers can connect.
 * Set the ssid to the name of the router or hotspot to which the ESP8266 should connect
 *             to or offer
 * Set the password as needed.
*/

//#define PROVIDE_AP
const char *ssid = "HexRotator";
const char *password = "12345678";

/* ------------------- End Configuration ------------------------------------------------ */


#include "rotatorControllerESP8266.h"

#include <ESP8266WiFi.h>
#include <DNSServer.h>
#include <vector>
#include <WiFiUdp.h>
#include <Wire.h>

#define DEBUG
#ifdef DEBUG
 #define DBG
#else
 #define DBG for(;0;)
#endif

//#define USE_DECLINATION

//#define COMPASS_OFFICAL // if defined, use QMC5883LCompass library, otherwise use QMC5883L library
#ifdef COMPASS_OFFICAL
#include <QMC5883LCompass.h>
#else
#include <QMC5883L.h>
#endif

const char *version = "BuddiHex Rotator Firmware version 1.0.1beta";

// Program constants
const int ROTATE_CCW = 15; // GPIO-15 of NodeMCU esp8266 connecting to IN1 of L293D;
const int ROTATE_CW  = 13; // GPIO-13 of NodeMCU esp8266 connecting to IN2 of L293D


const unsigned long ROTATE_CHK_TIME = 100; // msecs between check for rotation completion.
const unsigned long STUCK_TIME = 5000; // msecs w/o rotation to declare stuck
const unsigned long STUCK_DEG = 5;     // Min degrees that are expected to rotate through w/in STUCK_TIME
const int MAX_UDP_PACKET_SZ = 255;

// String representations of all possible commands (used in messages)
String cmdWord[MAX_CMDS] = { "NONE", "PARK", "STOP", "SET_BEARING", "GET_PST_BEARING", "GET_BEARING", "CAL_DECL"};

boolean rotating = false; // true means the antenna is being commanded to rotate
boolean clockwise = true; // direction of rotation. Clockwise means degrees are increasing, 0 to 359. Valid only if rotation==true
boolean stuck = false;    // true after a stuck rotator detected. Cleared on next bearing command.
WIRE_SIDE wireSide = UNKNWN;
boolean wrapped = false;  // rotator has crossed over due south
int targetBearing;        // in degrees.
unsigned long stuckTimer;  // used to check for stuck rotator
unsigned long checkRotationTimer;
int stuckBearingCheck;    // used to check for stuck rotator
int lastBearing;

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
#ifdef COMPASS_OFFICAL
	Azimuth = compass.getAzimuth();
#else
	if ( compass.ready() )
		Azimuth = compass.readHeading(); // returns 1 - 360
	else
		return Azimuth;
#endif

#ifdef USE_DECLINATION
	if ( Azimuth + Declination < 0 )
		Azimuth = 360 + (Azimuth + Declination);
	else if ( Azimuth + Declination > 360 )
		Azimuth = Azimuth + Declination - 360;
	else
		Azimuth += Declination;
#endif

	if (abs(Azimuth - 270) <= 45)
		wireSide = WEST;
	if (abs(Azimuth - 90) <= 45)
		wireSide = EAST;

	return Azimuth;
}

/**
 * Transform the N=360/1 and S=180/179 into 0 to 359 with S-N-S being 0-180-359
 */
int transform( int in )
{
	int out;
	if ( in == 180 || in == 360)
		out = 180;
	if ( in > 180 && in < 360 )
		out = in - 180;
	else
		out = in + 179;

	return out;
}

#ifdef PROVIDE_AP
	// Functions to support the ESP8266 acting as DNS and connection AP
static DNSServer DNS;

void createWiFiAP() {
	Serial.println();
	Serial.println();

	// create access point
	while (!WiFi.softAP(ssid, password, 6, false, 15)) {
		delay(500);
	}

	// start dns server
	if (!DNS.start(DNS_PORT, ssid, WiFi.softAPIP()))
		Serial.printf("\n failed to start dns service \n");

	Udp.begin(4210);
}

#else
/*
 * Connect to a router/hotspot AP
 */
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
#endif

/*
 *
 */
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

DBG	Serial.printf("Complete. %d, %d, %d, %d\n", x,y,z,t);
#endif

#ifdef PROVIDE_AP
	createWiFiAP();
#else
	connectWiFi();
#endif

	// Declaring L293D Motor Controller control pins as Output
	pinMode(ROTATE_CW, OUTPUT);
	pinMode(ROTATE_CCW, OUTPUT);
}

/*
 *
 */
void loop() {
	CMD command = NONE;

	if ( digitalRead(ROTATE_CW) && digitalRead(ROTATE_CCW))
	{
		rotateStop(6);
		Serial.println("PANIC!! Both pins high!!");
	}

	if (rotating == true)
	{
		unsigned long timeCurrent = millis();
		if (checkRotationTimer > timeCurrent) // roll over. Just rest and ignore this interval
			checkRotationTimer = timeCurrent;
		if (timeCurrent - checkRotationTimer > ROTATE_CHK_TIME)
		{
			checkRotateComplete();
			checkRotationTimer = millis();
		}
	}

#ifdef PROVIDE_AP
	DNS.processNextRequest();
#else
	if (WiFi.status() != WL_CONNECTED)
		connectWiFi();
#endif

	int packetSize = Udp.parsePacket();
	if (packetSize) {
		// receive incoming UDP packets
//DBG		Serial.printf("Received %d bytes from %s, port %d\n", packetSize, \
				Udp.remoteIP().toString().c_str(), Udp.remotePort());
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
		rotateStop(0);
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
		calculateDeclination();
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
void rotateStop(int why) {
	digitalWrite(LED_BUILTIN, LOW);
	rotating = false; // done
	digitalWrite(ROTATE_CW, LOW);
	digitalWrite(ROTATE_CCW, LOW);

	if (why == 6) {
		// Stopping prior to starting new rotation.
		Serial.println("Rotate stop." );
		return;
	}

	Serial.print("Rotate stop." );
	switch(why) {
	case 0:
		Serial.println(" CMD"); break;
	case 1: Serial.println(" CW complete"); break;
	case 2: Serial.println(" CCW complete"); break;
	case 3: Serial.println(" CW bump complete"); break;
	case 4: Serial.println(" CCW bump complete"); break;
	case 5: Serial.println(" STUCK"); break;
	case 6: Serial.println(""); break;
	}
}

/*
 * Start the rotation
 */
void rotate() {
	// Make sure stopped
	rotateStop(6);

	// Set the digital outputs to start rotating in the desired direction
	digitalWrite(ROTATE_CW, !clockwise);
	digitalWrite(ROTATE_CCW, clockwise);
	digitalWrite(LED_BUILTIN, HIGH);
	rotating = true;

	Serial.printf("Rotate %s to %d", clockwise ? "CW" : "CCW", targetBearing);	Serial.println();

	// Setup to check for a stuck rotator
	stuckTimer = checkRotationTimer = millis();
}

/*
 * Select the rotation direction and start the rotation
 */
void bumpRotate(int newBearing, int currenBearing) {
	if (newBearing == currenBearing)
		return;

	bool cw = transform(newBearing) > transform(currenBearing);

	// Make sure stopped
	rotateStop(6);

	Serial.printf("Rotate %s (bump)", clockwise ? "CW" : "CCW"); Serial.println();

	// Set the digital outputs to start rotating in the desired direction
	digitalWrite(ROTATE_CW, !cw);
	digitalWrite(ROTATE_CCW, cw);
	digitalWrite(LED_BUILTIN, HIGH);
	rotating = true;

	delay(250);
	rotateStop(cw ? 3 : 4);
}

/*
 * Check to see if the target bearing has been reached and if so stop
 * the rotation.
 */
void checkRotateComplete() {

	// get the current bearing
	int currentBearing = getAzimuth();

	// Logical based on hemisphere to avoid compass discontinuity
	bool west = (currentBearing > 179 && currentBearing <= 360);
	bool south = (currentBearing > 90 && currentBearing < 270);
	int currentBearingT = transform(currentBearing); // used in N
	int  targetBearingT = transform( targetBearing); // used in N

	// Ignore completion checks until unwrapped)
	wrapped = south ? wrapped : false; // never wrapped if in the N
	if ( wrapped == true )
	{
		if (wireSide == WEST)
			wrapped = ( clockwise && west);//currentBearingT < 180 );
		else
			wrapped = ( !clockwise && !west); //currentBearingT > 179);
	}

#ifdef DEBUG
	if (wrapped == true)
	{
		Serial.printf("checkRotateComplete(). Unwrapping in %s %s from %d(%d) to %d(%d). %s.", \
				south ? "S" : "N", clockwise ? "CW":"CCW", \
				currentBearing, currentBearingT, targetBearing, targetBearingT, \
				wireSide == EAST ? "wire east side" : wireSide == WEST ? "wire west side" : "?" ); \
		Serial.println("");
	}
#endif

	if ( wrapped == false )
	{
		// Consider the target reached once the rotator bearing has passed the target.
		if ( south && west)
		{
			// S hemi easy using compass bearings (not transformed bearings)
			if (clockwise && wireSide == EAST && (currentBearing >= targetBearing) )
				rotateStop(1);
			if (!clockwise && currentBearing <= targetBearing)
				rotateStop(2);
			// following checks case where target is in the E thus between 1 and 179.
			// The 315 check ensures switch to N hemi logic
			if (clockwise && (currentBearing >= (targetBearing < 180 ? 315 : targetBearing) ))
				rotateStop(1);
		}
		else if ( south && !west)
		{
			if (!clockwise && wireSide == WEST && (currentBearing <= targetBearing) )
				rotateStop(2);
			if (clockwise && currentBearing >= targetBearing)
				rotateStop(1);
			if (!clockwise && (currentBearing <= (targetBearing > 180 ? 45 : targetBearing) ))
				rotateStop(2);
		}
		else
		{
			// north hemi
			if (clockwise && (currentBearingT >= targetBearingT)) {
				rotateStop(1);
			} else if (!clockwise && (currentBearingT <= targetBearingT)) {
				rotateStop(2);
			}
		}

	}

	// If still rotating, check if stuck
	if (rotating) {
		unsigned long timeCurrent = millis();
		if (stuckTimer > timeCurrent) // roll over. Just rest and ignore this interval
			stuckTimer = timeCurrent;
		if (timeCurrent - stuckTimer > STUCK_TIME) {
			// time to check for rotation
			if (abs(currentBearing - stuckBearingCheck) < STUCK_DEG) {
				rotateStop(5); // stuck
				stuck = true;
			} else {
				// still moving. reset timer and bearing
				stuckBearingCheck = currentBearing;
				stuckTimer = timeCurrent;
			}
		}
	}

#ifdef DEBUG
   if ( !rotating) { // debug
		Serial.printf("rotating %s stopped in %s%s: c=%d(%d), t=%d(%d) %s",
				clockwise ? "CW" : "CCW",
				south ? "S" : "N",
				west ? "W" : "E",
				currentBearing, currentBearingT,
				targetBearing, transform(targetBearing),
				wrapped ? "wrapped" : "");
		Serial.println("");
		Serial.println("----------------");
		Serial.println("");
	}
#endif
}

/*
 * Check and set a new bearing
 */
CMD setNewBearing(int newBearing)
{
	if (newBearing < 0 || newBearing > 360)
		Serial.println("ERR:New bearing degrees out of range.");
	else
	{
		newBearing = newBearing == 0 ? 1 : newBearing; // Range is 1 to 360 but allow 0

		int currentBearing = getAzimuth();

		// If current bearing already close to new bearing, rotate on time
		if ( abs(transform(newBearing) - transform(currentBearing)) < 5 )
		{
			// bump for one second
			bumpRotate(newBearing, currentBearing);
			return NONE;
		}

		// Adjust the target slightly to compensate for overshoot
		newBearing = newBearing + (newBearing < 180 ? -3 : +3);
		if (newBearing <   1) newBearing = 360 + newBearing;  //  0 -> 360
		if (newBearing > 360) newBearing = newBearing - 1;    // 360 -> 1

		clockwise = transform(newBearing) > transform(currentBearing) ? true : false;
		// Set off in opposite direction if wrapped.
		if ( clockwise && wireSide == EAST && currentBearing > 179 && currentBearing < 315) clockwise = false;
		if (!clockwise && wireSide == WEST && currentBearing < 180 && currentBearing > 45) clockwise = true;

DBG		Serial.printf("setNewBearing(). Turning %s from %d(%d) to %d(%d). %s.", \
				clockwise ? "CW":"CCW", \
				currentBearing, transform(currentBearing), newBearing, transform(newBearing), \
				wireSide == EAST ? "wire east side" : wireSide == WEST ? "wire west side" : "?" ); \
		Serial.println("");

		targetBearing = newBearing;

		stuck = false;
		wrapped = true; // assume true until confirmed not.
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
		return setNewBearing( 0 );
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
void calculateDeclination()
{
#ifdef USE_DECLINATION
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
#else
	Serial.println("USE_DECLINATION not defined");
	Serial.println();
#endif
}

