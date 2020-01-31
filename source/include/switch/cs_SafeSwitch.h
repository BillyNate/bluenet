/*
 * Author: Crownstone Team
 * Copyright: Crownstone (https://crownstone.rocks)
 * Date: Jan 28, 2020
 * License: LGPLv3+, Apache License 2.0, and/or MIT (triple-licensed)
 */
#pragma once

#include <common/cs_Types.h>
#include <drivers/cs_Dimmer.h>
#include <drivers/cs_Relay.h>
#include <events/cs_EventListener.h>
#include <third/std/function.h>

/**
 * Class that takes care of safety of switch.
 * - Checks if dimmer circuit is powered.
 * - Handles hardware errors.
 * - Checks which hardware board can do what.
 */
class SafeSwitch : public EventListener {
public:
	void init(const boards_config_t& board);

	/**
	 * Start switch.
	 *
	 * To be called once there is enough power to switch relay, and enable dimmer.
	 */
	void start();

	/**
	 * Set relay.
	 *
	 * It could be that something else is done instead, check the error code.
	 *
	 * @return     Error code: if not successful, check what the current state is.
	 */
	cs_ret_code_t setRelay(bool on);

	/**
	 * Set dimmer intensity.
	 *
	 * It could be that something else is done instead, check the error code.
	 *
	 * @return     Error code: if not successful, check what the current state is.
	 */
	cs_ret_code_t setDimmer(uint8_t intensity);

	/**
	 * Get current switch state.
	 *
	 * @return     Current switch state.
	 */
	switch_state_t getState();

	/**
	 * Callback function definition.
	 */
	typedef function<void(switch_state_t newState)> callback_on_state_change_t;

	/**
	 * Register a callback function that's called when state changes unexpectedly.
	 */
	void onUnexpextedStateChange(const callback_on_state_change_t& closure);

	/**
	 * Handle events.
	 */
	void handleEvent(event_t& evt) override;

private:
	Dimmer dimmer;
	Relay relay;

	bool initialized = false;

	uint32_t hardwareBoard;

	/**
	 * Current state of relay and dimmer.
	 */
	switch_state_t currentState;

	callback_on_state_change_t callbackOnStateChange;

	/**
	 * Whether the relay circuit is powered.
	 */
	bool relayPowered = false;

	/**
	 * Whether the dimmer circuit is powered.
	 */
	bool dimmerPowered = false;

	/**
	 * Counter that counts down until enough time has passed for dimmer circuit to be powered.
	 */
	uint32_t dimmerPowerUpCountDown = PWM_BOOT_DELAY_MS / TICK_INTERVAL_MS;

	/**
	 * Counter that counts down until power usage via dimmer should be checked.
	 */
	uint32_t dimmerCheckCountDown = 0;

	/**
	 * Whether power usage via dimmer has been checked already.
	 */
	bool checkedDimmerPowerUsage = false;

	/**
	 * Set relay.
	 *
	 * Checks: similar.
	 * Does not check: safe, powered.
	 */
	cs_ret_code_t setRelayUnchecked(bool on);

	/**
	 * Set dimmer intensity.
	 *
	 * Checks: similar.
	 * Does not check: safe, powered.
	 */
	cs_ret_code_t setDimmerUnchecked(uint8_t intensity);

	/**
	 * Try to see if dimmer is powered:
	 * Set dimmer intensity, and start power usage check.
	 *
	 * Sets dimmer powered.
	 * Does not check: safe, powered.
	 */
	cs_ret_code_t startDimmerPowerCheck(uint8_t intensity);

	/**
	 * Cancel power usage check.
	 *
	 * Marks dimmer not powered.
	 */
	void cancelDimmerPowerCheck();

	/**
	 * Check if power is used due to dimmer being on.
	 *
	 * Marks dimmer not powered if check fails.
	 */
	void checkDimmerPower();

	/**
	 * Called when enough time has passed for the dimmer to be powered.
	 *
	 * Marks dimmer powered.
	 */
	void dimmerPoweredUp();

	/**
	 * Set dimmerPowered and sends event.
	 */
	void setDimmerPowered(bool powered);

	/**
	 * Turn relay and dimmer off, without any checks.
	 * Also send an event.
	 */
	void forceSwitchOff();

	/**
	 * Turn relay on and dimmer off, without any checks.
	 * Also send an event.
	 */
	void forceRelayOnAndDimmerOff();

	/**
	 * Get state errors from State.
	 */
	state_errors_t getErrorState();

	bool isSwitchOverLoaded(state_errors_t stateErrors);

	bool hasDimmerError(state_errors_t stateErrors);

	bool isSafeToDim(state_errors_t stateErrors);

	bool isSafeToTurnRelayOn(state_errors_t stateErrors);

	bool isSafeToTurnRelayOff(state_errors_t stateErrors);

	/**
	 * Send state update to listeners.
	 * Only to be called when state changes due to events, not due to calls to setRelay() or setDimmer().
	 * This prevents the user to get updates before the call is even finished, this is what return codes are for.
	 */
	void sendUnexpectedStateUpdate();

	/**
	 * To be called when system is going to DFU mode.
	 */
	void goingToDfu();

	void handleGetBehaviourDebug(event_t& evt);
};


