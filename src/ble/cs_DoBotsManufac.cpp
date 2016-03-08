/**
 * Author: Dominik Egger
 * Copyright: Distributed Organisms B.V. (DoBots)
 * Date: Sep 23, 2015
 * License: LGPLv3+
 */
#include <ble/cs_DoBotsManufac.h>

namespace BLEpp {

void DoBotsManufac::toArray(uint8_t* array) {
	*array = _deviceType;
}

void DoBotsManufac::parse(uint8_t* array, uint16_t len) {

	_deviceType = *array;

}

}/** namespace BLEpp */
