#ifndef _PULSAR_STRUCTURES_STRING_H
#define _PULSAR_STRUCTURES_STRING_H

#include "pulsar/core.h"

namespace Pulsar
{
    class String
    {
    public:
        String() : String(0, '\0') { }
        ~String() { PULSAR_FREE(m_Data); }

        String(const String& other) { *this = other; }
        String(String&& other) { *this = other; }

        String(size_t count, char ch)
        {
            Resize(count);
            PULSAR_MEMSET((void*)m_Data, ch, count*sizeof(char));
        }

        String(const char* data)
            : String(data, std::strlen(data)) { }

        String(const char* data, size_t length)
        {
            Resize(length);
            PULSAR_MEMCPY(m_Data, data, length*sizeof(char));
        }

        void Resize(size_t newLength)
        {
            if (!m_Data || newLength > m_Capacity)
                Reserve(newLength*3/2);
            m_Length = newLength;
            m_Data[m_Length] = '\0';
        }

        void Reserve(size_t newCapacity)
        {
            if (m_Data && newCapacity <= m_Capacity)
                return;
            m_Data = (char*)PULSAR_REALLOC((void*)m_Data, (newCapacity+1) * sizeof(char));
            m_Capacity = newCapacity;
            m_Data[m_Capacity] = '\0';
        }

        char operator[](size_t idx) const  { return m_Data[idx]; }
        const char& operator[](size_t idx) { return m_Data[idx]; }

        String operator+(const String& other) const
        {
            String concat = *this;
            concat += other;
            return concat;
        }

        String operator+(char ch) const
        {
            String concat = *this;
            concat += ch;
            return concat;
        }

        bool operator==(const String& other) const
        {
            if (Length() != other.Length())
                return false;
            for (size_t i = 0; i < other.Length(); i++) {
                if ((*this)[i] != other[i])
                    return false;
            }
            return true;
        }

        bool operator!=(const String& other) const { return !(*this == other); }

        String& operator+=(const String& other)
        {
            size_t appendIdx = m_Length;
            Resize(m_Length+other.m_Length);
            PULSAR_MEMCPY(
                (void*)(m_Data+appendIdx), other.m_Data,
                other.m_Length*sizeof(char));
            return *this;
        }

        String& operator+=(char ch)
        {
            size_t appendIdx = m_Length;
            Resize(m_Length+1);
            m_Data[appendIdx] = ch;
            return *this;
        }

        String& operator=(const String& other)
        {
            Resize(other.m_Length);
            PULSAR_MEMCPY((void*)m_Data, other.m_Data, other.m_Length*sizeof(char));
            return *this;
        }

        String& operator=(String&& other)
        {
            PULSAR_FREE((void*)m_Data);
            m_Data = other.m_Data;
            m_Length = other.m_Length;
            m_Capacity = other.m_Capacity;
            other.m_Data = nullptr;
            other.m_Length = 0;
            other.m_Capacity = 0;
            return *this;
        }

        const char* Data() const { return m_Data; }
        size_t Length() const { return m_Length; }
        size_t Capacity() const { return m_Capacity; }
    private:
        char* m_Data = nullptr;
        size_t m_Length = 0;
        size_t m_Capacity = 0;
    };
}

template<>
struct std::hash<Pulsar::String>
{
    size_t operator()(const Pulsar::String& str) const
    {
        size_t hash = 0;
        for (size_t i = 0; i < str.Length(); i++)
            hash += std::hash<char>{}(str[i]);
        return hash;
    }
};

inline Pulsar::String operator+(const char* a, const Pulsar::String& b)
{
    return Pulsar::String(a) + b;
}

#endif // _PULSAR_STRUCTURES_STRING_H
