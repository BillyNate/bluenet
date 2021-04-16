/*
 * Author: Crownstone Team
 * Copyright: Crownstone (https://crownstone.rocks)
 * Date: Apr 14, 2021
 * License: LGPLv3+, Apache License 2.0, and/or MIT (triple-licensed)
 */

#pragma once

#include <events/cs_EventListener.h>

class TestCentral: EventListener {
public:
	TestCentral();
	void init();

	void connect();
	void discover();
	void onDiscovery(ble_central_discovery_t& packet);
	void read();
	void write();
	void disconnect();

	void handleEvent(event_t & event);
private:
	uint16_t _fwVersionHandle;
	uint16_t _controlHandle;
};

