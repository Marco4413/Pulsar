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
            : StringView(str.CString(), str.Length()) { }

        StringView(const char* str)
            : StringView(str, std::strlen(str)) { }

        StringView(const char* str, size_t length)
            : m_Begin(str), m_End(str+length) { }

        StringView(const StringView&) = default;
        StringView(StringView&&) = default;
        StringView& operator=(const StringView&) = default;
        StringView& operator=(StringView&&) = default;

        String ToString() const
        {
            return String(m_Begin, Length());
        }

        String PrefixToString(size_t count) const
        {
            PULSAR_ASSERT(count <= Length(), "Getting more characters than the available amount.");
            return String(m_Begin, count);
        }

        void RemovePrefix(size_t count)
        {
            PULSAR_ASSERT(count <= Length(), "Removing more characters than the available amount.");
            m_Begin += count;
        }

        void RemoveSuffix(size_t count)
        {
            PULSAR_ASSERT(count <= Length(), "Removing more characters than the available amount.");
            m_End -= count;
        }

        void SetLength(size_t newLength)
        {
            PULSAR_ASSERT(newLength <= Length(), "Setting length to a value greater than the max length.");
            m_End = m_Begin + newLength;
        }

        char operator[](size_t idx) const
        {
            PULSAR_ASSERT(idx < Length(), "StringView index out of bounds.");
            return m_Begin[idx];
        }

        bool operator==(const StringView& other) const
        {
            if (Length() != other.Length())
                return false;
            for (size_t i = 0; i < other.Length(); i++) {
                if ((*this)[i] != other[i])
                    return false;
            }
            return true;
        }

        bool operator!=(const StringView& other) const { return !(*this == other); }

        size_t Length() const   { return Empty() ? 0 : m_End - m_Begin; }
        bool Empty() const      { return m_Begin >= m_End; }
        // Data may not be null-terminated, use StringView::Length to get the remaining characters.
        const char* Data() const { return m_Begin; }
    private:
        const char* m_Begin;
        const char* m_End;
    };
}

template<>
struct std::hash<Pulsar::StringView>
{
    size_t operator()(const Pulsar::StringView& strView) const
    {
        size_t hash = 0;
        for (size_t i = 0; i < strView.Length(); i++)
            hash += std::hash<char>{}(strView[i]);
        return hash;
    }
};

#endif // _PULSAR_STRUCTURES_STRINGVIEW_H
