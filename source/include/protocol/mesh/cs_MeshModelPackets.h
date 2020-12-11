/**
 * Author: Crownstone Team
 * Copyright: Crownstone (https://crownstone.rocks)
 * Date: 7 May., 2019
 * License: LGPLv3+, Apache License 2.0, and/or MIT (triple-licensed)
 */

#pragma once

#include <mesh/cs_MeshDefines.h>
#include <protocol/cs_Typedefs.h>
#include <protocol/cs_CmdSource.h>

/**
 * Message opcodes.
 *
 * Must be in the range 0xC0 - 0xFF
 * See access_opcode_t
 */
enum cs_mesh_model_opcode_t {
	CS_MESH_MODEL_OPCODE_MSG = 0xC0,
	CS_MESH_MODEL_OPCODE_UNICAST_RELIABLE_MSG = 0xC1,
	CS_MESH_MODEL_OPCODE_UNICAST_REPLY = 0xC2,
	CS_MESH_MODEL_OPCODE_MULTICAST_RELIABLE_MSG = 0xC3,
	CS_MESH_MODEL_OPCODE_MULTICAST_REPLY = 0xC4,
};

/**
 * Max message size.
 * When you send packets that are longer than 15 bytes (including opCode of 1-3B, and MIC of 4 or 8B), they will be sent
 * as segmented packets of 12 byte each.
 * See https://devzone.nordicsemi.com/f/nordic-q-a/32854/max-size-of-data-to-send-from-one-node-to-other-ble-mesh
 * The minimum advertising interval that mesh are using now is 20ms, so each advertisement / segment, takes 20ms.
 */
#define MAX_MESH_MSG_SIZE (3 * 12 - 4 - 3)
#define MAX_MESH_MSG_NON_SEGMENTED_SIZE (15 - 4 - 3)

/**
 * Size of the header of each mesh model message.
 * 1B for the message type.
 */
#define MESH_HEADER_SIZE 1

enum cs_mesh_model_msg_type_t {
	CS_MESH_MODEL_TYPE_TEST                      = 0,  // Payload: cs_mesh_model_msg_test_t
	CS_MESH_MODEL_TYPE_ACK                       = 1,  // Payload: none
	CS_MESH_MODEL_TYPE_CMD_TIME                  = 3,  // Payload: cs_mesh_model_msg_time_t
	CS_MESH_MODEL_TYPE_CMD_NOOP                  = 4,  // Payload: none
	CS_MESH_MODEL_TYPE_CMD_MULTI_SWITCH          = 5,  // Payload: cs_mesh_model_msg_multi_switch_item_t
//	CS_MESH_MODEL_TYPE_CMD_KEEP_ALIVE_STATE      = 6,  // Payload: keep_alive_state_item_t
//	CS_MESH_MODEL_TYPE_CMD_KEEP_ALIVE            = 7,  // Payload: none
	CS_MESH_MODEL_TYPE_STATE_0                   = 8,  // Payload: cs_mesh_model_msg_state_0_t
	CS_MESH_MODEL_TYPE_STATE_1                   = 9,  // Payload: cs_mesh_model_msg_state_1_t
	CS_MESH_MODEL_TYPE_PROFILE_LOCATION          = 10, // Payload: cs_mesh_model_msg_profile_location_t
	CS_MESH_MODEL_TYPE_SET_BEHAVIOUR_SETTINGS    = 11, // Payload: behaviour_settings_t
	CS_MESH_MODEL_TYPE_TRACKED_DEVICE_REGISTER   = 12, // Payload: cs_mesh_model_msg_device_register_t
	CS_MESH_MODEL_TYPE_TRACKED_DEVICE_TOKEN      = 13, // Payload: cs_mesh_model_msg_device_token_t
	CS_MESH_MODEL_TYPE_SYNC_REQUEST              = 14, // Payload: cs_mesh_model_msg_sync_request_t
//	CS_MESH_MODEL_TYPE_SYNC_RESPONSE             = 15, // Payload: cs_mesh_model_msg_sync_response_t
	CS_MESH_MODEL_TYPE_TRACKED_DEVICE_LIST_SIZE  = 16, // Payload: cs_mesh_model_msg_device_list_size_t
	CS_MESH_MODEL_TYPE_STATE_SET                 = 17, // Payload: cs_mesh_model_msg_state_header_ext_t + payload
	CS_MESH_MODEL_TYPE_RESULT                    = 18, // Payload: cs_mesh_model_msg_result_header_t + payload
	CS_MESH_MODEL_TYPE_SET_IBEACON_CONFIG_ID     = 19, // Payload: set_ibeacon_config_id_packet_t
	CS_MESH_MODEL_TYPE_TRACKED_DEVICE_HEARTBEAT  = 20, // Payload: cs_mesh_model_msg_device_heartbeat_t
	CS_MESH_MODEL_TYPE_RSSI_PING                 = 21, // Payload: rssi_ping_message_t
	CS_MESH_MODEL_TYPE_TIME_SYNC                 = 22, // Payload: cs_mesh_model_msg_time_sync_t
	CS_MESH_MODEL_TYPE_NEAREST_WITNESS_REPORT    = 23, // Payload: nearest_witness_report_t

	CS_MESH_MODEL_TYPE_UNKNOWN                   = 255
};

struct __attribute__((__packed__)) cs_mesh_model_msg_test_t {
	uint32_t counter;
#if MESH_MODEL_TEST_MSG == 2
//	uint8_t dummy[3]; // non segmented
//	uint8_t dummy[12]; // 2 segments
	uint8_t dummy[24]; // 3 segments
#else
	uint8_t dummy[3]; // non segmented
#endif
};

struct __attribute__((__packed__)) cs_mesh_model_msg_time_t {
	uint32_t timestamp;
};

struct __attribute__((__packed__)) cs_mesh_model_msg_profile_location_t {
	uint8_t profile;
	uint8_t location;
};

struct __attribute__((__packed__)) cs_mesh_model_msg_state_0_t {
	uint8_t switchState;
	uint8_t flags;
	int8_t powerFactor;
	int16_t powerUsageReal;
	uint16_t partialTimestamp;
};

struct __attribute__((__packed__)) cs_mesh_model_msg_state_1_t {
	int8_t temperature;
	int32_t energyUsed;
	uint16_t partialTimestamp;
};

struct __attribute__((__packed__)) cs_mesh_model_msg_multi_switch_item_t {
	stone_id_t id;
	uint8_t switchCmd;
	uint16_t delay = 0; // Deprecated, set to 0 or backwards compatibility.
	cmd_source_with_counter_t source;
};

struct __attribute__((__packed__)) cs_mesh_model_msg_device_register_t {
	device_id_t deviceId;
	uint8_t locationId;
	uint8_t profileId;
	int8_t rssiOffset;
	uint8_t flags;
	uint8_t accessLevel;
};

struct __attribute__((__packed__)) cs_mesh_model_msg_device_token_t {
	device_id_t deviceId;
	uint8_t deviceToken[TRACKED_DEVICE_TOKEN_SIZE];
	uint16_t ttlMinutes;
};

struct __attribute__((__packed__)) cs_mesh_model_msg_device_heartbeat_t {
	device_id_t deviceId;
	uint8_t locationId;
	uint8_t ttlMinutes;
};

struct __attribute__((__packed__)) cs_mesh_model_msg_device_list_size_t {
	uint8_t listSize;
};

struct __attribute__((__packed__)) cs_mesh_model_msg_sync_request_t {
	/**
	 * ID of crownstone that requests the data
	 *
	 * Not really necessary, as the source address is also the crownstone id.
	 */
	stone_id_t id;
	union __attribute__((__packed__)) {
		struct __attribute__((packed)) {
			bool time           : 1;
			bool trackedDevices : 1;
		} bits;
		uint32_t bitmask;
	};
};

struct __attribute__((__packed__)) cs_mesh_model_msg_state_header_t {
	uint8_t type;                 // Shortened version of CS_TYPE
	uint8_t id : 6;               // Shortened version of state id.
	uint8_t persistenceMode : 2;  // Shortened version of peristenceMode.
};

struct __attribute__((__packed__)) cs_mesh_model_msg_state_header_ext_t {
	cs_mesh_model_msg_state_header_t header;
	uint8_t accessLevel : 3;      // Shortened version of access level.
	uint8_t sourceId : 5;         // Shortened version of source.
};

struct __attribute__((__packed__)) cs_mesh_model_msg_result_header_t {
	uint8_t msgType; // Mesh msg type of which this is the result.
	uint8_t retCode;
};

/**
 * Packed version of time_sync_message_t.
 */
struct __attribute__((__packed__)) cs_mesh_model_msg_time_sync_t {
	uint32_t posix_s;        // Seconds since epoch.
	uint16_t posix_ms : 10;  // Milliseconds passed since posix_s.
	uint8_t version : 6;     // Synchronization version,
	bool overrideRoot : 1;   // Whether this time overrides the root time.
	uint8_t reserved : 7;    // @arend maybe use these bits to increase version size.
};

/**
 * Packed version of NearestWitnessReport.
 */
struct __attribute__((__packed__)) nearest_witness_report_t {
	uint8_t trackable_device_mac[6];
	int8_t rssi;
};
