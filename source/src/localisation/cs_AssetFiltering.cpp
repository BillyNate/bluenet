/*
 * Author: Crownstone Team
 * Copyright: Crownstone (https://crownstone.rocks)
 * Date: May 7, 2021
 * License: LGPLv3+, Apache License 2.0, and/or MIT (triple-licensed)
 */

#include <localisation/cs_AssetFiltering.h>
#include <util/cs_Rssi.h>
#include <util/cs_Utils.h>

#define LogLevelAssetFilteringDebug SERIAL_VERY_VERBOSE
#define LogAssetFilteringDebug LOGd
#define LogAssetFilteringWarn LOGw

cs_ret_code_t AssetFiltering::init() {
	_filterStore = new AssetFilterStore();
	if (_filterStore == nullptr) {
		return ERR_NO_SPACE;
	}
	cs_ret_code_t retCode = _filterStore->init();
	if (retCode != ERR_SUCCESS) {
		return retCode;
	}

	_filterSyncer = new AssetFilterSyncer();
	if (_filterSyncer == nullptr) {
		return ERR_NO_SPACE;
	}
	retCode = _filterSyncer->init(*_filterStore);
	if (retCode != ERR_SUCCESS) {
		return retCode;
	}

	listen();
	return ERR_SUCCESS;
}

void AssetFiltering::setAssetHandlerMac(AssetHandlerMac* assetHandlerMac) {
	_assetHandlerMac = assetHandlerMac;
}

void AssetFiltering::setAssetHandlerShortId(AssetHandlerShortId* assetHandlerShortId) {
	_assetHandlerShortId = assetHandlerShortId;
}

// ---------------------------- Handling events ----------------------------

void AssetFiltering::handleEvent(event_t& evt) {
	switch (evt.type) {
		case CS_TYPE::EVT_DEVICE_SCANNED: {
			auto scannedDevice = CS_TYPE_CAST(EVT_DEVICE_SCANNED, evt.data);
			handleScannedDevice(*scannedDevice);
			break;
		}
		default:
			break;
	}
}

void AssetFiltering::handleScannedDevice(const scanned_device_t& device) {
	if (_filterStore->isInProgress()) {
		return;
	}

	for (size_t i = 0; i < _filterStore->getFilterCount(); ++i) {
		auto filter = AssetFilter(_filterStore->getFilter(i));
		if (filterInputResult(filter, device)) {
			processAcceptedAsset(filter,device);
		}
	}
}

void AssetFiltering::processAcceptedAsset(AssetFilter filter, const scanned_device_t& asset) {
	switch (*filter.filterdata().metadata().outputType().outFormat()) {
		case AssetFilterOutputFormat::Mac: {
			if( _assetHandlerMac != nullptr) {
				_assetHandlerMac->handleAcceptedAsset(filter, asset);
			}

			dispatchAcceptedAssetMacToMesh(filter, asset);

			break;
		}

		case AssetFilterOutputFormat::ShortAssetId: {
			if (_assetHandlerShortId != nullptr) {
				short_asset_id_t shortId = filterOutputResultShortAssetId(filter, asset);
				_assetHandlerShortId->handleAcceptedAsset(filter, asset, shortId);
			}
			break;
		}
	}
}

void AssetFiltering::dispatchAcceptedAssetMacToMesh(AssetFilter filter, const scanned_device_t& asset) {
	cs_mesh_model_msg_asset_rssi_mac_t asset_msg;
	asset_msg.rssiData = compressRssi(asset.rssi, asset.channel);
	memcpy(asset_msg.mac, asset.address, sizeof(asset_msg.mac));

	cs_mesh_msg_t msgWrapper;
	msgWrapper.type =  CS_MESH_MODEL_TYPE_ASSET_RSSI_MAC;
	msgWrapper.payload = reinterpret_cast<uint8_t*>(&asset_msg);
	msgWrapper.size = sizeof(asset_msg);
	msgWrapper.reliability = CS_MESH_RELIABILITY_LOW;
	msgWrapper.urgency = CS_MESH_URGENCY_LOW;

	event_t meshMsgEvt(CS_TYPE::CMD_SEND_MESH_MSG, &msgWrapper, sizeof(msgWrapper));

	meshMsgEvt.dispatch();
}

// ---------------------------- Extracting data from the filter  ----------------------------

/**
 * Extracted logic to parse the input type into a template that accepts a delegate expression
 * `ExpressionType returnExpression` in order to re-use the stuff.
 *
 * This method extracts the filters 'input description', prepares the input according to that
 * description and calls the delegate with the prepared data.
 *
 * delegateExpression should be of the form (CuckooFilter, uint8_t*, size_t) -> ReturnType.
 */
template <class ReturnType, class ExpressionType>
ReturnType prepareFilterInputAndCallDelegate(
		AssetFilter filter,
		const scanned_device_t& device,
		AssetFilterInput filterInputDescription,
		ExpressionType delegateExpression,
		ReturnType defaultValue) {

	if (*filter.filterdata().metadata().filterType() != AssetFilterType::CuckooFilter) {
		LogAssetFilteringWarn("Filtertype not implemented");
		return defaultValue;
	}

	CuckooFilter cuckoo = filter.filterdata().filterdata();

	switch (*filterInputDescription.type()) {
		case AssetFilterInputType::MacAddress: {
			return delegateExpression(cuckoo, device.address, sizeof(device.address));
		}
		case AssetFilterInputType::AdDataType: {
			// selects the first found field of configured type and checks if that field's
			// data is contained in the filter. returns defaultValue if it can't be found.
			cs_data_t result                  = {};
			ad_data_type_selector_t* selector = filterInputDescription.AdTypeField();

			assert(selector != nullptr, "Filter metadata type check failed");

			if (BLEutil::findAdvType(selector->adDataType, device.data, device.dataSize, &result) == ERR_SUCCESS) {
				return delegateExpression(cuckoo, result.data, result.len);
			}

			return defaultValue;
		}
		case AssetFilterInputType::MaskedAdDataType: {
			// selects the first found field of configured type and checks if that field's
			// data is contained in the filter. returns false if it can't be found.
			cs_data_t result                         = {};
			masked_ad_data_type_selector_t* selector = filterInputDescription.AdTypeMasked();

			assert(selector != nullptr, "Filter metadata type check failed");

			if (BLEutil::findAdvType(selector->adDataType, device.data, device.dataSize, &result) == ERR_SUCCESS) {
				uint8_t buff[31] = {0};
				assert(result.len <= 31, "advertisement length too big");

				// apply the mask
				for (size_t i = 0; i < result.len; i++) {
					if (BLEutil::isBitSet(selector->adDataMask, i)) {
						buff[i] = result.data[i];
					}
					else {
						buff[i] = 0x00;
					}
				}

				return delegateExpression(cuckoo, buff, result.len);
			}

			return defaultValue;
		}
	}

	return defaultValue;
}

bool AssetFiltering::filterInputResult(AssetFilter filter, const scanned_device_t& asset) {
	// The input result is nothing more than a call to .contains with the correctly prepared input.
	// It is 'correctly preparing the input' that is fumbly.
	return prepareFilterInputAndCallDelegate(
			filter,
			asset,
			filter.filterdata().metadata().inputType(),
			[](CuckooFilter cuckoo, const uint8_t* data, size_t len) { return cuckoo.contains(data, len); },
			false);
}

short_asset_id_t AssetFiltering::filterOutputResultShortAssetId(AssetFilter filter, const scanned_device_t& asset) {
	// The ouput result is nothing more than a call to .contains with the correctly prepared input.
	// It is 'correctly preparing the input' that is fumbly. (At least, if you don't want to always
	// preallocate the buffer that the MaskedAdData needs.)
	return prepareFilterInputAndCallDelegate(
			filter,
			asset,
			filter.filterdata().metadata().outputType().inFormat(),
			[](CuckooFilter cuckoo, const uint8_t* data, size_t len) {
				assert(sizeof(short_asset_id_t) == sizeof(cuckoo_compressed_fingerprint_t),
					   "can't cast compressed fingerprint to asset id");
				cuckoo_compressed_fingerprint_t ccf = cuckoo.getCompressedFingerprint(data, len);
				return *reinterpret_cast<short_asset_id_t*>(&ccf);
			},
			short_asset_id_t{});
}


// ======================== Utils ========================

void logServiceData(scanned_device_t* scannedDevice) {
	uint32_t errCode;
	cs_data_t serviceData;
	errCode = BLEutil::findAdvType(
			BLE_GAP_AD_TYPE_SERVICE_DATA, scannedDevice->data, scannedDevice->dataSize, &serviceData);

	if (errCode != ERR_SUCCESS) {
		return;
	}

	_log(LogLevelAssetFilteringDebug, false, "servicedata: ");
	_logArray(LogLevelAssetFilteringDebug, true, serviceData.data, serviceData.len);
}
