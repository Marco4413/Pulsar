#ifndef _PULSAR_BINARY_H
#define _PULSAR_BINARY_H

#include "pulsar/core.h"

namespace Pulsar::Binary
{
    // The Neutron Format
    constexpr uint64_t SIGNATURE_LENGTH = 4;
    constexpr const char* SIGNATURE     = "\0NTR";
    constexpr uint32_t FORMAT_VERSION   = 0;

    constexpr uint8_t CHUNK_END_OF_MODULE   = 0x00;
    constexpr uint8_t CHUNK_FUNCTIONS       = 0x01;
    constexpr uint8_t CHUNK_NATIVE_BINDINGS = 0x02;
    constexpr uint8_t CHUNK_GLOBALS         = 0x03;
    constexpr uint8_t CHUNK_CONSTANTS       = 0x04;
    constexpr uint8_t CHUNK_SOURCE_DEBUG_SYMBOLS = 0x80;

    constexpr bool IsOptionalChunk(uint8_t chunkType) { return chunkType >= 0x80; }

    constexpr uint8_t GLOBAL_FLAG_CONSTANT = 1;

    constexpr uint16_t _N = 0x0001;
    // Are we running on a Little Endian Processor?
    constexpr bool IS_LITTLE_ENDIAN = static_cast<const uint8_t&>(_N) == 0x01;
}

#endif // _PULSAR_BINARY_H
