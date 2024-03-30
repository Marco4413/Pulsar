#include "pulsar/binary/reader.h"

// https://en.wikipedia.org/wiki/LEB128
bool Pulsar::Binary::IReader::ReadSLEB(int64_t& value)
{
    constexpr int64_t SIZE = sizeof(int64_t) * 8;

    value = 0;
    int64_t shift = 0;
    while (true) {
        uint8_t byte;
        if (!ReadU8(byte))
            return false;
        value |= (int64_t)(byte & 0x7F) << shift;
        shift += 7;
        if ((byte & 0x80) == 0) {
            if (shift < SIZE && (byte & 0x40) != 0)
                value |= (~(int64_t)0) << shift;
            return true;
        }
    }
}

bool Pulsar::Binary::IReader::ReadULEB(uint64_t& value)
{
    value = 0;
    uint64_t shift = 0;
    while (true) {
        uint8_t byte;
        if (!ReadU8(byte))
            return false;
        value |= (uint64_t)(byte & 0x7F) << shift;
        if ((byte & 0x80) == 0)
            return true;
        shift += 7;
    }
}

bool Pulsar::Binary::IReader::ReadF64(double& dbl)
{
    static_assert(sizeof(double) == 8);
    void* dblPtr = (void*)&dbl;
    // TODO: We're assuming doubles are stored with the same endianness as Integers
    // If we encounter any issue we should change this
    uint64_t& value = *((uint64_t*)dblPtr);
    if (!ReadData(8, (uint8_t*)&value))
        return false;
    if constexpr (!IS_LITTLE_ENDIAN) {
        value = (
              ((value & 0x00000000000000FF) << 56)
            | ((value & 0x000000000000FF00) << 40)
            | ((value & 0x0000000000FF0000) << 24)
            | ((value & 0x00000000FF000000) << 8)
            | ((value & 0x000000FF00000000) >> 8)
            | ((value & 0x0000FF0000000000) >> 24)
            | ((value & 0x00FF000000000000) >> 40)
            | ((value & 0xFF00000000000000) >> 56)
        );
    }
    return true;
}

bool Pulsar::Binary::IReader::ReadI64(int64_t& value)
{
    return ReadSLEB(value);
}

bool Pulsar::Binary::IReader::ReadU64(uint64_t& value)
{
    return ReadULEB(value);
}

bool Pulsar::Binary::IReader::ReadU32(uint32_t& value)
{
    if (!ReadData(4, (uint8_t*)&value))
        return false;
    if constexpr (!IS_LITTLE_ENDIAN) {
        value = (
              ((value & 0x000000FF) << 24)
            | ((value & 0x0000FF00) << 8)
            | ((value & 0x00FF0000) >> 8)
            | ((value & 0xFF000000) >> 24)
        );
    }
    return true;
}

bool Pulsar::Binary::IReader::ReadU16(uint16_t& value)
{
    if (!ReadData(2, (uint8_t*)&value))
        return false;
    if constexpr (!IS_LITTLE_ENDIAN) {
        value = (
              ((value & 0x00FF) << 8)
            | ((value & 0xFF00) >> 8)
        );
    }
    return true;
}

bool Pulsar::Binary::IReader::ReadU8(uint8_t& value)
{
    return ReadData(1, &value);
}
