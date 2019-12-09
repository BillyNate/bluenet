/**
 * Author: Crownstone Team
 * Copyright: Crownstone (https://crownstone.rocks)
 * Date: Sep 24, 2019
 * License: LGPLv3+, Apache License 2.0, and/or MIT (triple-licensed)
 */

#pragma once

#include <behaviour/cs_Behaviour.h>
#include <behaviour/cs_SwitchBehaviour.h>
#include <presence/cs_PresencePredicate.h>
#include <presence/cs_PresenceCondition.h>
#include <time/cs_TimeOfDay.h>

#include <algorithm>
#include <cstdint>
#include <cstddef>

namespace WireFormat {

// data will be copied as few times as possible, but the constructed
// object is not emplaced over the [data] pointer passed as parameter.
template <class T>
T deserialize(uint8_t* data, size_t len);

// a uniform serialization mechanism for both object and fundamental 
// types. For each serializable class simply implement a member method called
// serialize that takes no arguments, and create an in class typedef 
// that is equal to the return type of this method.
template<class T>
typename T::SerializedDataType serialize(const T& obj){
    return obj.serialize();
}

// returns the bytecount of the array to which a type is serialized by WireFormat.
// (specialized to return sizeof(..) for some fundamental types to gain a uniform interface)
template<class T>
constexpr size_t size(T* = nullptr){ return std::tuple_size<typename T::SerializedDataType>::value; }



// ========== Specializations for deserialize =========

template<>
uint8_t WireFormat::deserialize(uint8_t* data, size_t len);

template<> 
uint32_t WireFormat::deserialize(uint8_t* data, size_t len);

template<> 
int32_t WireFormat::deserialize(uint8_t* data, size_t len);

template<> 
uint64_t WireFormat::deserialize(uint8_t* data, size_t len);

template<>
TimeOfDay WireFormat::deserialize(uint8_t* data, size_t len);

template<>
PresencePredicate WireFormat::deserialize(uint8_t* data, size_t len);

template<>
PresenceCondition WireFormat::deserialize(uint8_t* data, size_t len);

template<>
Behaviour WireFormat::deserialize(uint8_t* data, size_t len);

template<>
SwitchBehaviour WireFormat::deserialize(uint8_t* data, size_t len);

// ========== Specializations/overloads for serialize =========

std::array<uint8_t,1> serialize(const uint8_t& obj);
std::array<uint8_t,4> serialize(const uint32_t& obj);
std::array<uint8_t,4> serialize(const int32_t& obj);
std::array<uint8_t,8> serialize(const uint64_t& obj);

template<> constexpr size_t size<uint8_t>(uint8_t*){ return sizeof(uint8_t); }
template<> constexpr size_t size<int32_t>(int32_t*){ return sizeof(int32_t); }
template<> constexpr size_t size<uint32_t>(uint32_t*){ return sizeof(uint32_t); }
template<> constexpr size_t size<uint64_t>(uint64_t*){ return sizeof(uint64_t); }

template<> constexpr size_t size<const uint8_t>(const uint8_t*){ return sizeof(uint8_t); }
template<> constexpr size_t size<const int32_t>(const int32_t*){ return sizeof(int32_t); }
template<> constexpr size_t size<const uint32_t>(const uint32_t*){ return sizeof(uint32_t); }
template<> constexpr size_t size<const uint64_t>(const uint64_t*){ return sizeof(uint64_t); }


} // namespace WireFormat
