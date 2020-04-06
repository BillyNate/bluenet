/**
 * Author: Crownstone Team
 * Copyright: Crownstone (https://crownstone.rocks)
 * Date: 26 May., 2019
 * License: LGPLv3+, Apache License 2.0, and/or MIT (triple-licensed)
 */

#pragma once

#include <common/cs_Types.h>
#include <ble/cs_iBeacon.h>
#include <events/cs_EventListener.h>

extern "C" {
#include <advertiser.h>
}

/**
 * Buffer for the advertising packet.
 *
 * For some reason the buffer needs to be a bit larger, else we get stuck in an infinite loop in
 * packet_buffer.c::packet_buffer_packets_ready_to_pop()
 */
#define MESH_ADVERTISER_BUF_SIZE (ADVERTISER_PACKET_BUFFER_PACKET_MAXLEN + 4)

class MeshAdvertiser: public EventListener {
public:
	void init();

	/**
	 * Set the advertised MAC address.
	 */
	void setMacAddress(uint8_t* address);

	/**
	 * Set the advertisement interval.
	 */
	void setInterval(uint32_t intervalMs);

	/**
	 * Set the TX power.
	 */
	void setTxPower(int8_t power);

	/**
	 * Start advertising.
	 */
	void start();

	/**
	 * Stop advertising.
	 */
	void stop();

	/**
	 * Start advertising ibeacon.
	 */
	void advertiseIbeacon(uint8_t ibeaconIndex);

	/** Internal usage */
	void handleEvent(event_t & event);

private:
	static const uint8_t num_ibeacon_config_ids = 2;
	advertiser_t* _advertiser = NULL;
	uint8_t* _buffer = NULL;
	adv_packet_t* _advPacket = NULL;
	uint8_t _ibeaconConfigId = 0;
	ibeacon_config_id_packet_t _ibeaconInterval[num_ibeacon_config_ids]; // Timestamp and interval = 0 for invalid entry.

	void updateIbeacon();

	/**
	 * Advertise iBeacon data.
	 *
	 * Updates previous advertisement.
	 */
	void advertise(IBeacon* ibeacon);

	cs_ret_code_t handleSetIbeaconConfig(ibeacon_config_id_packet_t* packet);

	void handleTime(uint32_t now);
};
