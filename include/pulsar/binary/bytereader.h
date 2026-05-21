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
        // ByteReader treats data as a view, which means
        //  that the pointer must outlive the Reader.
        ByteReader(uint64_t size, const uint8_t* data)
            : m_Index(0)
            , m_Data(data)
            , m_Size(static_cast<size_t>(size))
        {}

        bool ReadData(uint64_t size, uint8_t* data) override
        {
            if (size == 0)
                return true;
            if (IsAtEndOfFile() || !m_Data || (m_Index+(size_t)size) > m_Size)
                return false;
            PULSAR_MEMCPY((void*)data, (void*)&m_Data[m_Index], (size_t)size);
            m_Index += (size_t)size;
            return true;
        }

        void DiscardBytes()
        {
            m_Index = 0;
            m_Data  = nullptr;
            m_Size  = 0;
        }

        bool IsAtEndOfFile() const { return !m_Data || m_Index >= m_Size; }

    private:
        size_t m_Index;
        const uint8_t* m_Data;
        size_t m_Size;
    };
}

#endif // _PULSAR_BINARY_BYTEREADER_H
