/*
 * Author: Crownstone Team
 * Copyright: Crownstone (https://crownstone.rocks)
 * Date: Jan 29, 2020
 * License: LGPLv3+, Apache License 2.0, and/or MIT (triple-licensed)
 */
#pragma once

#include <cfg/cs_Boards.h>
#include <common/cs_Types.h>

/**
 * Class that provides a dimmer.
 * - Determines whether to start on zero crossing.
 * - Controls dimmer enable GPIO.
 * - TODO: take care of synchronizing with net frequency
 */
class Dimmer {
public:
	void init(const boards_config_t& board);

	/**
	 * Start dimmer.
	 *
	 * To be called once there is enough power to enable the dimmer.
	 */
	void start();

	/**
	 * Set dimmer intensity.
	 *
	 * @param[in] intensity       Intensity value to set.
	 * @param[in] immediately     Whether to set the value immediately. False will nicely fade towards the value.
	 * @return true on success.
	 */
	bool set(uint8_t intensity, bool immediately);

	/**
	 * Change the soft of speed.
	 *
	 * TODO: remove this function again once we have a nice default value.
	 */
	void setSoftOnSpeed(uint8_t speed);

private:
	uint32_t hardwareBoard;
	uint8_t pinEnableDimmer;

	TYPIFY(STATE_SOFT_ON_SPEED) softOnfSpeed;

	bool initialized = false;
	bool started = false;
	bool enabled = false;

	void enable();
};


