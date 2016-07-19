/**
 * Author: Dominik Egger
 * Copyright: Distributed Organisms B.V. (DoBots)
 * Date: Oct 29, 2014
 * License: LGPLv3+, Apache, and/or MIT, your choice
 */
#pragma once

//#include <cstdint>
#include <string>
#include <cstdlib>
//

//#include <common/cs_Types.h>

#include <drivers/cs_Serial.h>

#if __clang__
#define STRINGIFY(str) #str
#else
#define STRINGIFY(str) str
#endif

/** @namespace BLEutil
 *
 * Utilities, e.g. for printing over UART.
 */
namespace BLEutil {

/** Convert a short (uint16_t) from LSB (little-endian) to
 * MSB (big-endian) and vice versa
 *
 * @val the value to be converted
 *
 * @return the converted value
 */
inline uint16_t convertEndian16(uint16_t val) {
	return ((val >> 8) & 0xFF) | ((val & 0xFF) << 8);
}

/** Convert an integer (uint32_t) from LSB (little-endian) to
 * MSB (big-endian) and vice versa
 *
 * @val the value to be converted
 *
 * @return the converted value
 */
inline uint32_t convertEndian32(uint32_t val) {
	return ((val >> 24) & 0xFF)
		 | ((val >> 8) & 0xFF00)
		 | ((val & 0xFF00) << 8)
		 | ((val & 0xFF) << 24);
}

/** Macro that returns the length of an array
 *
 * @a the array whose length should be calculated
 *
 * @return the length of the array
 */
#define SIZEOF_ARRAY( a ) (int)(sizeof( a ) / sizeof( a[ 0 ] ))

template<typename T>
void printInlineArray(T* arr, uint16_t len) {
	uint8_t* ptr = (uint8_t*)arr;
	for (int i = 0; i < len; ++i) {
		_log(DEBUG, " %02X", ptr[i]);
		if ((i+1) % 30 == 0) {
			_log(DEBUG, "\r\n");
		}
	}
}

template<typename T>
void printArray(T* arr, uint16_t len) {
//	uint8_t* ptr = (uint8_t*)arr;
//	for (int i = 0; i < len; ++i) {
//		_log(DEBUG, " %02X", ptr[i]);
//		if ((i+1) % 30 == 0) {
//			_log(DEBUG, "\r\n");
//		}
//	}
	printInlineArray(arr, len);
	_log(DEBUG, "\r\n");
}

template<typename T>
std::string toBinaryString(T& value) {
	std::string result("");
	for (int i = sizeof(T) * 8 - 1; i >= 0; --i) {
		result += value & (1 << i) ? "1" : "0";
	}
	return result;
}

inline void print_heap(const std::string & msg) {
	uint8_t *p = (uint8_t*)malloc(1);
	LOGd("%s %p", msg.c_str(), p);
	free(p);
}

inline void print_stack(const std::string & msg) {
	void* sp;
	asm("mov %0, sp" : "=r"(sp) : : );
	LOGd("%s %p", msg.c_str(), (uint8_t*)sp);
}


}
