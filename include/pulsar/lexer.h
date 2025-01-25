#ifndef _PULSAR_LEXER_H
#define _PULSAR_LEXER_H

#include "pulsar/core.h"

#include "pulsar/utf8.h"
#include "pulsar/structures/hashmap.h"
#include "pulsar/structures/stringview.h"

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

    static const HashMap<String, TokenType> Keywords {
        { "not",      TokenType::KW_Not      },
        { "if",       TokenType::KW_If       },
        { "else",     TokenType::KW_Else     },
        { "end",      TokenType::KW_End      },
        { "global",   TokenType::KW_Global   },
        { "const",    TokenType::KW_Const    },
        { "do",       TokenType::KW_Do       },
        { "while",    TokenType::KW_While    },
        { "break",    TokenType::KW_Break    },
        { "continue", TokenType::KW_Continue },
        { "local",    TokenType::KW_Local    },
    };

    static const HashMap<String, int64_t> CompilerDirectives {
        { "include", TOKEN_CD_INCLUDE },
    };

    const char* TokenTypeToString(TokenType ttype);

    String ToStringLiteral(const String& str);

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
    bool IsIdentifier(const String& s);

    class Lexer
    {
    public:
        using Decoder   = UTF8::Decoder;
        using Codepoint = UTF8::Codepoint;

    public:
        Lexer(const String& src) :
            m_Decoder(src)
        {}

        Lexer(const Lexer& other) = default;

        ~Lexer() = default;

        Lexer& operator=(const Lexer&) = default;

        // Skips a sha-bang at the current position
        // Use this after creating a Lexer to ignore it
        bool SkipShaBang();

        Token NextToken();
        bool IsEndOfFile() const { return !m_Decoder; }

    private:
        Token ParseIdentifier();
        Token ParseLabel();
        Token ParseCompilerDirective();
        Token ParseIntegerLiteral();
        Token ParseHexIntegerLiteral();
        Token ParseOctIntegerLiteral();
        Token ParseBinIntegerLiteral();
        Token ParseDoubleLiteral();
        Token ParseStringLiteral();
        Token ParseCharacterLiteral();

        void SkipUntilNewline();
        bool SkipWhiteSpaces();
        bool SkipComments();

        template<typename ...Args>
        Token PullToken(Decoder decoder, Args&& ...args)
        {
            // size_t tokenBytes = decoder.GetDecodedBytes() - m_Decoder.GetDecodedBytes();
            size_t charSpan = decoder.GetDecodedCodepoints() - m_Decoder.GetDecodedCodepoints();

            Token token(std::forward<Args>(args)...);
            token.SourcePos = {
                .Line = m_Line,
                .Char = m_Decoder.GetDecodedCodepoints() - m_LineStartCodepoint,
                .Index = m_Decoder.GetDecodedBytes(),
                .CharSpan = charSpan
            };

            m_Decoder = decoder;
            return token;
        }

        Token CreateNoneToken() const
        {
            Token token(TokenType::None);
            token.SourcePos = {0,0,0,0};
            return token;
        }
    private:
        Decoder m_Decoder;

        size_t m_Line = 0;
        size_t m_LineStartCodepoint = 0;
    };
}


#endif // _PULSAR_LEXER_H
