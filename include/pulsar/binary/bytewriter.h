#ifndef _PULSAR_BINARY_BYTEWRITER_H
#define _PULSAR_BINARY_BYTEWRITER_H

#include "pulsar/core.h"

#include "pulsar/binary/writer.h"
#include "pulsar/structures/list.h"

namespace Pulsar::Binary
{
    class ByteWriter : public IWriter
    {
    public:
        ByteWriter()
            : ByteWriter(0) { }

        ByteWriter(size_t initCap)
            : m_Bytes(initCap) { }

        bool WriteData(uint64_t size, const uint8_t* data) override
        {
            size_t idx = m_Bytes.Size();
            m_Bytes.Resize(m_Bytes.Size() + (size_t)size);
            PULSAR_MEMCPY((void*)&m_Bytes[idx], (void*)data, (size_t)size);
            return true;
        }

        List<uint8_t>& Bytes()             { return m_Bytes; }
        const List<uint8_t>& Bytes() const { return m_Bytes; }

    private:
        List<uint8_t> m_Bytes;
    };
}

#endif // _PULSAR_BINARY_BYTEWRITER_H
