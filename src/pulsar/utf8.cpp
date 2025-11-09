#include "pulsar/utf8.h"

Pulsar::String Pulsar::UTF8::Encode(Codepoint code)
{
    char encoded[MAX_ENCODED_SIZE] = {0};

    if (!IsValidCodepoint(code))
        code = REPLACEMENT_CHARACTER;

    size_t encodedLength = GetEncodedSize(code);
    switch (encodedLength) {
    case 1: {
        // We don't really need & 0x7F
        // However, I prefer to play it safe.
        encoded[0] = (char)(code & 0x7F);
    } break;
    case 2: {
        encoded[0] = (char)(( code >>  6) | 0xC0);
        encoded[1] = (char)(((code      ) & 0x3F) | 0x80);
    } break;
    case 3: {
        encoded[0] = (char)(( code >> 12) | 0xE0);
        encoded[1] = (char)(((code >>  6) & 0x3F) | 0x80);
        encoded[2] = (char)(((code      ) & 0x3F) | 0x80);
    } break;
    case 4: {
        encoded[0] = (char)(( code >> 18) | 0xF0);
        encoded[1] = (char)(((code >> 12) & 0x3F) | 0x80);
        encoded[2] = (char)(((code >>  6) & 0x3F) | 0x80);
        encoded[3] = (char)(((code      ) & 0x3F) | 0x80);
    } break;
    default:
        PULSAR_ASSERT(false, "Unhandled encoding length.");
        return "";
    }

    return String(encoded, encodedLength);
}

Pulsar::UTF8::Codepoint Pulsar::UTF8::Decoder::Next()
{
    if (!*this) return 0;

    // == PEEK CACHE == //
    if (m_PeekedCodepoint != 0 && GetEncodedSize(m_PeekedCodepoint) <= m_Data.Length()) {
        m_Data.RemovePrefix(GetEncodedSize(m_PeekedCodepoint));
        ++m_DecodedCodepoints;

        Codepoint next = m_PeekedCodepoint;
        m_PeekedCodepoint = 0;
        return next;
    }

    m_PeekedCodepoint = 0;
    // == PEEK CACHE == //

    unsigned char byte1 = (unsigned char)m_Data[0];
    if ((byte1 & 0x80) == 0) {
        // ASCII is always valid Unicode
        m_Data.RemovePrefix(1);
        ++m_DecodedCodepoints;
        return byte1;
    }

    if (m_Data.Length() < 2) {
        m_IsInvalidEncoding = true;
        return REPLACEMENT_CHARACTER;
    }

    if ((byte1 & 0xE0) == 0xC0) {
        unsigned char byte2 = (unsigned char)m_Data[1];
        if ((byte2 & 0xC0) == 0x80) {
            m_Data.RemovePrefix(2);
            ++m_DecodedCodepoints;
            Codepoint code =
                ((Codepoint)(byte1 & 0x1F) << 6) |
                ((Codepoint)(byte2 & 0x3F)     ) ;
            if (GetEncodedSize(code) == 2 && IsValidCodepoint(code))
                return code;
        }
        m_IsInvalidEncoding = true;
        return REPLACEMENT_CHARACTER;
    }

    if (m_Data.Length() < 3) {
        m_IsInvalidEncoding = true;
        return REPLACEMENT_CHARACTER;
    }

    if ((byte1 & 0xF0) == 0xE0) {
        unsigned char byte2 = (unsigned char)m_Data[1];
        unsigned char byte3 = (unsigned char)m_Data[2];
        if (((byte2 | byte3) & 0xC0) == 0x80) {
            m_Data.RemovePrefix(3);
            ++m_DecodedCodepoints;
            Codepoint code =
                ((Codepoint)(byte1 & 0x0F) << 12) |
                ((Codepoint)(byte2 & 0x3F) <<  6) |
                ((Codepoint)(byte3 & 0x3F)      ) ;
            if (GetEncodedSize(code) == 3 && IsValidCodepoint(code))
                return code;
        }
        m_IsInvalidEncoding = true;
        return REPLACEMENT_CHARACTER;
    }

    if (m_Data.Length() < 4) {
        m_IsInvalidEncoding = true;
        return REPLACEMENT_CHARACTER;
    }

    if ((byte1 & 0xF8) == 0xF0) {
        unsigned char byte2 = (unsigned char)m_Data[1];
        unsigned char byte3 = (unsigned char)m_Data[2];
        unsigned char byte4 = (unsigned char)m_Data[3];
        if (((byte2 | byte3 | byte4) & 0xC0) == 0x80) {
            m_Data.RemovePrefix(4);
            ++m_DecodedCodepoints;
            Codepoint code =
                ((Codepoint)(byte1 & 0x07) << 18) |
                ((Codepoint)(byte2 & 0x3F) << 12) |
                ((Codepoint)(byte3 & 0x3F) <<  6) |
                ((Codepoint)(byte4 & 0x3F)      ) ;
            if (GetEncodedSize(code) == 4 && IsValidCodepoint(code))
                return code;
        }
        m_IsInvalidEncoding = true;
        return REPLACEMENT_CHARACTER;
    }

    m_IsInvalidEncoding = true;
    return REPLACEMENT_CHARACTER;
}

size_t Pulsar::UTF8::Decoder::Skip()
{
    if (!*this) return 0;

    // == PEEK CACHE == //
    if (m_PeekedCodepoint != 0 && GetEncodedSize(m_PeekedCodepoint) <= m_Data.Length()) {
        size_t peekSize = GetEncodedSize(m_PeekedCodepoint);
        m_Data.RemovePrefix(peekSize);
        ++m_DecodedCodepoints;

        m_PeekedCodepoint = 0;
        return peekSize;
    }

    m_PeekedCodepoint = 0;
    // == PEEK CACHE == //

    unsigned char byte1 = (unsigned char)m_Data[0];
    if ((byte1 & 0x80) == 0) {
        m_Data.RemovePrefix(1);
        ++m_DecodedCodepoints;
        return 1;
    }

    if (m_Data.Length() < 2) {
        m_IsInvalidEncoding = true;
        return 0;
    }

    if ((byte1 & 0xE0) == 0xC0) {
        m_Data.RemovePrefix(2);
        ++m_DecodedCodepoints;
        return 2;
    }

    if (m_Data.Length() < 3) {
        m_IsInvalidEncoding = true;
        return 0;
    }

    if ((byte1 & 0xF0) == 0xE0) {
        m_Data.RemovePrefix(3);
        ++m_DecodedCodepoints;
        return 3;
    }

    if (m_Data.Length() < 4) {
        m_IsInvalidEncoding = true;
        return 0;
    }

    if ((byte1 & 0xF8) == 0xF0) {
        m_Data.RemovePrefix(4);
        ++m_DecodedCodepoints;
        return 4;
    }

    m_IsInvalidEncoding = true;
    return 0;
}

Pulsar::UTF8::Codepoint Pulsar::UTF8::Decoder::Peek()
{
    // == PEEK CACHE == //
    if (m_PeekedCodepoint != 0)
        return m_PeekedCodepoint;

    Decoder decoder = *this;
    m_PeekedCodepoint = decoder.Next();
    return m_PeekedCodepoint;
    // == PEEK CACHE == //
}

Pulsar::UTF8::Codepoint Pulsar::UTF8::Decoder::Peek() const
{
    // == PEEK CACHE == //
    if (m_PeekedCodepoint != 0)
        return m_PeekedCodepoint;
    // == PEEK CACHE == //
    Decoder decoder = *this;
    return decoder.Next();
}

Pulsar::UTF8::Codepoint Pulsar::UTF8::Decoder::Peek(size_t lookAhead) const
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
