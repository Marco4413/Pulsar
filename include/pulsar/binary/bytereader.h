#ifndef _PULSAR_BINARY_BYTEREADER_H
#define _PULSAR_BINARY_BYTEREADER_H

#include "pulsar/core.h"

#include "pulsar/binary/reader.h"
#include "pulsar/structures/list.h"

namespace Pulsar::Binary
{
    class ByteReader : public IReader
    {
    public:
        ByteReader(uint64_t size, uint8_t* data)
        {
            m_Bytes.Resize((size_t)size);
            PULSAR_MEMCPY((void*)m_Bytes.Data(), (void*)data, (size_t)size);
        }

        explicit ByteReader(List<uint8_t>&& bytes)
            : m_Bytes(std::move(bytes)) { }

        bool ReadData(uint64_t size, uint8_t* data) override
        {
            if (size == 0)
                return true;
            if (IsAtEndOfFile() || (m_Index+(size_t)size) > m_Bytes.Size())
                return false;
            PULSAR_MEMCPY((void*)data, (void*)&m_Bytes[m_Index], (size_t)size);
            m_Index += (size_t)size;
            return true;
        }

        void DiscardBytes()
        {
            m_Index = 0;
            m_Bytes.Clear();
        }

        bool IsAtEndOfFile() const { return m_Index >= m_Bytes.Size(); }

    private:
        size_t m_Index = 0;
        List<uint8_t> m_Bytes;
    };
}

#endif // _PULSAR_BINARY_BYTEREADER_H
