#pragma once

#include <cs_MicroappStructs.h>
#include <events/cs_EventListener.h>
#include <structs/buffer/cs_CircularBuffer.h>

extern "C" {
#include <util/cs_DoubleStackCoroutine.h>
}

typedef struct {
	coroutine* c;
	int cntr;
	int delay;
} coargs;

// Call loop every 10 ticks. The ticks are every 100 ms so this means every second.
#define MICROAPP_LOOP_FREQUENCY 10

#define MICROAPP_LOOP_INTERVAL_MS (TICK_INTERVAL_MS * MICROAPP_LOOP_FREQUENCY)

// The number of 8 interrupt service routines per type should be sufficient.
const uint8_t MAX_PIN_ISR_COUNT = 8;
const uint8_t MAX_BLE_ISR_COUNT = 8;

struct pin_isr_t {
	uint8_t pin;
	uintptr_t callback;
};

struct ble_isr_t {
	uint16_t type;
	uintptr_t callback;
};

struct __attribute__((packed)) microapp_buffered_mesh_message_t {
	stone_id_t stoneId;  // Stone ID of the sender.
	uint8_t messageSize;
	uint8_t message[MICROAPP_MAX_MESH_MESSAGE_SIZE];
};

/**
 * The class MicroappProtocol has functionality to store a second app (and perhaps in the future even more apps) on
 * another part of the flash memory.
 */
class MicroappProtocol : public EventListener {
private:
	/**
	 * Singleton, constructor, also copy constructor, is private.
	 */
	MicroappProtocol();
	MicroappProtocol(MicroappProtocol const&);
	void operator=(MicroappProtocol const&);

	/**
	 * Local flag to check if app did boot. This flag is set to true after the main of the microapp is called
	 * and will be set after that function returns. If there is something wrong with the microapp it can be used
	 * to disable the microapp.
	 *
	 * TODO: Of course, if there is something wrong, _booted will not be reached. The foolproof implementation
	 * sets first the state to a temporarily "POTENTIAL BOOT FAILURE" and if it after a reboots finds the state
	 * at this particular step, it will disable the app rather than try again. The app has to be then enabled
	 * explicitly again.
	 */
	bool _booted;

	/**
	 * Local flag to indicate that ram section has been loaded.
	 */
	bool _loaded;

	/**
	 * Debug mode
	 */
	bool _debug;

	/**
	 * Address to setup() function.
	 */
	uintptr_t _setup;

	/**
	 * Address to loop() function.
	 */
	uintptr_t _loop;

	/**
	 * Addressees of pin interrupt service routines.
	 */
	pin_isr_t _pinIsr[MAX_PIN_ISR_COUNT];

	/**
	 * Addressees of ble interrupt service routines.
	 */
	ble_isr_t _bleIsr[MAX_BLE_ISR_COUNT];

	/**
	 * Coroutine for microapp.
	 */
	coroutine _coroutine;

	/**
	 * Arguments to the coroutine.
	 */
	coargs _coargs;

	/**
	 * A counter used for the coroutine (to e.g. set the number of ticks for "delay" functionality).
	 */
	int _cocounter;

	/**
	 * Implementing count down for a coroutine counter.
	 */
	int _coskip;

	/**
	 * Store data for callbacks towards the microapp.
	 */
	CircularBuffer<uint8_t>* _callbackData;

	/**
	 * Flag to indicate whether to forward scanned devices to microapp
	 */
	bool _microappIsScanning;

	/**
	 * Max number of mesh messages that will be queued.
	 */
	const uint8_t MAX_MESH_MESSAGES_BUFFERED = 3;

	/**
	 * Buffer received mesh messages.
	 *
	 * Starts with microapp_buffered_mesh_message_header_t, followed by the message.
	 */
	CircularBuffer<microapp_buffered_mesh_message_t> _meshMessageBuffer;

protected:
	/**
	 * Call the loop function (internally).
	 */
	void callLoop(int& cntr, int& skip);

	/**
	 * Initialize memory for the microapp.
	 */
	uint16_t initMemory();

	/**
	 * Load ram information, set by microapp.
	 */
	uint16_t interpretRamdata();

	/**
	 * Handle a received mesh message.
	 */
	void onMeshMessage(MeshMsgEvent event);

	/**
	 * Handle a scanned BLE device.
	 */
	void onDeviceScanned(scanned_device_t* dev);

	/**
	 * Handle microapp log commands.
	 */
	cs_ret_code_t handleMicroappLogCommand(uint8_t* payload, uint16_t length);

	/**
	 * Handle microapp delay commands.
	 */
	cs_ret_code_t handleMicroappDelayCommand(microapp_delay_cmd_t* delay_cmd);

	/**
	 * Handle microapp pin commands.
	 */
	cs_ret_code_t handleMicroappPinCommand(microapp_pin_cmd_t* pin_cmd);

	/**
	 * Handle microapp pin switching commands.
	 */
	cs_ret_code_t handleMicroappPinSwitchCommand(microapp_pin_cmd_t* pin_cmd);

	/**
	 * Handle microapp pin commands for setting pin modes.
	 */
	cs_ret_code_t handleMicroappPinSetModeCommand(microapp_pin_cmd_t* pin_cmd);

	/**
	 * Handle microapp pin commands for reading and writing pins.
	 */
	cs_ret_code_t handleMicroappPinActionCommand(microapp_pin_cmd_t* pin_cmd);

	/**
	 * Handle microapp commands for advertising microapp state in service data.
	 */
	cs_ret_code_t handleMicroappServiceDataCommand(uint8_t* payload, uint16_t length);

	/**
	 * Handle microapp TWI commands.
	 */
	cs_ret_code_t handleMicroappTwiCommand(microapp_twi_cmd_t* twi_cmd);

	/**
	 * Handle microapp BLE commands.
	 */
	cs_ret_code_t handleMicroappBleCommand(microapp_ble_cmd_t* ble_cmd);

	/**
	 * Handle microapp commands for power usage requests.
	 */
	cs_ret_code_t handleMicroappPowerUsageCommand(uint8_t* payload, uint16_t length);

	/**
	 * Handle microapp commands for presence requests.
	 */
	cs_ret_code_t handleMicroappPresenceCommand(uint8_t* payload, uint16_t length);

	/**
	 * Handle microapp commands for sending and reading mesh messages.
	 */
	cs_ret_code_t handleMicroappMeshCommand(microapp_mesh_header_t* meshCommand, uint8_t* payload, size_t payloadSize);

public:
	static MicroappProtocol& getInstance() {
		static MicroappProtocol instance;
		return instance;
	}

	/**
	 * Set IPC ram data.
	 */
	void setIpcRam();

	/**
	 * Actually run the app.
	 */
	void callApp(uint8_t appIndex);

	/**
	 * Call setup and loop functions.
	 */
	void callSetupAndLoop(uint8_t appIndex);

	/**
	 * Receive events (for example for i2c)
	 */
	void handleEvent(event_t& event);

	/**
	 * Handle commands from the microapp
	 */
	cs_ret_code_t handleMicroappCommand(uint8_t* payload, uint16_t length);
};
