#ifndef _PULSAR_BINARY_WRITER_H
#define _PULSAR_BINARY_WRITER_H

#include "pulsar/core.h"

#include "pulsar/binary.h"

namespace Pulsar::Binary
{
    class IWriter
    {
    public:
        virtual bool WriteData(uint64_t size, const uint8_t* data) = 0;
        
        // These all use WriteData
        bool WriteSLEB(int64_t value);
        bool WriteULEB(uint64_t value);
        bool WriteF64(double value);
        bool WriteI64(int64_t value);
        bool WriteU64(uint64_t value);
        bool WriteU32(uint32_t value);
        bool WriteU16(uint16_t value);
        bool WriteU8(uint8_t value);
    };
}

#endif // _PULSAR_BINARY_WRITER_H
