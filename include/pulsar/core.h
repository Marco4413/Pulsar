#ifndef _PULSAR_CORE_H
#define _PULSAR_CORE_H

// All std includes are in this file.
#include <cinttypes>
#include <cstring>
#include <functional>
#include <memory>
#include <unordered_map>
#include <vector>

#ifndef PULSAR_REALLOC
#define PULSAR_REALLOC std::realloc
#endif // PULSAR_REALLOC

#ifndef PULSAR_FREE
#define PULSAR_FREE std::free
#endif // PULSAR_FREE

#ifndef PULSAR_MEMSET
#define PULSAR_MEMSET std::memset
#endif // PULSAR_MEMSET

#ifndef PULSAR_MEMCPY
#define PULSAR_MEMCPY std::memcpy
#endif // PULSAR_MEMCPY

#endif // _PULSAR_CORE_H
