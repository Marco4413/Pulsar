#include "pulsar/lexer/utils.h"

void Pulsar::PutHexString(String& out, uint64_t n, size_t maxHexDigits)
{
    constexpr size_t MAX_DIGITS = sizeof(n) * 2;
    size_t digitsCount = 0;
    // digits is null terminated
    char digits[MAX_DIGITS+1] = {0};

    do {
        size_t digitIdx = MAX_DIGITS - ++digitsCount;
        uint8_t digit = n & 0xF;
        n = n >> 4;

        if (digit >= 0xA) {
            digits[digitIdx] = (char)('A'+digit-10);
        } else {
            digits[digitIdx] = (char)('0'+digit);
        }
    } while (n > 0 && digitsCount < maxHexDigits);

    out += &digits[MAX_DIGITS-digitsCount];
}

Pulsar::String Pulsar::ToStringLiteral(StringView str)
{
    String lit(1, '"');
    size_t decoderOffset = 0;
    UTF8::Decoder decoder(str);
    while (decoder.HasData()) {
        UTF8::Codepoint code = decoder.Next();
        if (decoder.IsInvalidEncoding()) {
            size_t byteIdx = decoderOffset + decoder.GetDecodedBytes();
            uint8_t byte = (uint8_t)(str[byteIdx]);

            lit += "\\x";
            PutHexString(lit, byte, 2);
            lit += ';';

            decoderOffset = byteIdx+1;
            // Create new decoder which points at the byte after the bad one.
            // Maybe put this into a method of the Decoder.
            decoder = UTF8::Decoder(StringView(
                str.Data()   + decoderOffset,
                str.Length() - decoderOffset
            ));
            continue;
        }

        switch (code) {
        case '"':  lit += "\\\""; break;
        case '\n': lit += "\\n";  break;
        case '\r': lit += "\\r";  break;
        case '\t': lit += "\\t";  break;
        default:
            if (Unicode::IsAscii(code)) {
                if (Unicode::IsControl(code)) {
                    lit += "\\x";
                    PutHexString(lit, code, 2);
                    lit += ';';
                } else {
                    lit += (char)(code);
                }
            } else {
                lit += "\\u";
                // See UTF8::MAX_CODEPOINT for digits
                PutHexString(lit, code, 6);
                lit += ';';
            }
        }
    }
    lit += '"';
    return lit;
}

bool Pulsar::IsIdentifier(StringView s)
{
    if (s.Length() == 0)
        return false;

    UTF8::Decoder decoder(s);
    if (!IsIdentifierStart(decoder.Peek()))
        return false;

    while (decoder && IsIdentifierContinuation(decoder.Next()));
    return !decoder.HasData() && !decoder.IsInvalidEncoding();
}
