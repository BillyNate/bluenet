/*
 * Author: Dominik Egger
 * Copyright: Distributed Organisms B.V. (DoBots)
 * Date: May 6, 2015
 * License: LGPLv3+
 */
#pragma once

//#include <cstddef>

#define VERSION_V2

#include <ble_gap.h>

extern "C" {
#include <third/protocol/rbc_mesh.h>
}

#include <structs/cs_ScanResult.h>
#include <structs/cs_StreamBuffer.h>

enum MeshChannels {
	HUB_CHANNEL     = 1,
	DATA_CHANNEL    = 2,
};

enum MeshMessageTypes {
	//! data channel messages
	CONTROL_MESSAGE          = 0,
	BEACON_MESSAGE           = 1,
	CONFIG_MESSAGE           = 2,
	STATE_MESSAGE            = 3,

	//! hub channel messages
	SCAN_MESSAGE          = 101,
	POWER_SAMPLES_MESSAGE = 102,
	EVENT_MESSAGE         = 103,    // todo: do we need event messages on the mesh at all?
	SERVICE_DATA_MESSAGE  = 104,
};

//! broadcast address is defined as 00:00:00:00:00:00
#define BROADCAST_ADDRESS {}

//! available number of bytes for a mesh message
#define MAX_MESH_MESSAGE_LEN RBC_MESH_VALUE_MAX_LEN
/** number of bytes used for our mesh message header
 *   - target MAC address: 6B
 *   - message type: 2B
 */
#define MAX_MESH_MESSAGE_HEADER_LENGTH BLE_GAP_ADDR_LEN - sizeof(uint16_t)
//! available number of bytes for the mesh message payload. the payload depends on the message type
#define MAX_MESH_MESSAGE_PAYLOAD_LENGTH MAX_MESH_MESSAGE_LEN - MAX_MESH_MESSAGE_HEADER_LENGTH
//! available number of bytes for the data of the message, for command and config messages
#define MAX_MESH_MESSAGE_DATA_LENGTH MAX_MESH_MESSAGE_PAYLOAD_LENGTH - SB_HEADER_SIZE

/** Event mesh message
 */
struct __attribute__((__packed__)) event_mesh_message_t {
	uint16_t event;
//	uint8_t data[MAX_MESH_MESSAGE_PAYLOAD_LENGTH];
};

/** Beacon mesh message
 */
struct __attribute__((__packed__)) beacon_mesh_message_t {
	uint16_t major;
	uint16_t minor;
	ble_uuid128_t uuid;
	int8_t txPower;
};

using control_mesh_message_t = stream_t<uint8_t, MAX_MESH_MESSAGE_DATA_LENGTH>;
using config_mesh_message_t = stream_t<uint8_t, MAX_MESH_MESSAGE_DATA_LENGTH>;
using state_mesh_message_t = stream_t<uint8_t, MAX_MESH_MESSAGE_DATA_LENGTH>;


#define NR_DEVICES_PER_MESSAGE SR_MAX_NR_DEVICES
//#define NR_DEVICES_PER_MESSAGE 1
//#define NR_DEVICES_PER_MESSAGE 10

/** Scan mesh message
 */
struct __attribute__((__packed__)) scan_mesh_message_t {
	uint8_t numDevices;
	peripheral_device_t list[NR_DEVICES_PER_MESSAGE];
};

#define POWER_SAMPLE_MESH_NUM_SAMPLES 43
//! 91 bytes in total
struct __attribute__((__packed__)) power_samples_mesh_message_t {
	uint32_t timestamp;
//	uint16_t firstSample;
//	int8_t sampleDiff[POWER_SAMPLE_MESH_NUM_SAMPLES-1];
	uint16_t samples[POWER_SAMPLE_MESH_NUM_SAMPLES];
	uint8_t reserved;
//	struct __attribute__((__packed__)) {
//		int8_t dt1 : 4;
//		int8_t dt2 : 4;
//	} timeDiffs[POWER_SAMPLE_MESH_NUM_SAMPLES / 2];
};

/** Test mesh message
 */
struct __attribute__((__packed__)) test_mesh_message_t {
	uint32_t counter;
};

struct __attribute__((__packed__)) service_data_mesh_message_t {
	uint16_t crownstoneId;
	uint8_t switchState;
	uint8_t eventBitmask;
	int32_t powerUsage;
	int32_t accumulatedEnergy;
	int32_t temperature;
};


//#ifdef VERSION_V2

/** mesh header
 */
struct __attribute__((__packed__)) mesh_header_v2_t {
	uint16_t crownstoneId;
	uint16_t reason;
	uint16_t userId;
	uint16_t messageType;
};

//#else

/** mesh header
 */
struct __attribute__((__packed__)) mesh_header_t {
	uint8_t address[BLE_GAP_ADDR_LEN];
	uint16_t messageType;
};

//#endif


struct __attribute__((__packed__)) mesh_message_t {
	mesh_header_t header;
	uint8_t payload[1]; // dynamic size
};

/** Device mesh message
 */
struct __attribute__((__packed__)) device_mesh_message_t {
	mesh_header_t header;
	union {
		uint8_t payload[MAX_MESH_MESSAGE_PAYLOAD_LENGTH];
		event_mesh_message_t evtMsg;
		beacon_mesh_message_t beaconMsg;
		control_mesh_message_t commandMsg;
		config_mesh_message_t configMsg;
	};
};

//struct __attribute__((__packed__)) hub_mesh_header_t {
//	uint16_t sourceCrownstoneId;
//	uint16_t messageType;
//};

struct __attribute__((__packed__)) hub_mesh_message_t {
	mesh_header_t header;
	union {
		uint8_t payload[MAX_MESH_MESSAGE_PAYLOAD_LENGTH];
		scan_mesh_message_t scanMsg;
		test_mesh_message_t testMsg;
		power_samples_mesh_message_t powerSamplesMsg;
		service_data_mesh_message_t serviceDataMsg;
	};
};
