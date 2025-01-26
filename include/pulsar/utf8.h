#ifndef _PULSAR_UTF8_H
#define _PULSAR_UTF8_H

#include "pulsar/core.h"

#include "pulsar/unicode.h"
#include "pulsar/structures/string.h"
#include "pulsar/structures/stringview.h"

namespace Pulsar
{
// Nested namespace to keep some compatibility with older C++20 compilers.

namespace UTF8
{
    using Codepoint = Unicode::Codepoint;
    constexpr Codepoint MAX_CODEPOINT = Unicode::MAX_CODEPOINT;
    constexpr Codepoint REPLACEMENT_CHARACTER = Unicode::REPLACEMENT_CHARACTER;
    constexpr bool IsValidCodepoint(Codepoint code) { return Unicode::IsValidCodepoint(code); }

    constexpr size_t MAX_ENCODED_SIZE = 4;
    constexpr size_t GetEncodedSize(Codepoint code)
    {
        if (code <= 0x7F) return 1;
        else if (code <= 0x7FF) return 2;
        else if (code <= 0xFFFF) return 3;
        return 4;
    }

    Pulsar::String Encode(Codepoint code);

    class Decoder
    {
    public:
        Decoder(StringView data) :
            m_Data(data)
        {}

        Decoder(const Decoder&) = default;

        ~Decoder() = default;

        Decoder& operator=(const Decoder&) = default;

        operator bool() const { return !IsInvalidEncoding() && HasData(); }

        bool HasData() const { return m_Data.Length() > 0; }
        bool IsInvalidEncoding() const { return m_IsInvalidEncoding; }

        size_t GetRemainingBytes() const    { return m_Data.Length(); }
        size_t GetDecodedBytes() const      { return m_Data.GetStart(); }
        size_t GetDecodedCodepoints() const { return m_DecodedCodepoints; }

        StringView Data() const { return m_Data; }

        Codepoint Next();
        // non-const Peek with caching
        Codepoint Peek();
        size_t Skip();

        Codepoint Peek() const
        {
            // == PEEK CACHE == //
            if (m_PeekedCodepoint != 0)
                return m_PeekedCodepoint;
            // == PEEK CACHE == //
            Decoder decoder = *this;
            return decoder.Next();
        }

        Codepoint Peek(size_t lookAhead) const
        {
            if (lookAhead == 0) {
                return '\0';
            } else if (lookAhead == 1) {
                return Peek();
            }

            Decoder decoder = *this;
            for (size_t i = 1; i < lookAhead && decoder; ++i)
                decoder.Skip();
            return decoder.Next();
        }

    private:
        StringView m_Data;

        Codepoint m_PeekedCodepoint = 0;
        bool m_IsInvalidEncoding = false;
        size_t m_DecodedCodepoints = 0;
    };

    inline size_t Length(StringView str)
    {
        Decoder decoder(str);
        size_t len = 0;
        while (decoder && decoder.Skip())
            ++len;
        return len;
    }
}
}

#endif // _PULSAR_UTF8_H
