#ifndef _PULSAR_LEXER_TOKEN_H
#define _PULSAR_LEXER_TOKEN_H

#include "pulsar/core.h"

#include "pulsar/structures/string.h"

namespace Pulsar
{
    enum class TokenType
    {
        None = 0,
        EndOfFile = 1,
        Identifier,
        OpenParenth,
        CloseParenth,
        OpenBracket,
        CloseBracket,
        IntegerLiteral,
        DoubleLiteral,
        StringLiteral,
        Plus, Minus,
        Star, Slash,
        Modulus,
        BitAnd, BitOr, BitNot, BitXor,
        BitShiftLeft, BitShiftRight,
        FullStop,
        Negate,
        Colon,
        Comma,
        RightArrow, LeftArrow, BothArrows,
        Equals, NotEquals,
        Less, LessOrEqual,
        More, MoreOrEqual,
        PushReference,
        KW_Not,
        KW_If, KW_Else, KW_End,
        KW_Global, KW_Const,
        KW_Do, KW_While, KW_Break, KW_Continue,
        KW_Local,
        CompilerDirective,
        Label,
    };

    constexpr int64_t TOKEN_CD_GENERIC = 0;
    constexpr int64_t TOKEN_CD_INCLUDE = 1;

    const char* TokenTypeToString(TokenType ttype);

    struct SourcePosition
    {
        size_t Line;
        // How many codepoints to traverse within Line to reach Index
        size_t Char;
        // Byte index of the Token start (where Line and Char point to)
        size_t Index;
        // How many codepoints this Token takes
        size_t CharSpan;

        bool operator==(const SourcePosition&) const = default;
        bool operator!=(const SourcePosition&) const = default;
    };

    class Token
    {
    public:
        Token() = default;

        Token(TokenType type, const String& val)
            : Type(type), StringVal(val) { }
        Token(TokenType type, String&& val)
            : Type(type), StringVal(val) { }

        Token(TokenType type, int64_t val)
            : Type(type), IntegerVal(val) { }
        Token(TokenType type, double val)
            : Type(type), DoubleVal(val) { }
        Token(TokenType type)
            : Type(type), IntegerVal(0) { }
    
    public:
        TokenType Type = TokenType::None;
        String StringVal = "";
        int64_t IntegerVal = 0;
        double DoubleVal = 0.0;
        SourcePosition SourcePos = {0,0,0,0};
    };
}

#endif // _PULSAR_LEXER_TOKEN_H
