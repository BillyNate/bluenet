/*
 * Author: Bart van Vliet
 * Copyright: Distributed Organisms B.V. (DoBots)
 * Date: Apr 4, 2016
 * License: LGPLv3+, Apache License, or MIT, your choice
 */

#include "structs/cs_ScheduleEntries.h"
#include <util/cs_BleError.h>

//#define PRINT_DEBUG

uint8_t ScheduleEntry::getTimeType(const schedule_entry_t* entry) {
	uint8_t timeType = entry->type & 0x00FF;
	return timeType;
}

uint8_t ScheduleEntry::getActionType(const schedule_entry_t* entry) {
	uint8_t actionType = (entry->type & 0xFF00) >> 4;
	return actionType;
}

void ScheduleEntry::print(const schedule_entry_t* entry) {

	LOGd("id=%03u type=%02X override=%02X nextTimestamp=%u", entry->id, entry->type, entry->overrideMask, entry->nextTimestamp);

	switch (getTimeType(entry)) {
	case SCHEDULE_TIME_TYPE_REPEAT:
		LOGd("repeatTime=%u", entry->repeat.repeatTime);
		break;
	case SCHEDULE_TIME_TYPE_DAILY:
		LOGd("days=%02X nextDay=%u", entry->daily.dayOfWeek, entry->daily.nextDayOfWeek);
		break;
	}

	switch (getActionType(entry)) {
	case SCHEDULE_ACTION_TYPE_PWM:
		LOGd("pwm=%u", entry->pwm.pwm);
		break;
	case SCHEDULE_ACTION_TYPE_FADE:
		LOGd("pwmEnd=%u fadeTime=%u", entry->fade.pwmEnd, entry->fade.fadeDuration);
		break;
	}
}



uint16_t ScheduleList::getSize() const {
	assert(_buffer != NULL, FMT_BUFFER_NOT_ASSIGNED);
	return _buffer->size;
}

bool ScheduleList::isEmpty() const {
	return getSize() == 0;
}

bool ScheduleList::isFull() const {
	return getSize() >= MAX_SCHEDULE_ENTRIES;
}

void ScheduleList::clear() {
	assert(_buffer != NULL, FMT_BUFFER_NOT_ASSIGNED);
	_buffer->size = 0;
}

bool ScheduleList::add(const schedule_entry_t* entry) {
	bool success = false;
	for (uint16_t i=0; i<getSize(); i++) {
		if (_buffer->list[i].id == entry->id) {
#ifdef PRINT_SE_VERBOSE
			LOGd("update id %u", entry->id);
#endif
			_buffer->list[i] = *entry;
			success = true;
			break;
		}
	}
	if (!success && !isFull()) {
		_buffer->list[_buffer->size++] = *entry;
#ifdef PRINT_SE_VERBOSE
		LOGd("add id %u", entry->id);
#endif
		success = true;
	}
	return success;
}

bool ScheduleList::rem(const schedule_entry_t* entry) {
	bool success = false;
	for (uint16_t i=0; i<getSize(); i++) {
		if (_buffer->list[i].id == entry->id) {
#ifdef PRINT_SE_VERBOSE
			LOGd("rem id %u", entry->id);
#endif
			success = true;
			for (;i<_buffer->size-1; i++) {
				_buffer->list[i] = _buffer->list[i+1];
			}
			_buffer->size--;
			break;
		}
	}
	return success;
}

schedule_entry_t* ScheduleList::checkSchedule(uint32_t currentTime) {
	schedule_entry_t* result = NULL;
	for (uint16_t i=0; i<getSize(); i++) {
		if (_buffer->list[i].nextTimestamp == 0) {
			//TODO: remove this entry
		}
		if (_buffer->list[i].nextTimestamp < currentTime) {
			result = &(_buffer->list[i]);

			switch (ScheduleEntry::getTimeType(result)) {
			case SCHEDULE_TIME_TYPE_REPEAT:
				result->nextTimestamp += result->repeat.repeatTime * 60;
				break;
			case SCHEDULE_TIME_TYPE_DAILY:
				// Daily task, reschedule in 24h
				result->nextTimestamp += SECONDS_PER_DAY;
				// Check day of week
				if (!(result->daily.dayOfWeek & DAILY_REPEAT_ALL_DAYS) && !(result->daily.dayOfWeek & (1 << result->daily.nextDayOfWeek))) {
					result = NULL;
				}
				// Keep up the next day of week
				_buffer->list[i].daily.nextDayOfWeek = (_buffer->list[i].daily.nextDayOfWeek + 1) % 7;
				break;
			case SCHEDULE_TIME_TYPE_ONCE:
				result->nextTimestamp = 0; // Mark for deletion
				break;
			}
		}
	}
	return result;
}

void ScheduleList::sync(uint32_t currentTime) {
	for (uint16_t i=0; i<getSize(); i++) {
		if (_buffer->list[i].nextTimestamp == 0) {
			//TODO: remove this entry
		}
		//! Make sure nextTimestamp > currentTime
		if (_buffer->list[i].nextTimestamp < currentTime) {
			switch (ScheduleEntry::getTimeType(&(_buffer->list[i]))) {
			case SCHEDULE_TIME_TYPE_REPEAT:
				_buffer->list[i].nextTimestamp += ((currentTime - _buffer->list[i].nextTimestamp) / (_buffer->list[i].repeat.repeatTime*60) + 1) * (_buffer->list[i].repeat.repeatTime*60);
				break;
			case SCHEDULE_TIME_TYPE_DAILY:{
				uint32_t daysDiff = (currentTime - _buffer->list[i].nextTimestamp) / SECONDS_PER_DAY + 1;
				_buffer->list[i].nextTimestamp += daysDiff * SECONDS_PER_DAY;

				// Keep up the next day of week
				// TODO: is this correct?
				_buffer->list[i].daily.nextDayOfWeek = (_buffer->list[i].daily.nextDayOfWeek + daysDiff) % 7;
				break;
			}
			case SCHEDULE_TIME_TYPE_ONCE:
				// TODO: remove this entry immediately
				_buffer->list[i].nextTimestamp = 0; // Mark for deletion
				break;
			}
		}
	}
}

void ScheduleList::print() const {
	LOGd("Schedule list size=%u", getSize());
	for (uint16_t i=0; i<getSize(); i++) {
		ScheduleEntry::print(&(_buffer->list[i]));
	}
}
