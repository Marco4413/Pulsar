#ifndef _PULSAR_UNICODE_H
#define _PULSAR_UNICODE_H

#include "pulsar/core.h"

namespace Pulsar
{
// Nested namespace to keep some compatibility with older C++20 compilers.

namespace Unicode
{
    using Codepoint = uint32_t;
    constexpr Codepoint MAX_CODEPOINT = 0x10FFFF;
    constexpr Codepoint REPLACEMENT_CHARACTER = 0xFFFD;
    constexpr bool IsValidCodepoint(Codepoint code)
    {
        // TODO: There are some codepoints in between (0,MAX_CODEPOINT] which should be removed.
        return code > 0 && code <= MAX_CODEPOINT;
    }

    /**
     * Gets the display width of a codepoint on a terminal.
     * See:
     * - https://unicode.org/reports/tr11/tr11-43.html
     * - https://unicode.org/Public/16.0.0/ucd/EastAsianWidth.txt
     * - https://unicode.org/Public/emoji/16.0/emoji-sequences.txt
     * 
     * NOTE: Multi-codepoint emojis may break any calculation.
     */
    size_t Width(Codepoint code);

    constexpr bool IsAscii(Codepoint code)        { return code <= 0x7F; }
    constexpr bool IsControl(Codepoint code)      { return code <= 0x1F || code == 0x7F; }
    constexpr bool IsWhiteSpace(Codepoint code)   { return code == ' ' || (code >= '\t' && code <= '\r'); }
    constexpr bool IsAlpha(Codepoint code)        { return (code >= 'a' && code <= 'z') || (code >= 'A' && code <= 'Z'); }
    constexpr bool IsDecimalDigit(Codepoint code) { return code >= '0' && code <= '9'; }
    constexpr bool IsHexDigit(Codepoint code)     { return IsDecimalDigit(code) || (code >= 'a' && code <= 'f') || (code >= 'A' && code <= 'F'); }
    constexpr bool IsOctalDigit(Codepoint code)   { return code >= '0' && code <= '7'; }
    constexpr bool IsBinaryDigit(Codepoint code)  { return code == '0' || code == '1'; }
    constexpr bool IsAlphaNumeric(Codepoint code) { return IsAlpha(code) || IsDecimalDigit(code); }
    constexpr Codepoint ToLowerCase(Codepoint code)
    {
        return (code >= 'A' && code <= 'Z')
            ? code + ('a'-'A')
            : code;
    }
}
}

#endif // _PULSAR_UNICODE_H
