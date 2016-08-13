/*
 * Author: Bart van Vliet
 * Copyright: Distributed Organisms B.V. (DoBots)
 * Date: Aug 4, 2016
 * License: LGPLv3+, Apache License, or MIT, your choice
 */

#include "processing/cs_FactoryReset.h"
#include "cfg/cs_Config.h"
#include "storage/cs_State.h"
#include "drivers/cs_RTC.h"
#include "ble/cs_Stack.h"
#include "processing/cs_CommandHandler.h"

FactoryReset::FactoryReset() : _recoveryEnabled(true), _rtcStartTime(0),
#if (NORDIC_SDK_VERSION >= 11)
		_recoveryDisableTimerId(NULL)
#else
		_recoveryDisableTimerId(UINT32_MAX)
#endif
{
#if (NORDIC_SDK_VERSION >= 11)
	_recoveryDisableTimerData = { {0} };
	_recoveryDisableTimerId = &_recoveryDisableTimerData;
#endif
}

void FactoryReset::init() {
	Timer::getInstance().createSingleShot(_recoveryDisableTimerId, (app_timer_timeout_handler_t)FactoryReset::staticTimeout);
	resetTimeout();
}

void FactoryReset::resetTimeout() {
	_recoveryEnabled = true;
	_rtcStartTime = RTC::getCount();
	Timer::getInstance().start(_recoveryDisableTimerId, MS_TO_TICKS(FACTORY_RESET_TIMEOUT), this);
}

void FactoryReset::timeout() {
	_recoveryEnabled = false;
	uint8_t resetState;
	State::getInstance().get(STATE_FACTORY_RESET, resetState);
	if (resetState == FACTORY_RESET_STATE_LOWTX) {
		Nrf51822BluetoothStack::getInstance().changeToNormalTxPowerMode();
	}
	State::getInstance().set(STATE_FACTORY_RESET, (uint8_t)FACTORY_RESET_STATE_NORMAL);
}

void FactoryReset::enableRecovery(bool enable) {
	LOGd("recovery: %i", enable);
	_recoveryEnabled = enable;
}

bool FactoryReset::recover(uint32_t resetCode) {
	if (!_recoveryEnabled || RTC::difference(RTC::getCount(), _rtcStartTime) > RTC::msToTicks(FACTORY_RESET_TIMEOUT)) {
		return false;
	}
	uint8_t resetState;
	State::getInstance().get(STATE_FACTORY_RESET, resetState);
	switch (resetState) {
	case FACTORY_RESET_STATE_NORMAL:
		State::getInstance().set(STATE_FACTORY_RESET, (uint8_t)FACTORY_RESET_STATE_LOWTX);
		LOGd("recovery: go to low tx");
		Nrf51822BluetoothStack::getInstance().changeToLowTxPowerMode();
		Nrf51822BluetoothStack::getInstance().disconnect();
		resetTimeout();
		return true;
//		break;
	case FACTORY_RESET_STATE_LOWTX:
		State::getInstance().set(STATE_FACTORY_RESET, (uint8_t)FACTORY_RESET_STATE_RESET);
		LOGd("recovery: factory reset");

		return factoryReset();
//		break;
	case FACTORY_RESET_STATE_RESET:
		// What to do here?
		break;
	default:
		break;
	}
	return false;
}

bool FactoryReset::factoryReset(uint32_t resetCode) {
	if (resetCode != FACTORY_RESET_CODE) {
		return false;
	}
	return factoryReset();
}

bool FactoryReset::factoryReset() {
	LOGf("factory reset");

	//! Go into factory reset mode after next reset.
	State::getInstance().set(STATE_OPERATION_MODE, (uint8_t)OPERATION_MODE_FACTORY_RESET);

	LOGi("Going into factory reset mode, rebooting device in 2s ...");
	CommandHandler::getInstance().resetDelayed(GPREGRET_SOFT_RESET);

	return true;
}

bool FactoryReset::finishFactoryReset() {
	//! First clear sensitive data: keys
	Settings::getInstance().factoryReset(FACTORY_RESET_CODE);

	// Clear other data
	//! TODO: this also set operation mode to setup
	State::getInstance().factoryReset(FACTORY_RESET_CODE);

	//! Remove bonded devices
	// todo: might not be neccessary if we only use dm in setup mode we can handle it specifically
	//   there. maybe with a mode factory reset
	// todo: remove stack again from if we don't need it here
	Nrf51822BluetoothStack::getInstance().device_manager_reset();

	//! Lastly, go into setup mode after next reset
	State::getInstance().set(STATE_OPERATION_MODE, (uint8_t)OPERATION_MODE_SETUP);

	LOGi("Factory reset done, rebooting device in 2s ...");
	CommandHandler::getInstance().resetDelayed(GPREGRET_SOFT_RESET);
	return true;
}
