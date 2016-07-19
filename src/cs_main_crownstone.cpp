/**
 * Author: Anne van Rossum
 * Copyright: Distributed Organisms B.V. (DoBots)
 * Date: 14 Aug., 2014
 * License: LGPLv3+, Apache, or MIT, your choice
 */

/**********************************************************************************************************************
 * The Crownstone is a high-voltage (domestic) switch. It can be used to combine:
 *   - indoor localization
 *   - building automation
 * It is therefore one of the first real internet-of-things devices entering the market.
 * Read more on: https://dobots.nl/products/crownstone/
 *
 * A large part of the software is open-source. You can find it at: https://github.com/dobots/bluenet. The README
 * file at that repository will get you started.
 *
 * Almost all configuration options should be set in CMakeBuild.config.
 *********************************************************************************************************************/

//! temporary defines

#define RESET_COUNTER
//#define MICRO_VIEW 1
#define CHANGE_NAME_ON_RESET
//#define CHANGE_MINOR_ON_RESET

/**********************************************************************************************************************
 * General includes
 *********************************************************************************************************************/

#include <cs_Crownstone.h>

#include "cfg/cs_Boards.h"
#include "drivers/cs_RTC.h"
#include "drivers/cs_PWM.h"
#include "util/cs_Utils.h"
#include "drivers/cs_Timer.h"
#include "structs/buffer/cs_MasterBuffer.h"

#include <drivers/cs_RNG.h>

/**********************************************************************************************************************
 * Custom includes
 *********************************************************************************************************************/

#include <events/cs_EventDispatcher.h>
#include <events/cs_EventTypes.h>

#include <ble/cs_DoBotsManufac.h>

#include <processing/cs_PowerSampling.h>
#include <storage/cs_State.h>

/**********************************************************************************************************************
 * Main functionality
 *********************************************************************************************************************/

using namespace BLEpp;

Crownstone::Crownstone() :
	_switch(NULL), _temperatureGuard(NULL), _powerSampler(NULL),
	_deviceInformationService(NULL), _crownstoneService(NULL), _setupService(NULL),
	_generalService(NULL), _localizationService(NULL), _powerService(NULL),
	_scheduleService(NULL),
	_serviceData(NULL), _beacon(NULL),
	_mesh(NULL), _sensors(NULL), _fridge(NULL),
	_commandHandler(NULL), _scanner(NULL), _tracker(NULL), _scheduler(NULL),
	_advertisementPaused(false), _mainTimer(0), _operationMode(0)
{

	MasterBuffer::getInstance().alloc(MASTER_BUFFER_SIZE);
	EventDispatcher::getInstance().addListener(this);

	//! set up the bluetooth stack that controls the hardware.
	_stack = &Nrf51822BluetoothStack::getInstance();

	// create all the objects that are needed for execution, but make sure they
	// don't execute any softdevice related code. do that in an init function
	// and call it in the setup/configure phase
	_timer = &Timer::getInstance();

	// persistent storage, configuration, state
	_storage = &Storage::getInstance();
	_settings = &Settings::getInstance();
	_stateVars = &State::getInstance();

	//! create command handler
	_commandHandler = &CommandHandler::getInstance();

#if DEVICE_TYPE==DEVICE_CROWNSTONE
	// switch using PWM or Relay
	_switch = &Switch::getInstance();
	//! create temperature guard
	_temperatureGuard = new TemperatureGuard();

	_powerSampler = &PowerSampling::getInstance();
#endif

};

void Crownstone::init() {


	LOGi(FMT_HEADER, "init");

	//! initialize drivers
	initDrivers();

	LOGi(FMT_HEADER, "configure");

	//! configure the crownstone
	configure();

	LOGi(FMT_HEADER, "setup");

	_stateVars->get(STATE_OPERATION_MODE, _operationMode);
//	_operationMode = OPERATION_MODE_NORMAL;
//	_operationMode = OPERATION_MODE_SETUP;

	switch(_operationMode) {
	case OPERATION_MODE_SETUP: {

		LOGd("Configure setup mode");

		//! create services
		createSetupServices();

		//! set it by default into low tx mode
		_stack->setTxPowerLevel(LOW_TX_POWER);

		LOGi("Enable PIN encryption");
		//! use PIN encryption for setup mode
		_stack->setEncrypted(true);

		break;
	}
	case OPERATION_MODE_NORMAL: {

		LOGd("Configure normal operation mode");

		//! setup normal operation mode
		prepareCrownstone();

		//! create services
		createCrownstoneServices();

		break;
	}
	case OPERATION_MODE_DFU: {

		CommandHandler::getInstance().handleCommand(CMD_GOTO_DFU);
		while(true) {}; // loop until reset triggers
	}
	}

	//! loop through all services added to the stack and create the characteristics
	_stack->createCharacteristics();

	LOGi(FMT_HEADER, "init services");

	_stack->initServices();

	// [16.06.16] need to execute app scheduler, otherwise pstorage
	// events will get lost ... maybe need to check why that actually happens??
	app_sched_execute();
}

void Crownstone::configure() {

	LOGi("> stack ...");
	//! configure parameters for the Bluetooth stack
	configureStack();

#ifdef RESET_COUNTER
	uint32_t resetCounter;
	State::getInstance().get(STATE_RESET_COUNTER, resetCounter);
	++resetCounter;
	LOGf("Reset counter at: %d", resetCounter);
	State::getInstance().set(STATE_RESET_COUNTER, resetCounter);
#endif

	//! set advertising parameters such as the device name and appearance.
	//! Note: has to be called after _stack->init or Storage is initialized too early and won't work correctly
	setName();

	LOGi("> advertisement ...");
	//! configure advertising parameters
	configureAdvertisement();

//	BLEutil::print_heap("Heap config: ");
//	BLEutil::print_stack("Stack config: ");
}

/**
 * This must be called after the SoftDevice has started.
 */
void Crownstone::initDrivers() {

	LOGd(FMT_INIT, "stack");

	// things that need to be configured on the stack **BEFORE** init is called
#if LOW_POWER_MODE==0
	_stack->setClockSource(NRF_CLOCK_LFCLKSRC_SYNTH_250_PPM);
#else
	//! TODO: depends on board!
//	_stack->setClockSource(NRF_CLOCK_LFCLKSRC_XTAL_50_PPM);
	_stack->setClockSource(CLOCK_SOURCE);
#endif

	//! start up the softdevice early because we need it's functions to configure devices it ultimately controls.
	//! in particular we need it to set interrupt priorities.
	_stack->init();

	LOGd(FMT_INIT, "timers");
	_timer->init();

	LOGd(FMT_INIT, "pstorage");
	// initialization of storage and settings has to be done **AFTER** stack is initialized
	_storage->init();

	LOGi("Loading configuration");
	_settings->init();
	_stateVars->init();

	LOGd(FMT_INIT, "command handler");
	_commandHandler->init();

#if DEVICE_TYPE==DEVICE_CROWNSTONE
	// switch / PWM init
	LOGd(FMT_INIT, "switch / PWM");
	_switch->init();

	LOGd(FMT_INIT, "temperature guard");
	_temperatureGuard->init();

	_powerSampler->init();
#endif

	// init GPIOs
#if HARDWARE_BOARD==PCA10001
	nrf_gpio_cfg_output(PIN_GPIO_LED_CON);
#endif

#if HAS_LEDS==1
	// Note: DO NOT USE THEM WHILE SCANNING OR MESHING ...
	nrf_gpio_cfg_output(PIN_GPIO_LED_1);
	nrf_gpio_cfg_output(PIN_GPIO_LED_2);
	// setting the pin makes them turn off ....
	nrf_gpio_pin_set(PIN_GPIO_LED_1);
	nrf_gpio_pin_set(PIN_GPIO_LED_2);
#endif
}

/** Sets default parameters of the Bluetooth connection.
 *
 * Data is transmitted with TX_POWER dBm.
 *
 * On transmission of data within a connection (higher interval -> lower power consumption, slow communication)
 *   - minimum connection interval (in steps of 1.25 ms, 16*1.25 = 20 ms)
 *   - maximum connection interval (in steps of 1.25 ms, 32*1.25 = 40 ms)
 * The supervision timeout multiplier is 400
 * The slave latency is 10
 * On advertising:
 *   - advertising interval (in steps of 0.625 ms, 1600*0.625 = 1 sec) (can be between 0x0020 and 0x4000)
 *   - advertising timeout (disabled, can be between 0x0001 and 0x3FFF, and is in steps of seconds)
 *
 * There is no whitelist defined, nor peer addresses.
 */
void Crownstone::configureStack() {

	_stack->setTxPowerLevel(TX_POWER);
	_stack->setMinConnectionInterval(16);
	_stack->setMaxConnectionInterval(32);
	_stack->setConnectionSupervisionTimeout(400);
	_stack->setSlaveLatency(10);
	_stack->setAdvertisingInterval(ADVERTISEMENT_INTERVAL);
	_stack->setAdvertisingTimeoutSeconds(0);

	//! Set the stored tx power
	int8_t txPower;
	_settings->get(CONFIG_TX_POWER, &txPower);
	_stack->setTxPowerLevel(txPower);

	//! Set the stored advertisement interval
	uint16_t advInterval;
	_settings->get(CONFIG_ADV_INTERVAL, &advInterval);
	_stack->setAdvertisingInterval(advInterval);

	//! Retrieve and Set the PASSKEY for pairing
	uint8_t passkey[BLE_GAP_PASSKEY_LEN];
	_settings->get(CONFIG_PASSKEY, passkey);
	_stack->setPasskey(passkey);

	_stack->onConnect([&](uint16_t conn_handle) {
		LOGi("onConnect...");
		//! todo this signature needs to change

		//! first stop, see https://devzone.nordicsemi.com/index.php/about-rssi-of-ble
		//! be neater about it... we do not need to stop, only after a disconnect we do...
#if RSSI_ENABLE==1

		sd_ble_gap_rssi_stop(conn_handle);

#if (SOFTDEVICE_SERIES == 130 && SOFTDEVICE_MAJOR == 1 && SOFTDEVICE_MINOR == 0) || \
	(SOFTDEVICE_SERIES == 110 && SOFTDEVICE_MAJOR == 8)
		sd_ble_gap_rssi_start(conn_handle, 0, 0);
#else
		sd_ble_gap_rssi_start(conn_handle);
#endif
#endif

#if HARDWARE_BOARD==PCA10001
		nrf_gpio_pin_set(PIN_GPIO_LED_CON);
#endif

//		LOGd("clear gpregret on connect ...");
		sd_power_gpregret_clr(0xFF);

	});
	_stack->onDisconnect([&](uint16_t conn_handle) {
		LOGi("onDisconnect...");
		//! of course this is not nice, but dirty! we immediately start advertising automatically after being
		//! disconnected. but for now this will be the default behaviour.

#if HARDWARE_BOARD==PCA10001
		nrf_gpio_pin_clear(PIN_GPIO_LED_CON);
#endif

		// [31.05.16] it seems as if it is not necessary anmore to stop / start scanning when
		//   disconnecting from the device. just calling startAdvertising is enough
		//		bool wasScanning = _stack->isScanning();
		//		_stack->stopScanning();

		_stateVars->disableNotifications();

		_stack->startAdvertising();

		// [23.06.16] need to restart the mesh on disconnect, otherwise we have ~10s delay until the device starts
		// advertising.
		// [29.06.16] this happened on the pca10000, but doesn't seem to happen on the dobeacon v0.7 need to check
		// on other versions. On the contrary, the reset seems to crash the dobeacon v0.7
#if HARDWARE_BOARD==PCA10031
		if (_mesh->isRunning()) {
			_mesh->restart();
		}
#endif

		// [31.05.16] it seems as if it is not necessary anmore to stop / start scanning when
		//   disconnecting from the device. just calling startAdvertising is enough
		//		if (wasScanning) {
		//			_stack->startScanning();
		//		}

	});

}

void Crownstone::configureAdvertisement() {

	//! create the iBeacon parameter object which will be used
	//! to configure advertisement as an iBeacon

	//! get values from config
	uint16_t major, minor;
	uint8_t rssi;
	ble_uuid128_t uuid;

	_settings->get(CONFIG_IBEACON_MAJOR, &major);
	_settings->get(CONFIG_IBEACON_MINOR, &minor);
	_settings->get(CONFIG_IBEACON_UUID, uuid.uuid128);
	_settings->get(CONFIG_IBEACON_TXPOWER, &rssi);

#ifdef CHANGE_MINOR_ON_RESET
	minor++;
	LOGi("Increase minor to %d", minor);
	_settings->set(CONFIG_IBEACON_MINOR, &minor);
#endif

	//! create ibeacon object
	_beacon = new IBeacon(uuid, major, minor, rssi);

	//! create the Service Data  object which will be used
	//! to advertise certain state variables
	_serviceData = new ServiceData();

	// initialize service data

	//! read crownstone id from storage
	uint16_t crownstoneId;
	_settings->get(CONFIG_CROWNSTONE_ID, &crownstoneId);
	LOGi("Set crownstone id to %d", crownstoneId);

	//! and set it to the service data
	_serviceData->updateCrownstoneId(crownstoneId);

	// fill service data with initial data
	uint8_t switchState;
	_stateVars->get(STATE_SWITCH_STATE, switchState);
	_serviceData->updateSwitchState(switchState);

	_serviceData->updateTemperature(getTemperature());

	int32_t powerUsage;
	_stateVars->get(STATE_POWER_USAGE, powerUsage);
	_serviceData->updatePowerUsage(powerUsage);

	//! assign service data to stack
	_stack->setServiceData(_serviceData);

	if (_settings->isSet(CONFIG_IBEACON_ENABLED)) {
		_stack->configureIBeacon(_beacon, DEVICE_TYPE);
	} else {
		_stack->configureBleDevice(DEVICE_TYPE);
	}

}

void Crownstone::createSetupServices() {
	LOGi(STR_CREATE_ALL_SERVICES);

	//! should be available always
	_deviceInformationService = new DeviceInformationService();
	_stack->addService(_deviceInformationService);

	_setupService = new SetupService();
	_stack->addService(_setupService);

}

void Crownstone::createCrownstoneServices() {
	LOGi(STR_CREATE_ALL_SERVICES);

	//! should be available always
	_deviceInformationService = new DeviceInformationService();
	_stack->addService(_deviceInformationService);

#if CROWNSTONE_SERVICE==1
	//! should be available always
	_crownstoneService = new CrownstoneService();
	_stack->addService(_crownstoneService);
#endif

#if GENERAL_SERVICE==1
	//! general services, such as internal temperature, setting names, etc.
	_generalService = new GeneralService;
	_stack->addService(_generalService);
#endif

#if INDOOR_SERVICE==1
	//! now, build up the services and characteristics.
	_localizationService = new IndoorLocalizationService;
	_stack->addService(_localizationService);
#endif

#if POWER_SERVICE==1
#if DEVICE_TYPE==DEVICE_CROWNSTONE
	_powerService = new PowerService;
	_stack->addService(_powerService);
#else
	LOGe("PowerService only available for device type Crownstone!!");
#endif
#endif

#if SCHEDULE_SERVICE==1
	_scheduleService = new ScheduleService;
	_stack->addService(_scheduleService);
#endif

}

/**
 * The default name. This can later be altered by the user if the corresponding service and characteristic is enabled.
 */
void Crownstone::setName() {
	char device_name[32];
	uint16_t size;
	_settings->get(CONFIG_NAME, device_name, size);

#ifdef CHANGE_NAME_ON_RESET
	//! get reset counter
	uint32_t resetCounter;
	State::getInstance().get(STATE_RESET_COUNTER, resetCounter);
//	uint16_t minor;
//	ps_configuration_t cfg = Settings::getInstance().getConfig();
//	Storage::getUint16(cfg.beacon.minor, minor, BEACON_MINOR);
	char devicename_resetCounter[32];
	//! clip name to 5 chars and add reset counter at the end
	sprintf(devicename_resetCounter, "%.*s_%d", MIN(size, 5), device_name, STRINGIFY(resetCounter));
	std::string deviceName = std::string(devicename_resetCounter);
#else
	std::string deviceName(devicename, size);
#endif

	//! assign name
	LOGi(FMT_SET_STR_VAL, "name", deviceName.c_str());
	_stack->updateDeviceName(deviceName); //! max len = ble_gap_devname_max_len (31)
	_stack->updateAppearance(BLE_APPEARANCE_GENERIC_TAG);
}

void Crownstone::prepareCrownstone() {

	LOGi(FMT_CREATE, "Timer");
	_timer->createSingleShot(_mainTimer, (app_timer_timeout_handler_t)Crownstone::staticTick);

	//! create scanner object
	_scanner = &Scanner::getInstance();
	_scanner->setStack(_stack);

	//! create scheduler
	_scheduler = &Scheduler::getInstance();

#if (HARDWARE_BOARD==CROWNSTONE_SENSOR || HARDWARE_BOARD==NORDIC_BEACON)
	_sensors = new Sensors();
#endif

#if DEVICE_TYPE==DEVICE_FRIDGE
	_fridge = new Fridge;
#endif

//	if (_settings->isEnabled(CONFIG_MESH_ENABLED)) {

#if HARDWARE_BOARD == VIRTUALMEMO
			nrf_gpio_range_cfg_output(7,14);
#endif

		_mesh = &Mesh::getInstance();
		_mesh->init();

//	}

//	BLEutil::print_heap("Heap setup: ");
//	BLEutil::print_stack("Stack setup: ");

}

void Crownstone::startUp() {

	LOGi(FMT_HEADER, "startup");

	uint32_t gpregret;
	sd_power_gpregret_get(&gpregret);
	LOGi("Soft reset count: %d", gpregret);

	uint16_t bootDelay;
	_settings->get(CONFIG_BOOT_DELAY, &bootDelay);
	if (bootDelay) {
		LOGi("Boot delay: %d ms", bootDelay);
		nrf_delay_ms(bootDelay);
	}

	//! start advertising
	_stack->startAdvertising();
	//! have to give the stack a moment of pause to start advertising, otherwise we get into race conditions
	nrf_delay_ms(50);

	//! the rest we only execute if we are in normal operation
	//! during setup mode, most of the crownstone's functionality is
	//! disabled
	if (_operationMode == OPERATION_MODE_NORMAL) {

//#if (DEFAULT_ON==1)
//		LOGi("Set power ON by default");
//		_switch->turnOn();
//#elif (DEFAULT_ON==0)
//		LOGi("Set power OFF by default");
//		_switch->turnOff();
//#endif

		//! start main tick
		scheduleNextTick();
		_stack->startTicking();

#if DEVICE_TYPE==DEVICE_CROWNSTONE
		// restore the last value. the switch reads the last state from the storage, but does
		// not automatically update the pwm/relay values. so we read out the last value
		// and set it again to update the pwm
		uint8_t pwm = _switch->getPwm();
		_switch->setPwm(pwm);

		//! start ticking of peripherals
		_temperatureGuard->startTicking();

		LOGd(FMT_START, "power sampling");
		_powerSampler->startSampling();
#endif

		_scheduler->start();

		if (_settings->isSet(CONFIG_SCANNER_ENABLED)) {
			RNG rng;
			uint16_t delay = rng.getRandom16() / 6; // Delay in ms (about 0-10 seconds)
			_scanner->delayedStart(delay);
		}

		if (_settings->isSet(CONFIG_TRACKER_ENABLED)) {
			_tracker->startTracking();
		}

		if (_settings->isSet(CONFIG_MESH_ENABLED)) {
			_mesh->start();
		}

#if DEVICE_TYPE==DEVICE_FRIDGE
		_fridge->startTicking();
#endif

#if (HARDWARE_BOARD==CROWNSTONE_SENSOR || HARDWARE_BOARD==NORDIC_BEACON)
		_sensors->startTicking();
#endif

//		BLEutil::print_heap("Heap startup: ");
//		BLEutil::print_stack("Stack startup: ");

	}

}

void Crownstone::tick() {

	//! update advertisement
	_stack->updateAdvertisement();

	//! update temperature
	int32_t temperature = getTemperature();
	_stateVars->set(STATE_TEMPERATURE, temperature);

	scheduleNextTick();
}

void Crownstone::scheduleNextTick() {
	Timer::getInstance().start(_mainTimer, HZ_TO_TICKS(CROWNSTONE_UPDATE_FREQUENCY), this);
}

void Crownstone::run() {

	LOGi(FMT_HEADER, "running");

	//! forever, run scheduler, wait for events and handle them
	while(1) {

		app_sched_execute();

#if(NORDIC_SDK_VERSION > 5)
		BLE_CALL(sd_app_evt_wait, ());
#else
		BLE_CALL(sd_app_event_wait, () );
#endif

	}
}

void Crownstone::handleEvent(uint16_t evt, void* p_data, uint16_t length) {

//	LOGi("handleEvent: %d", evt);

	bool reconfigureBeacon = false;
	switch(evt) {

	case CONFIG_NAME: {
		_stack->updateDeviceName(std::string((char*)p_data, length));
		// need to reconfigure scan response package for updated name
		_stack->configureScanResponse(DEVICE_TYPE);
		_stack->updateAdvertisement();
		break;
	}
	case CONFIG_IBEACON_MAJOR: {
		_beacon->setMajor(*(uint32_t*)p_data);
		reconfigureBeacon = true;
		break;
	}
	case CONFIG_IBEACON_MINOR: {
		_beacon->setMinor(*(uint32_t*)p_data);
		reconfigureBeacon = true;
		break;
	}
	case CONFIG_IBEACON_UUID: {
		_beacon->setUUID(*(ble_uuid128_t*)p_data);
		reconfigureBeacon = true;
		break;
	}
	case CONFIG_IBEACON_TXPOWER: {
		_beacon->setTxPower(*(int8_t*)p_data);
		reconfigureBeacon = true;
		break;
	}
	case CONFIG_TX_POWER: {
//		LOGd("setTxPowerLevel %d", *(int8_t*)p_data);
		_stack->setTxPowerLevel(*(int8_t*)p_data);

		break;
	}
	case CONFIG_ADV_INTERVAL: {
		_stack->setAdvertisingInterval(*(uint32_t*)p_data);
		_stack->configureAdvertisementParameters();

		if (_stack->isAdvertising()) {
			_stack->stopAdvertising();
			_stack->startAdvertising();
		}
//		restartAdvertising = true;
		break;
	}
	case CONFIG_PASSKEY: {
		_stack->setPasskey((uint8_t*)p_data);
		break;
	}

	case EVT_ADVERTISEMENT_PAUSE: {
		if (_stack->isAdvertising()) {
			_advertisementPaused = true;
			_stack->stopAdvertising();
		}
		break;
	}
	case EVT_ADVERTISEMENT_RESUME: {
		if (_advertisementPaused) {
			_advertisementPaused = false;
			_stack->startAdvertising();
		}
		break;
	}
	case CONFIG_IBEACON_ENABLED: {
		bool enabled = *(bool*)p_data;
		if (enabled) {
			_stack->configureIBeaconAdvData(_beacon);
		} else {
			_stack->configureBleDeviceAdvData();
		}
		_stack->updateAdvertisement();
		break;
	}
	case EVT_ADVERTISEMENT_UPDATED: {
		_stack->updateAdvertisement();
		break;
	}
	case EVT_BROWNOUT_IMPENDING: {
		// turn everything off that consumes power
		LOGf("brownout impending!! force shutdown ...")

		rbc_mesh_stop();
    	_scanner->stop();

#if DEVICE_TYPE==DEVICE_CROWNSTONE
    	_switch->pwmOff();
    	_switch->relayOff();
    	_powerSampler->stopSampling();
#endif

    	// now reset with brownout reset mask set.
    	// NOTE: do not clear the gpregret register, this way
    	//   we can count the number of brownouts in the bootloader
    	sd_power_gpregret_set(GPREGRET_BROWNOUT_RESET);
    	// soft reset, because brownout can't be distinguished from
    	// hard reset otherwise
    	sd_nvic_SystemReset();
		break;
	}
	default: return;
	}

	if (reconfigureBeacon && _settings->isSet(CONFIG_IBEACON_ENABLED)) {
		_stack->updateAdvertisement();
	}
}

void on_exit(void) {
	LOGf("PROGRAM TERMINATED");
}

/**
 * If UART is enabled this will be the message printed out over a serial connection. Connectors are expensive, so UART
 * is not available in the final product.
 */
void welcome() {
	config_uart();

	_log(INFO, CRLN);
//	BLEutil::print_heap("Heap init");
//	BLEutil::print_stack("Stack init");
	//! To have DFU, keep application limited to (BOOTLOADER_REGION_START - APPLICATION_START_CODE - DFU_APP_DATA_RESERVED)
	//! For (0x38000 - 0x1C000 - 0x400) this is 0x1BC00 (113664 bytes)
	LOGi("Welcome Crownstone");
	LOGi("Compilation date: %s", STRINGIFY(COMPILATION_TIME));
	LOGi("Compilation time: %s", __TIME__);
#ifdef GIT_HASH
	LOGi("Git hash: %s", GIT_HASH);
#else
	LOGi("Firmware version: %s", FIRMWARE_VERSION);
#endif
}

/**********************************************************************************************************************/

/**********************************************************************************************************************
 * The main function. Note that this is not the first function called! For starters, if there is a bootloader present,
 * the code within the bootloader has been processed before. But also after the bootloader, the code in
 * cs_sysNrf51.c will set event handlers and other stuff (such as coping with product anomalies, PAN), before calling
 * main. If you enable a new peripheral device, make sure you enable the corresponding event handler there as well.
 *********************************************************************************************************************/

int main() {

	atexit(on_exit);

	//! int uart, be nice and say hello
	welcome();

	Crownstone crownstone;

	// initialize crownstone (depends on the operation mode) ...
	crownstone.init();

	//! start up phase, start ticking (depends on the operation mode) ...
	crownstone.startUp();

	//! run forever ...
	crownstone.run();

}
