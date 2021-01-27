#include <localisation/cs_MeshTopology.h>

#include <common/cs_Types.h>
#include <logging/cs_Logger.h>
#include <drivers/cs_Timer.h>
#include <events/cs_Event.h>
#include <storage/cs_State.h>
#include <time/cs_SystemTime.h>
#include <test/cs_Test.h>

#include <cmath>

// REVIEW: This won't be recognized by binary logger.
#define LOGMeshTopologyDebug LOGd
#define LOGMeshTopologyVerbose LOGnone

// ------------ MeshTopology methods ------------

MeshTopology::MeshTopology() :
		flushRoutine([this]() {
			return flushAggregatedRssiData();
		}) {
}

void MeshTopology::init() {
	// REVIEW: Use TYPIFY for variable.
	State::getInstance().get(CS_TYPE::CONFIG_CROWNSTONE_ID, &my_id,
			sizeof(my_id));
	LOGMeshTopologyDebug("MeshTopology: my_id %d", my_id);

	boot_sequence_finished = false;
	last_stone_id_broadcasted_in_burst = 0;
}

// ------------ Recording ping stuff ------------


void MeshTopology::recordRssiValue(stone_id_t sender_id, int8_t rssi, uint8_t channel) {
	auto channel_index = channel - CHANNEL_START;

	if (channel_index < 0 || CHANNEL_COUNT <= channel_index) {
		return;
	}

	auto& recorder = variance_map[channel_index][sender_id];

	recorder.addValue(rssi);
}

// ------------ Sending Rssi Data ------------

uint8_t MeshTopology::getVarianceRepresentation(float variance) {
	variance = std::abs(variance);
	if (variance <  2 *  2) return 0;
	if (variance <  4 *  4) return 1;
	if (variance <  6 *  6) return 2;
	if (variance <  8 *  8) return 3;
	if (variance < 10 * 10) return 4;
	if (variance < 15 * 15) return 5;
	if (variance < 20 * 15) return 6;
	return 7;
}

uint8_t MeshTopology::getMeanRssiRepresentation(float mean) {
	mean = std::abs(mean);
	if (mean >= 1<<7 ) {
		return (1<<7) - 1;
	}
	return static_cast<uint8_t>(mean);
}

uint8_t MeshTopology::getCountRepresentation(uint32_t count) {
	if (count >= 1<<6) {
		return (1<< 6) - 1;
	}
	return count;
}


uint32_t MeshTopology::flushAggregatedRssiData() {
	if (!boot_sequence_finished) {
		LOGMeshTopologyDebug("flushAggregatedRssiData boot delay");
		boot_sequence_finished = true;
		return Coroutine::delayMs(Settings.boot_sequence_period_ms);
	}

	LOGMeshTopologyDebug("flushAggregatedRssiData");
	// start flushing phase, here we wait quite a bit shorter until the map is empty.

	// ** begin burst loop **
	for (auto main_iter = variance_map[0].upper_bound(last_stone_id_broadcasted_in_burst);
			main_iter != variance_map[0].end(); ++main_iter) {

		stone_id_t id = main_iter->first;

		LOGMeshTopologyDebug("Burst start for id=%u", id);

		// the maps may not have the same key sets, this depends
		// on possible loss differences between the channels.

		// Intersecting the key sets of these maps is pure ugly.
		// Fortunately we only doing this once every half hour or so.
		decltype(main_iter) rec_iters[] = {
				main_iter,
				variance_map[1].find(id),
				variance_map[2].find(id),
		};

		bool all_maps_have_sufficient_data_for_id = true;
		for (auto i = 0; i < CHANNEL_COUNT; ++i) {
			if (rec_iters[i] == variance_map[i].end() ||
					rec_iters[i]->second.getCount() < Settings.min_samples_to_trigger_burst) {
				all_maps_have_sufficient_data_for_id = false;
				break;
			}
		}

		if (all_maps_have_sufficient_data_for_id) {
			rssi_data_message_t rssi_data;

			rssi_data.sender_id = id;

			rssi_data.channel37.sampleCount = getCountRepresentation(rec_iters[0]->second.getCount());
			rssi_data.channel38.sampleCount = getCountRepresentation(rec_iters[1]->second.getCount());
			rssi_data.channel39.sampleCount = getCountRepresentation(rec_iters[2]->second.getCount());

			rssi_data.channel37.rssi = getMeanRssiRepresentation(rec_iters[0]->second.getMean());
			rssi_data.channel38.rssi = getMeanRssiRepresentation(rec_iters[1]->second.getMean());
			rssi_data.channel39.rssi = getMeanRssiRepresentation(rec_iters[2]->second.getMean());

			rssi_data.channel37.variance = getVarianceRepresentation(rec_iters[0]->second.getVariance());
			rssi_data.channel38.variance = getVarianceRepresentation(rec_iters[1]->second.getVariance());
			rssi_data.channel39.variance = getVarianceRepresentation(rec_iters[2]->second.getVariance());

			sendRssiDataOverMesh(&rssi_data);

			// delete entry from map
			for (auto i = 0; i < 3; ++i) {
				variance_map[i].erase(rec_iters[i]);
				// this invalidates main_iter, so we _must_ return after deleting.
			}

			last_stone_id_broadcasted_in_burst = id;

			return Coroutine::delayMs(Settings.burst_period_ms);
		}

	} // ** end burst loop **

	LOGMeshTopologyDebug("End of burst");

	last_stone_id_broadcasted_in_burst = 0;

	return Coroutine::delayMs(Settings.accumulation_period_ms);
}

// --------------- generating rssi data --------------

void MeshTopology::sendPingRequestOverMesh() {
	rssi_ping_message_t ping_msg;

	cs_mesh_msg_t msg_wrapper;
	msg_wrapper.type = CS_MESH_MODEL_TYPE_RSSI_PING;
	msg_wrapper.reliability = CS_MESH_RELIABILITY_LOW;
	msg_wrapper.urgency = CS_MESH_URGENCY_LOW;

	msg_wrapper.payload = reinterpret_cast<uint8_t*>(&ping_msg);
	msg_wrapper.size = sizeof(ping_msg);

	event_t msgevt(CS_TYPE::CMD_SEND_MESH_MSG, &msg_wrapper,
			sizeof(cs_mesh_msg_t));

	msgevt.dispatch();
}

void MeshTopology::sendPingResponseOverMesh() {
	cs_mesh_msg_t msg_wrapper;
	msg_wrapper.type = CS_MESH_MODEL_TYPE_CMD_NOOP;
	msg_wrapper.reliability = CS_MESH_RELIABILITY_LOW;
	msg_wrapper.urgency = CS_MESH_URGENCY_LOW;

	msg_wrapper.payload = nullptr;
	msg_wrapper.size = 0;

	event_t msgevt(CS_TYPE::CMD_SEND_MESH_MSG, &msg_wrapper,
			sizeof(cs_mesh_msg_t));

	msgevt.dispatch();
}

void MeshTopology::receivePingMessage(MeshMsgEvent& meshMsgEvent) {
	if (meshMsgEvent.hops == 0) {
		sendPingResponseOverMesh();
	}
}

// ------------- communicating rssi data -------------

void MeshTopology::sendRssiDataOverMesh(rssi_data_message_t* rssi_data_message) {
	cs_mesh_msg_t msg_wrapper;
	msg_wrapper.type = CS_MESH_MODEL_TYPE_RSSI_DATA;
	msg_wrapper.reliability = CS_MESH_RELIABILITY_LOW;
	msg_wrapper.urgency = CS_MESH_URGENCY_LOW;

	msg_wrapper.payload = reinterpret_cast<uint8_t*>(rssi_data_message);
	msg_wrapper.size = sizeof(*rssi_data_message);

	event_t msgevt(CS_TYPE::CMD_SEND_MESH_MSG, &msg_wrapper,
			sizeof(cs_mesh_msg_t));

	msgevt.dispatch();
}

// REVIEW: Why not a simpler message (without bit fields) for uart?
void MeshTopology::sendRssiDataOverUart(rssi_data_message_t* rssi_data_message) {
	RssiDataMessage datamessage;

	datamessage.receiver_id = my_id;
	datamessage.sender_id = rssi_data_message->sender_id;

	datamessage.count[0] = rssi_data_message->channel37.sampleCount;
	datamessage.count[1] = rssi_data_message->channel38.sampleCount;
	datamessage.count[2] = rssi_data_message->channel39.sampleCount;

	datamessage.rssi[0] = rssi_data_message->channel37.rssi;
	datamessage.rssi[1] = rssi_data_message->channel38.rssi;
	datamessage.rssi[2] = rssi_data_message->channel39.rssi;

	datamessage.standard_deviation[0] = rssi_data_message->channel37.variance;
	datamessage.standard_deviation[1] = rssi_data_message->channel38.variance;
	datamessage.standard_deviation[2] = rssi_data_message->channel39.variance;

	LOGMeshTopologyDebug("%d -> %d: ch37 #%d  -%d dB",
			datamessage.sender_id,
			datamessage.receiver_id,
			datamessage.count[0],
			datamessage.rssi[0],
			datamessage.standard_deviation[0]
	);

	UartHandler::getInstance().writeMsg(
			UART_OPCODE_TX_RSSI_DATA_MESSAGE,
			reinterpret_cast<uint8_t*>(&datamessage),
			sizeof(datamessage));
}

void MeshTopology::receiveRssiDataMessage(MeshMsgEvent& meshMsgEvent) {
	auto& rssi_data_message =
			meshMsgEvent.getPacket<CS_MESH_MODEL_TYPE_RSSI_DATA>();
	sendRssiDataOverUart(&rssi_data_message);
}

// ------------- recording mesh messages -------------

void MeshTopology::receiveMeshMsgEvent(MeshMsgEvent& mesh_msg_evt) {
	// REVIEW: Mesh specific code, should be in the mesh code.
	if (mesh_msg_evt.hops == 0) { // TODO: 0 hops, or 1 hops?!
		LOGMeshTopologyVerbose("handle mesh msg event with 0 hops");
		recordRssiValue(
				mesh_msg_evt.srcAddress,
				mesh_msg_evt.rssi,
				mesh_msg_evt.channel
				);
	}

	// can't interpret the rssi value in this case.
}

void MeshTopology::handleEvent(event_t &evt) {
	if (flushRoutine.handleEvent(evt)) {
		return;
	}

	if (evt.type == CS_TYPE::EVT_RECV_MESH_MSG) {
		auto& meshMsgEvent = *CS_TYPE_CAST(EVT_RECV_MESH_MSG, evt.getData());

		receiveMeshMsgEvent(meshMsgEvent);

		switch (meshMsgEvent.type) {
			case CS_MESH_MODEL_TYPE_RSSI_PING: {
				LOGMeshTopologyDebug("received rssi ping message");
				receivePingMessage(meshMsgEvent);
				break;
			}
			case CS_MESH_MODEL_TYPE_RSSI_DATA: {
				LOGMeshTopologyDebug("received rssi data message");
				receiveRssiDataMessage(meshMsgEvent);
				break;
			}
			default: {
				break;
			}
		}
	}

	return;
}
