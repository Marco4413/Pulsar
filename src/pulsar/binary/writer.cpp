#include "pulsar/binary/writer.h"

// https://en.wikipedia.org/wiki/LEB128
bool Pulsar::Binary::IWriter::WriteSLEB(int64_t value)
{
    while (true) {
        uint8_t byte = (uint8_t)(value & 0x7F);
        value >>= 7;
        if ((value == 0 && (byte & 0x40) == 0) ||
            (value == -1 && (byte & 0x40) != 0)) {
            return WriteU8(byte);
        } else if (!WriteU8(byte | 0x80))
            return false;
    }
}

bool Pulsar::Binary::IWriter::WriteULEB(uint64_t value)
{
    while (true) {
        uint8_t byte = (uint8_t)(value & 0x7F);
        value >>= 7;
        if (value == 0) {
            return WriteU8(byte);
        } else if (!WriteU8(byte | 0x80))
            return false;
    }
}

bool Pulsar::Binary::IWriter::WriteF64(double dbl)
{
    static_assert(sizeof(double) == 8);
    void* dblPtr = (void*)&dbl;
    // TODO: We're assuming doubles are stored with the same endianness as Integers
    // If we encounter any issue we should change this
    uint64_t value = *((uint64_t*)dblPtr);
    if constexpr (IS_LITTLE_ENDIAN) {
        return WriteData(8, (uint8_t*)&value);
    } else {
        uint8_t data[8] {
            (uint8_t)( value        & 0xFF),
            (uint8_t)((value >>  8) & 0xFF),
            (uint8_t)((value >> 16) & 0xFF),
            (uint8_t)((value >> 24) & 0xFF),
            (uint8_t)((value >> 32) & 0xFF),
            (uint8_t)((value >> 40) & 0xFF),
            (uint8_t)((value >> 48) & 0xFF),
            (uint8_t)((value >> 56) & 0xFF),
        };
        return WriteData(8, data);
    }
}

bool Pulsar::Binary::IWriter::WriteI64(int64_t value)
{
    return WriteSLEB(value);
}

bool Pulsar::Binary::IWriter::WriteU64(uint64_t value)
{
    return WriteULEB(value);
}

bool Pulsar::Binary::IWriter::WriteU32(uint32_t value)
{
    if constexpr (IS_LITTLE_ENDIAN) {
        return WriteData(4, (uint8_t*)&value);
    } else {
        uint8_t data[4] {
            (uint8_t)( value        & 0xFF),
            (uint8_t)((value >>  8) & 0xFF),
            (uint8_t)((value >> 16) & 0xFF),
            (uint8_t)((value >> 24) & 0xFF),
        };
        return WriteData(4, data);
    }
}

bool Pulsar::Binary::IWriter::WriteU16(uint16_t value)
{
    if constexpr (IS_LITTLE_ENDIAN) {
        return WriteData(2, (uint8_t*)&value);
    } else {
        uint8_t data[2] {
            (uint8_t)( value       & 0xFF),
            (uint8_t)((value >> 8) & 0xFF),
        };
        return WriteData(2, data);
    }
}

bool Pulsar::Binary::IWriter::WriteU8(uint8_t value)
{
    return WriteData(1, &value);
}
