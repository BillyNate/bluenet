/*
 * Author: Crownstone Team
 * Copyright: Crownstone (https://crownstone.rocks)
 * Date: May 6, 2015
 * License: LGPLv3+, Apache License 2.0, and/or MIT (triple-licensed)
 */
#pragma once

#include <common/cs_Types.h>
#include <events/cs_EventListener.h>

#define MAX_EVENT_LISTENERS 30

/**
 * Event dispatcher.
 */
class EventDispatcher {

private:
	EventDispatcher();

	//! Array of listeners
	EventListener* _listeners[MAX_EVENT_LISTENERS];

	//! Count of added listeners
	uint16_t _listenerCount;

public:
	static EventDispatcher& getInstance() {
		static EventDispatcher instance;
		return instance;
	}

	EventDispatcher(EventDispatcher const&) = delete;
	void operator=(EventDispatcher const&)  = delete;

	//! Add a listener
	bool addListener(EventListener *listener);

	//! Dispatch an event with data
	void dispatch(event_t & event);
};


