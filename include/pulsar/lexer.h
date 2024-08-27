#ifndef _PULSAR_LEXER_H
#define _PULSAR_LEXER_H

#include "pulsar/core.h"

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
        size_t Char;
        size_t Index;
        size_t CharSpan;
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

    inline bool IsControlCharacter(char ch)      { return (ch >= 0x00 && ch <= 0x1F) || ch == 0x7F; }
    inline bool IsSpace(char ch)                 { return ch == ' ' || (ch >= '\t' && ch <= '\r'); }
    inline bool IsAlphaCharacter(char ch)        { return (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z'); }
    inline bool IsDigit(char ch)                 { return ch >= '0' && ch <= '9'; }
    inline bool IsHexDigit(char ch)              { return IsDigit(ch) || (ch >= 'a' && ch <= 'f') || (ch >= 'A' && ch <= 'F'); }
    inline bool IsOctDigit(char ch)              { return ch >= '0' && ch <= '7'; }
    inline bool IsBinDigit(char ch)              { return ch == '0' || ch == '1'; }
    inline bool IsAlphaNumericCharacter(char ch) { return IsAlphaCharacter(ch) || IsDigit(ch); }
    inline char ToLowerCase(char ch)             { return (ch >= 'A' && ch <= 'Z') ? ch+('a'-'A') : ch; }

    inline bool IsIdentifierStart(char ch) { return IsAlphaCharacter(ch) || ch == '_'; }
    inline bool IsIdentifierContinuation(char ch)
    {
        return IsIdentifierStart(ch) || IsDigit(ch)
            || (ch >= '<' && ch <= '?') // < = > ?
            || ch == '+' || ch == '-'
            || ch == '*' || ch == '/'
            || ch == '!';
    }

    // Checks if the given String is a valid Identifier
    // As described by IsIdentifierStart/Continuation
    bool IsIdentifier(const String& s);

    class Lexer
    {
    public:
        Lexer(const String& src)
            : m_Source(src), m_SourceView(m_Source) { }
        Lexer(String&& src)
            : m_Source(src), m_SourceView(m_Source) { }

        Lexer(const Lexer& other)
            : m_Source(other.m_Source), m_SourceView(m_Source)
        {
            m_SourceView.RemovePrefix(other.m_SourceView.GetStart());
            m_SourceView.RemoveSuffix(m_SourceView.Length()-other.m_SourceView.Length());
            m_Line = other.m_Line;
            m_LineStartIdx = other.m_LineStartIdx;
        }

        Lexer(Lexer&& other)
            : m_Source(std::move(other.m_Source)), m_SourceView(m_Source)
        {
            m_SourceView.RemovePrefix(other.m_SourceView.GetStart());
            m_SourceView.RemoveSuffix(m_SourceView.Length()-other.m_SourceView.Length());
            other.m_SourceView.RemoveSuffix(other.m_SourceView.Length());
            m_Line = other.m_Line;
            m_LineStartIdx = other.m_LineStartIdx;
        }

        Lexer& operator=(const Lexer&) = delete;
        Lexer& operator=(Lexer&&) = delete;

        // Skips a sha-bang at the current position
        // Use this after creating a Lexer to ignore it
        bool SkipShaBang();

        Token NextToken()               { return ParseNextToken(); }
        bool IsEndOfFile() const        { return m_SourceView.Length() == 0; }
        const String& GetSource() const { return m_Source; }
        SourcePosition GetSourcePosition(size_t span=0) const
            { return { m_Line, m_SourceView.GetStart() - m_LineStartIdx, m_SourceView.GetStart(), span }; }
    private:
        Token ParseNextToken();
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
        size_t SkipWhitespaces();
        size_t SkipComments();

        template<typename ...Args>
        Token TrimToToken(size_t length, Args&& ...args)
        {
            Token token(std::forward<Args>(args)...);
            token.SourcePos = GetSourcePosition(length);
            if (length > 0)
                m_SourceView.RemovePrefix(length);
            return token;
        }

        Token CreateNoneToken() const
        {
            Token token = Token(TokenType::None);
            token.SourcePos = GetSourcePosition(0);
            return token;
        }
    private:
        String m_Source;
        StringView m_SourceView;
        size_t m_Line = 0;
        size_t m_LineStartIdx = 0;
    };
}


#endif // _PULSAR_LEXER_H
