/*
 * Author: Crownstone Team
 * Copyright: Crownstone (https://crownstone.rocks)
 * Date: Aug 15, 2022
 * License: LGPLv3+, Apache License 2.0, and/or MIT (triple-licensed)
 */

#include <drivers/cs_RTC.h>
#include <chrono>

uint32_t nsToTicks(uint64_t ns) {
	return static_cast<uint32_t>((ns * RTC_CLOCK_FREQ) / 1000000000) % MAX_RTC_COUNTER_VAL;
}

uint32_t RTC::getCount() {
	static auto starttime = std::chrono::high_resolution_clock::now();

	auto now = std::chrono::high_resolution_clock::now();
	auto diff = now - starttime;
	auto rtcDuration = std::chrono::duration_cast<
			std::chrono::duration<long int, std::ratio<1, RTC_CLOCK_FREQ>>
		>(diff);

	return rtcDuration.count() % MAX_RTC_COUNTER_VAL;
}

uint32_t RTC::ticksToMs(uint32_t ticks) {
	return static_cast<uint32_t>((static_cast<uint64_t>(ticks) * 1000) / RTC_CLOCK_FREQ);
}

uint32_t RTC::msToTicks(uint32_t ms) {
	return nsToTicks(1000000*ms);
}
