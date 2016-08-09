/*
 * Author: Dominik Egger
 * Copyright: Distributed Organisms B.V. (DoBots)
 * Date: Apr 2, 2015
 * License: LGPLv3+
 */
#pragma once

#include <cstdlib>

#include <common/cs_Types.h>
#include "drivers/cs_Serial.h"

#include <cstring>

/** EncryptionBuffer is a byte array with header.
 *
 * The EncryptionBuffer is used to put in all kind of data. This data is unorganized. The EncryptionBuffer can also be
 * accessed through more dedicated structures. This allows to read/write from the buffer directly ScanResults or
 * other types of sophisticated objects.
 *
 * The disadvantage is that the data will be overwritten by the different accessors. The advantage is that the data
 * fits actually in the device RAM.
 */
class EncryptionBuffer {

private:
	buffer_ptr_t _buffer;
	uint16_t _size;

	bool _locked;

	EncryptionBuffer() :_buffer(NULL), _size(0), _locked(false) {};
	~EncryptionBuffer() {
		if (_buffer) {
			free(_buffer);
		}
	};

public:
	static EncryptionBuffer& getInstance() {
		static EncryptionBuffer instance;
		return instance;
	}

	void alloc(uint16_t size) {
		LOGd("alloc %d", size);
		_size = size;
		_buffer = (buffer_ptr_t)calloc(_size, sizeof(uint8_t));
		LOGd("buffer: %p", _buffer);
	}

	void clear() {
//		LOGd("clear");
		if (_buffer) {
			memset(_buffer, 0, _size);
		}
	}

	bool getBuffer(buffer_ptr_t& buffer, uint16_t& maxLength) {
//		LOGd("getBuffer");
		if (_buffer) {
			buffer = _buffer;
			maxLength = _size;
			return true;
		} else {
			return false;
		}
	}

//	buffer_ptr_t getBuffer() {
//		return _buffer;
//	}

	uint16_t size() { return _size; }

};
