/*
 * rotatorControllerESP8266.h
 *
 *  Created on: Oct 27, 2023
 *      Author: mdann
 */

#ifndef ROTATORCONTROLLERESP8266_H_
#define ROTATORCONTROLLERESP8266_H_

#define DNS_PORT 53

enum CMD {
	NONE, PARK,     // not implemented
	STOP,           // stop rotation
	SET_BEARING,    // set a new bearing
	GET_PST_BEARING,// return the current bearing
	GET_BEARING,    // return the current bearing along with rotation direction
	CAL_DECL,       // calculate a new calibration
	IDENTIFY,       // send IP address
	MAX_CMDS
};

enum WIRE_SIDE {
	UNKNWN, EAST, WEST, MAX_SIDES
};

// Fatal errors
#define NO_ERROR         0
#define COMPASS_MISSING -1
#define COMPASS_NOT_RDY -2
#define COMPASS_CAL_FLT -3


// Function prototype forward declarations
CMD processCommand(String inputString);
int findData(String inputString);
int getData(String inputString);
void checkRotateComplete();
void calculateDeclination();

String getWifiSSID();
String getWifiPassword();
bool isConfigured();


#endif /* ROTATORCONTROLLERESP8266_H_ */
