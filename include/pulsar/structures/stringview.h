#ifndef _PULSAR_STRUCTURES_STRINGVIEW_H
#define _PULSAR_STRUCTURES_STRINGVIEW_H

#include "pulsar/core.h"

#include "pulsar/structures/string.h"

namespace Pulsar
{
    class StringView
    {
    public:
        StringView(const String& str)
            : StringView(str.Data(), str.Length()) { }
        StringView(const char* str, size_t length)
            : m_Data(str), m_End(length) { }
        
        String GetPrefix(size_t count) const { return String(m_Data+m_Start, count); }
        void RemovePrefix(size_t count) { m_Start += count; };
        void RemoveSuffix(size_t count) { m_End -= count; };
        char operator[](size_t idx) const { return m_Data[m_Start+idx]; }
        size_t Length() const { return m_Start >= m_End ? 0 : m_End - m_Start; }
        size_t GetStart() const { return m_Start; }
        const char* CStringFrom(size_t idx) const { return &m_Data[m_Start+idx]; }
    private:
        const char* m_Data = nullptr;
        size_t m_Start = 0;
        size_t m_End = 0;
    };
}

#endif // _PULSAR_STRUCTURES_STRINGVIEW_H
