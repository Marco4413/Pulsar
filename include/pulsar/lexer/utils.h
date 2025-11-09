#ifndef _PULSAR_LEXER_UTILS_H
#define _PULSAR_LEXER_UTILS_H

#include "pulsar/core.h"

#include "pulsar/structures/string.h"
#include "pulsar/structures/stringview.h"
#include "pulsar/utf8.h"

namespace Pulsar
{
    // maxHexDigits must be >= 1, conversion will stop when outputted digits are >= maxHexDigits if padToMaxDigits is false
    void PutHexString(String& out, uint64_t n, size_t maxHexDigits, bool padToMaxDigits=false);

    String ToStringLiteral(StringView str);

    constexpr bool IsIdentifierStart(Unicode::Codepoint code) { return Unicode::IsAlpha(code) || code == '_'; }
    constexpr bool IsIdentifierContinuation(Unicode::Codepoint code)
    {
        return IsIdentifierStart(code) || Unicode::IsDecimalDigit(code)
            || (code >= '<' && code <= '?') // < = > ?
            || code == '+' || code == '-'
            || code == '*' || code == '/'
            || code == '!';
    }

    // Checks if the given String is a valid Identifier
    // As described by IsIdentifierStart/Continuation
    bool IsIdentifier(StringView s);
}

#endif // _PULSAR_LEXER_UTILS_H
