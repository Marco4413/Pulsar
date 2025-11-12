#ifndef _PULSAR_STRUCTURES_STRING_H
#define _PULSAR_STRUCTURES_STRING_H

#include "pulsar/core.h"

namespace Pulsar
{
    class String
    {
    public:
        using ConstIterator   = const char*;
        using MutableIterator = char*;

        String();
        ~String();

        String(const String& other);
        String(String&& other);

        String(size_t count, char ch);
        String(const char* data);
        String(const char* data, size_t length);

        void Resize(size_t newLength);
        void Reserve(size_t newCapacity);

        char operator[](size_t idx) const
        {
            PULSAR_ASSERT(idx < Length(), "String index out of bounds.");
            return m_Data[idx];
        }

        char& operator[](size_t idx)
        {
            PULSAR_ASSERT(idx < Length(), "String index out of bounds.");
            return m_Data[idx];
        }

        String operator+(const String& other) const;
        String operator+(const char* cstr) const;
        String operator+(char ch) const;

        bool operator==(const String& other) const;
        bool operator!=(const String& other) const { return !(*this == other); }

        String& operator+=(const String& other);
        String& operator+=(const char* cstr);
        String& operator+=(char ch);

        String& operator=(const String& other);
        String& operator=(String&& other);

        int64_t Compare(const String& other) const;
        String SubString(size_t startIdx, size_t endIdx) const;

        char* Data()             { return m_Data; }
        const char* Data() const { return m_Data; }

        const char* CString() const { return m_Data ? m_Data : ""; }

        size_t Length() const { return m_Length; }
        size_t Capacity() const { return m_Capacity; }

        ConstIterator Begin() const { return m_Data; }
        ConstIterator End()   const { return m_Data+m_Length; }
        MutableIterator Begin() { return m_Data; }
        MutableIterator End()   { return m_Data+m_Length; }

        PULSAR_ITERABLE_IMPL(String, ConstIterator, MutableIterator)

    private:
        char* m_Data;
        size_t m_Length;
        size_t m_Capacity;
    };

    String UIntToString(uint64_t n);
    String IntToString(int64_t n);
    String DoubleToString(double n, size_t precision=6);
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
