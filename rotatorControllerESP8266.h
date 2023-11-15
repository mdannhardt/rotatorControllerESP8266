/*
 * rotatorControllerESP8266.h
 *
 *  Created on: Oct 27, 2023
 *      Author: mdann
 */

#ifndef ROTATORCONTROLLERESP8266_H_
#define ROTATORCONTROLLERESP8266_H_

enum CMD {
	NONE, PARK,     // not implemented
	STOP,           // stop rotation
	SET_BEARING,    // set a new bearing
	GET_PST_BEARING,// return the current bearing
	GET_BEARING,    // return the current bearing along with rotation direction
	CAL_DECL,       // calculate a new calibration
	MAX_CMDS
};

// Function prototype forward declarations
CMD processCommand(String inputString);
int findData(String inputString);
int getData(String inputString);
void checkRotateComplete();
void calculate_declination();

#endif /* ROTATORCONTROLLERESP8266_H_ */
