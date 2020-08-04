/*
 * Author: Crownstone Team
 * Copyright: Crownstone (https://crownstone.rocks)
 * Date: Jan 28, 2020
 * License: LGPLv3+, Apache License 2.0, and/or MIT (triple-licensed)
 */

#include <events/cs_Event.h>
#include <storage/cs_StateData.h>
#include <storage/cs_State.h>
#include <switch/cs_SmartSwitch.h>

#define LOGSmartSwitchDebug LOGnone

void SmartSwitch::init(const boards_config_t& board) {
	State::getInstance().get(CS_TYPE::CONFIG_PWM_ALLOWED, &allowDimming, sizeof(allowDimming));

	TYPIFY(CONFIG_SWITCH_LOCKED) switchLocked;
	State::getInstance().get(CS_TYPE::CONFIG_SWITCH_LOCKED, &switchLocked, sizeof(switchLocked));
	_allowSwitching = !switchLocked;

	// Load intended state.
	State::getInstance().get(CS_TYPE::STATE_SWITCH_STATE, &storedState, sizeof(storedState));
	intendedState = getIntensityFromSwitchState(storedState);
	LOGi("init stored=%u, intended=%u allowDimming=%u _allowSwitching=%u", storedState.asInt, intendedState, allowDimming, _allowSwitching);


	safeSwitch.onUnexpextedStateChange([&](switch_state_t newState) -> void {
		handleUnexpectedStateChange(newState);
	});

	safeSwitch.init(board);

	listen();
}

void SmartSwitch::start() {
	LOGd("start");
	// Start safe switch first, then set stored intended state.
	safeSwitch.start();
	LOGd("restore intended state: %u", intendedState);
	allowSwitchingOverride = true;
	set(intendedState);
	allowSwitchingOverride = false;
}

switch_state_t SmartSwitch::getActualState() {
	return safeSwitch.getState();
}

uint8_t SmartSwitch::getIntensityFromSwitchState(switch_state_t switchState) {
	if (switchState.state.relay) {
		return 100;
	}
	return switchState.state.dimmer;
}

uint8_t SmartSwitch::getCurrentIntensity() {
	auto currentState = getActualState();
	return getIntensityFromSwitchState(currentState);
}

uint8_t SmartSwitch::getIntendedState() {
	return intendedState;
}

bool SmartSwitch::allowSwitching(){
	return _allowSwitching || allowSwitchingOverride;
}

cs_ret_code_t SmartSwitch::set(uint8_t intensity) {
	LOGSmartSwitchDebug("set %u", intensity);
	if (intensity > 100) {
		intensity = 100;
//		return ERR_WRONG_PARAMETER;
	}
	intendedState = intensity;
	return resolveIntendedState();
}

cs_ret_code_t SmartSwitch::resolveIntendedState() {
	switch_state_t currentState = getActualState();
	LOGSmartSwitchDebug("resolveIntendedState intended=%u current=(%u %u%%)", intendedState, currentState.state.relay, currentState.state.dimmer);

	if (intendedState == 0) {
		// Set dimmer to 0 and turn relay off.
		cs_ret_code_t retCode = ERR_SUCCESS;
		cs_ret_code_t retCodeDimmer = setDimmer(0);
		if (retCodeDimmer != ERR_SUCCESS) {
			retCode = retCodeDimmer;
		}

		cs_ret_code_t retCodeRelay = setRelay(false);
		if (retCodeRelay != ERR_SUCCESS) {
			retCode = retCodeRelay;
		}
		LOGSmartSwitchDebug("retCodeDimmer=%u retCodeRelay=%u", retCodeDimmer, retCodeRelay);
		return retCode;
	}
	else if (intendedState == 100) {
		// Set dimmer to 100 or turn relay on.
		if (currentState.state.relay && safeSwitch.isRelayStateAccurate()) {
			LOGSmartSwitchDebug("already on");
			return ERR_SUCCESS;
		} else if (!safeSwitch.isRelayStateAccurate()) {
			// Try the dimmer first, if that is already online there won't be any audible click.
			// If it isn't active yet, or the setDimmer call below doesn't succeed we'll try relay.
		}

		// Try to set dimmer to 100.
		cs_ret_code_t retCode = setDimmer(100);
		if (retCode != ERR_SUCCESS) {
			// If that doesn't work, turn relay on instead.
			LOGSmartSwitchDebug("Turn on relay instead.");
			retCode = setRelay(true);
			// Don't forget to turn off the dimmer, as it may have been on already.
			setDimmer(0);
		}
		LOGSmartSwitchDebug("retCode=%u", retCode);
		return retCode;
	}
	else {
		// Try to set dimmed value, and then turn relay off.
		// If that doesn't work, turn on relay instead.
		// Only fade if relay isn't on right now (else you get the effect that you go from 100% to 0%, then fade to N%).
		bool fade = !(currentState.state.relay && safeSwitch.isRelayStateAccurate());
		cs_ret_code_t retCode = setDimmer(intendedState, fade);
		LOGSmartSwitchDebug("allowDimming=%u", allowDimming);
		if (retCode == ERR_SUCCESS) {
			retCode = setRelay(false);
			if (retCode != ERR_SUCCESS) {
				LOGSmartSwitchDebug("Relay could not be turned off: turn dimmer off.");
				setDimmer(0);
			}
		} else {
			// This branch will also be reached when trying to change the value of a locked dimmable crownstone
			// in that case, setRelay will also refuse to change value, so everything is fine.

			LOGSmartSwitchDebug("setDimmer unsuccessful, try to turn on relay instead.");
			setRelay(true);
			// Don't forget to turn off the dimmer, as it may have been on already.
			setDimmer(0);
		}
		LOGSmartSwitchDebug("retCode=%u", retCode);
		return retCode;
	}
}

cs_ret_code_t SmartSwitch::setRelay(bool on) {
	switch_state_t currentState = getActualState();
	LOGSmartSwitchDebug("setRelay %u currenState=%u allowSwitching()=%u", on, currentState.asInt, allowSwitching() );
	
	if (currentState.state.relay == on && safeSwitch.isRelayStateAccurate()) {
		return ERR_SUCCESS;
	}
	// Check allow switching AFTER similarity, else error is returned while same value is set.
	if (!allowSwitching()) {
		return ERR_NO_ACCESS;
	}
	return setRelayUnchecked(on);
}

cs_ret_code_t SmartSwitch::setRelayUnchecked(bool on) {
	cs_ret_code_t retCode = safeSwitch.setRelay(on);
	LOGSmartSwitchDebug("setRelayUnchecked %u retCode=%u", on, retCode);
	switch_state_t currentState = getActualState();
	store(currentState);
	return retCode;
}

cs_ret_code_t SmartSwitch::setDimmer(uint8_t intensity, bool fade) {
	switch_state_t currentState = getActualState();
	LOGSmartSwitchDebug("setDimmer %u fade=%u currenState=%u allowSwitching()=%u allowDimming=%u", intensity, fade, currentState.asInt, allowSwitching(), allowDimming);

	if (!allowDimming && intensity > 0) {
		return ERR_NO_ACCESS;
	}
	// Check similarity AFTER allow dimming, else success is returned when dimming allowed is set to false, while already dimming.
	if (currentState.state.dimmer == intensity) {
		return ERR_SUCCESS;
	}
	// Check allow switching AFTER similarity, else error is returned while same value is set.
	if (!allowSwitching()) {
		return ERR_NO_ACCESS;
	}
	return setDimmerUnchecked(intensity, fade);
}

cs_ret_code_t SmartSwitch::setDimmerUnchecked(uint8_t intensity, bool fade) {
	cs_ret_code_t retCode = safeSwitch.setDimmer(intensity, fade);
	LOGSmartSwitchDebug("setDimmerUnchecked %u fade=%u retCode=%u", intensity, fade, retCode);
	switch_state_t currentState = getActualState();
	store(currentState);
	return retCode;
}



void SmartSwitch::handleUnexpectedStateChange(switch_state_t newState) {
	store(newState);
	uint8_t intensity = getIntensityFromSwitchState(newState);
	sendUnexpectedIntensityUpdate(intensity);
}

void SmartSwitch::onUnexpextedIntensityChange(const callback_on_intensity_change_t& closure) {
	callbackOnIntensityChange = closure;
}

void SmartSwitch::sendUnexpectedIntensityUpdate(uint8_t newIntensity) {
	LOGd("sendUnexpectedIntensityUpdate");
	callbackOnIntensityChange(newIntensity);
}


void SmartSwitch::store(switch_state_t newState) {
	if (newState.asInt == storedState.asInt) {
		return;
	}
	LOGd("store(%u, %u%%)", newState.state.relay, newState.state.dimmer);
	storedState = newState;

	// Never persist immediately, as another store() call might follow soon.
	cs_state_data_t stateData(CS_TYPE::STATE_SWITCH_STATE, reinterpret_cast<uint8_t*>(&newState), sizeof(newState));
	State::getInstance().setDelayed(stateData, SWITCH_DELAYED_STORE_MS / 1000);
}



cs_ret_code_t SmartSwitch::setAllowSwitching(bool allowed) {
	LOGi("setAllowSwitching %u", allowed);
	_allowSwitching = allowed;

	// must use the actual persisted value _allowSwitching rather than the returnvalue of allowSwitching here!
	TYPIFY(CONFIG_SWITCH_LOCKED) switchLocked = !_allowSwitching;
	State::getInstance().set(CS_TYPE::CONFIG_SWITCH_LOCKED, &switchLocked, sizeof(switchLocked));
	return ERR_SUCCESS;
}

cs_ret_code_t SmartSwitch::setAllowDimming(bool allowed) {
	LOGi("setAllowDimming %u", allowed);
	allowDimming = allowed;
	State::getInstance().set(CS_TYPE::CONFIG_PWM_ALLOWED, &allowDimming, sizeof(allowDimming));

	// This will trigger event CONFIG_PWM_ALLOWED, which will call handleAllowDimmingSet().
//	handleAllowDimmingSet();
	return ERR_SUCCESS;
}

void SmartSwitch::handleAllowDimmingSet() {
	// Update the switch state, as it may need to be changed.
	[[maybe_unused]] cs_ret_code_t retCode = resolveIntendedState();
	uint8_t newIntensity = getCurrentIntensity();
	if (newIntensity != intendedState) {
		sendUnexpectedIntensityUpdate(newIntensity);
	}
}

cs_ret_code_t SmartSwitch::handleCommandSetRelay(bool on) {
	LOGi("handleCommandSetRelay %u", on);
	[[maybe_unused]] cs_ret_code_t retCode = setRelay(on);
	uint8_t newIntensity = getCurrentIntensity();
	if (newIntensity != intendedState) {
		sendUnexpectedIntensityUpdate(newIntensity);
	}
	return retCode;
}

cs_ret_code_t SmartSwitch::handleCommandSetDimmer(uint8_t intensity) {
	LOGi("handleCommandSetDimmer %u", intensity);
	[[maybe_unused]] cs_ret_code_t retCode = setDimmer(intensity);
	uint8_t newIntensity = getCurrentIntensity();
	if (newIntensity != intendedState) {
		sendUnexpectedIntensityUpdate(newIntensity);
	}
	return retCode;
}


void SmartSwitch::handleEvent(event_t& evt) {
	switch (evt.type) {
		case CS_TYPE::CMD_SWITCHING_ALLOWED: {
			auto allowed = reinterpret_cast<TYPIFY(CMD_SWITCHING_ALLOWED)*>(evt.data);
			evt.result.returnCode = setAllowSwitching(*allowed);
			break;
		}
		case CS_TYPE::CMD_DIMMING_ALLOWED: {
			auto allowed = reinterpret_cast<TYPIFY(CMD_DIMMING_ALLOWED)*>(evt.data);
			evt.result.returnCode = setAllowDimming(*allowed);
			break;
		}
		case CS_TYPE::CMD_SET_RELAY: {
			auto turnOn = reinterpret_cast<TYPIFY(CMD_SET_RELAY)*>(evt.data);
			evt.result.returnCode = handleCommandSetRelay(*turnOn);
			break;
		}
		case CS_TYPE::CMD_SET_DIMMER: {
			auto intensity = reinterpret_cast<TYPIFY(CMD_SET_DIMMER)*>(evt.data);
			evt.result.returnCode = handleCommandSetDimmer(*intensity);
			break;
		}
		case CS_TYPE::EVT_DIMMER_POWERED: {
			// Fade to intended intensity?
			break;
		}
		case CS_TYPE::CONFIG_PWM_ALLOWED: {
			allowDimming = *reinterpret_cast<TYPIFY(CONFIG_PWM_ALLOWED)*>(evt.data);
			handleAllowDimmingSet();
			break;
		}
		default:
			break;
	}
}
