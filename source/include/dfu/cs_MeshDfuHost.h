/*
 * Author: Crownstone Team
 * Copyright: Crownstone (https://crownstone.rocks)
 * Date: Dec 21, 2021
 * License: LGPLv3+, Apache License 2.0, and/or MIT (triple-licensed)
 */


#pragma once

#include <ble/cs_BleCentral.h>
#include <ble/cs_CrownstoneCentral.h>
#include <events/cs_EventListener.h>

/**
 * This bluenet component manages the progress and protocol of a crownstone-to-crownstone firmware update.
 * I.e. application layer dfu implementation.
 *
 * Usage:
 * To start an update, send a A CMD_MESH_DFU event with parameers:
 *  - target device mac address
 *  - operation (start, cancel, info).
 * Response:
 *  - busy     already updating another stone
 *  - fail     starting did not succeed somehow, reverts state to idle.
 *  - success  process will be started, subsequent calls will return busy until process terminates.
 *
 *  NOTE: corresponds roughly to dfu_Write_application.py
 */
class MeshDfuHost : public EventListener {
public:
	/**
	 * return true if dfu process sucessfully started.
	 *
	 * Initializes this component if necessary.
	 */
	bool copyFirmwareTo(device_address_t target);

	enum class Phase {
		Idle = 0,
		TargetTriggerDfuMode, // sending dfu command and waiting for reset
		TargetPreparing,      // send PRN command
		TargetInitializing,   // sending init packets
		TargetUpdating,       // sending firmware packets
		TargetVerifying,
		Aborting,            // might leave the target in an ugly state but at least saves our ass.
		None = 0xFF
	};

private:
	// ----------------------- runtime phase variables -----------------------
	/**
	 * state describing what the device is currently doing/waiting for
	 */
	Phase _phaseCurrent = Phase::None;
	/**
	 * upon phase complete, this phase will be checked for a subsequent action.
	 * (this can be user overridden to stop a dfu flood.)
	 */
	Phase _phaseNextOverride = Phase::None;

	/**
	 * Administration variables for the method waiteForEvent(..);
	 *
	 * _expectedEvent is only valid if _expectedEventCallback is set.
	 * _expectedEventCallback is a pointer to member function taking an event_t&.
	 */
	CS_TYPE _expectedEvent;
	typedef void(MeshDfuHost::*PhaseCallback)(event_t&);
	PhaseCallback _expectedEventCallback = nullptr;

	// ----------------------- ble related variables -----------------------

	/**
	 * cached ble component pointers
	 */
	CrownstoneCentral* _crownstoneCentral = nullptr;
	BleCentral* _bleCentral = nullptr;

	/**
	 * current status of the connection.
	 */
	bool _isCrownstoneCentralConnected = false;
	bool _isBleCentralConnected = false;

	/**
	 * determines how many times a reconnect will be attempted until dfu is stopped.
	 * See MeshDfuConstants::DfuHostSettings::MaxReconnectionAttempts
	 */
	uint8_t _reconnectionAttemptsLeft = 0;

	/**
	 * the device to be dfu-d.
	 */
	device_address_t _targetDevice;

	// -------------------------------------------------------------------------------------
	// ---------------------------------- phase callbacks ----------------------------------
	// -------------------------------------------------------------------------------------

	/**
	 * Connect through CrownstoneBle and wait for result.
	 *
	 * Resets _reconnectionAttemptsLeft.
	 * Continue with sendDfuCommand.
	 */
	void triggerTargetDfuMode();

	/**
	 * Possibly retry connection if not _isCrownstoneCentralConnected yet.
	 * When connection is established, send dfu command and wait for disconnect.
	 * If max retries is reached, stopDfu().
	 *
	 * Resets _reconnectionAttemptsLeft.
	 * Continue with reconnectAfterDfuCommand.
	 */
	void sendDfuCommand(event_t& event);

	/**
	 * Connect through the BleCentral and wait for result.
	 * If connection was still active, wait for next connect result.
	 *
	 * Continue with verifyDfuMode on success.
	 */
	void reconnectAfterDfuCommand(event_t& event);

	/**
	 * checks if the required characteristics are available and obtains handles
	 * for them.
	 */
	void verifyDfuMode(event_t& event);

	void onDisconnect(event_t& event);


	// ------------------------------------------------------------------------------------
	// ------------------------------- phase administration -------------------------------
	// ------------------------------------------------------------------------------------

	/**
	 * Calls startPhaseX where X is determined by the `phase` variable.
	 *
	 * Returns the return value of the called method.
	 * If returnvalue is true:
	 *  - _phaseCurrent is set to `phase`
	 *  - _phaseNext is set to None
	 *  - the start method was successful and has called waitForEvent.
	 * Else:
	 *  - nothing changed
	 */
	bool startPhase(Phase phase);

	/**
	 * Calls the startPhaseX function for the _phaseCurrent.
	 * (Used by startPhase to get into async 'processing mode'.)
	 */
	void startPhase(event_t& event);

	/**
	 * sets a callback to be called when the given event type is received.
	 * The event is passed into the callback for further inspection.
	 *
	 * Returns true if the callback was succesfully set.
	 * Returns false if another event is already awaited.
	 */
	bool waitForEvent(CS_TYPE evttype, PhaseCallback callback);

	/**
	 * Call this method in the last phase event handler. It will:
	 *  - call completePhase(); and check intended next phase
	 *  - check the user set _phaseNext,
	 *  - call startPhase(nextPhase) depending on both these phase values.
	 *
	 * if phase status is reported by return value of completePhaseX methods.
	 * Return Phase::Aborting if unsuccesful to clean up as much as possible.
	 *
	 * completePhaseX methods must be synchronous.
	 */
	void completePhase();

	// ---------- phase start callbacks ----------
	/**
	 * startPhaseX methods start administration for a phase.
	 *
	 * If this method returns true:
	 *  - Any required administration variables have been set up.
	 *  - Phase was successfully scheduled to start using waitForEvent(...).
	 * If this method returns false:
	 *  - No changes happened.
	 */
	bool startPhaseHostInitializing();
	bool startPhaseTargetTriggerDfuMode();
	bool startPhaseTargetPreparing();
	bool startPhaseTargetInitializing();
	bool startPhaseTargetUpdating();
	bool startPhaseTargetVerifying();
	bool startPhaseAborting();
	bool startPhaseBooting();

	// ---------- phase complete callbacks ----------
	/**
	 * completePhaseX methods finish administration for a phase and return
	 * what is the next intended phase. They must be synchronous.
	 *
	 * Called by completePhase.
	 */

	Phase completePhaseHostInitializing();
	Phase completePhaseTargetTriggerDfuMode();
	Phase completePhaseTargetPreparing();
	Phase completePhaseTargetInitializing();
	Phase completePhaseTargetUpdating();
	Phase completePhaseTargetVerifying();
	Phase completePhaseAborting();
	Phase completePhaseBooting();


//	// -----------------------------------------------------------------------------------
//	// --------------------------------- event callbacks ---------------------------------
//	// -----------------------------------------------------------------------------------
//
//	void onTimeout();
//	void onConnect(cs_ret_code_t retCode);
//	void onDisconnect();
//	void onDiscovery(ble_central_discovery_t& result);
//	void onDiscoveryDone(cs_ret_code_t retCode);
//	void onRead(ble_central_read_result_t& result);
//	void onReadDuringConnect(ble_central_read_result_t& result);
//	void onWrite(cs_ret_code_t result);
//	void onNotification(ble_central_notification_t& result);


	// -------------------------------------------------------------------------------------
	// --------------------------------------- utils ---------------------------------------
	// -------------------------------------------------------------------------------------

	/**
	 * ble component pointers non-nullptr?
	 */
	bool isInitialized();

	/**
	 * returns true if this device has an init packet and no already running dfu process.
	 */
	bool ableToLaunchDfu();

	/**
	 * Is the dfu packet written to ICP flash page?
	 */
	bool haveInitPacket();

	/**
	 * No current dfu operations running or planned?
	 */
	bool dfuProcessIdle();

	void connectToTarget();
	void stopDfu();
	bool startDfu(device_address_t macaddr);

public:
	/**
	 * Checks _expectedEvent and calls the callback if it matches and is non-null.
	 */
	virtual void handleEvent(event_t & event) override;
};
