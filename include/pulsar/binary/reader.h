#ifndef _PULSAR_BINARY_READER_H
#define _PULSAR_BINARY_READER_H

#include "pulsar/core.h"

#include "pulsar/binary.h"

namespace Pulsar::Binary
{
    class IReader
    {
    public:
        virtual bool ReadData(uint64_t size, uint8_t* out) = 0;
        
        // These all use ReadData
        bool ReadSLEB(int64_t& value);
        bool ReadULEB(uint64_t& value);
        bool ReadF64(double& value);
        bool ReadI64(int64_t& value);
        bool ReadU64(uint64_t& value);
        bool ReadU32(uint32_t& value);
        bool ReadU16(uint16_t& value);
        bool ReadU8(uint8_t& value);
    };
}

#endif // _PULSAR_BINARY_READER_H
