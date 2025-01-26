#include "pulsar/utf8.h"

Pulsar::UTF8::Codepoint Pulsar::UTF8::Decoder::Next()
{
    if (!HasData()) return 0;

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
        m_Data.RemovePrefix(m_Data.Length());
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
            if (IsValidCodepoint(code))
                return code;
        }
        m_IsInvalidEncoding = true;
        m_Data.RemovePrefix(m_Data.Length());
        return REPLACEMENT_CHARACTER;
    }

    if (m_Data.Length() < 3) {
        m_IsInvalidEncoding = true;
        m_Data.RemovePrefix(m_Data.Length());
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
            if (IsValidCodepoint(code))
                return code;
        }
        m_IsInvalidEncoding = true;
        m_Data.RemovePrefix(m_Data.Length());
        return REPLACEMENT_CHARACTER;
    }

    if (m_Data.Length() < 4) {
        m_IsInvalidEncoding = true;
        m_Data.RemovePrefix(m_Data.Length());
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
            if (IsValidCodepoint(code))
                return code;
        }
        m_IsInvalidEncoding = true;
        m_Data.RemovePrefix(m_Data.Length());
        return REPLACEMENT_CHARACTER;
    }

    m_IsInvalidEncoding = true;
    m_Data.RemovePrefix(m_Data.Length());
    return REPLACEMENT_CHARACTER;
}

size_t Pulsar::UTF8::Decoder::Skip()
{
    if (!HasData()) return 0;

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
