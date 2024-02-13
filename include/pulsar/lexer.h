#ifndef _PULSAR_LEXER_H
#define _PULSAR_LEXER_H

#include <cinttypes>
#include <string>
#include <unordered_map>

namespace Pulsar
{
    enum class TokenType
    {
        None = 0,
        Identifier,
        OpenParenth,
        CloseParenth,
        IntegerLiteral,
        DoubleLiteral,
        Plus,
        Minus,
        Star,
        FullStop,
        Negate,
        Colon,
        RightArrow,
        EndOfFile,
        Equals, NotEquals,
        Less, LessOrEqual,
        More, MoreOrEqual,
        KW_If,
        KW_Else,
        KW_End
    };

    static const std::unordered_map<std::string, TokenType> Keywords {
        { "if", TokenType::KW_If },
        { "else", TokenType::KW_Else },
        { "end", TokenType::KW_End }
    };

    const char* TokenTypeToString(TokenType ttype);

    class Token
    {
    public:
        Token(TokenType type, const std::string& val)
            : Type(type), StringVal(val) { }
        Token(TokenType type, std::string&& val)
            : Type(type), StringVal(val) { }

        Token(TokenType type, int64_t val)
            : Type(type), IntegerVal(val) { }
        Token(TokenType type, double val)
            : Type(type), DoubleVal(val) { }
        Token(TokenType type)
            : Type(type), IntegerVal(0) { }

        TokenType Type;
        std::string StringVal = "";
        int64_t IntegerVal = 0;
        double DoubleVal = 0.0;
    };

    bool IsIdentifierStart(int ch);
    bool IsIdentifierContinuation(int ch);

    class Lexer
    {
    public:
        Lexer(const std::string& src)
            : m_Source(src), m_SourceView(m_Source) { }
        Lexer(std::string&& src)
            : m_Source(src), m_SourceView(m_Source) { }

        const Token& NextToken() { m_Token = ParseNextToken(); return m_Token; };
        const Token& CurrentToken() const { return m_Token; };
        bool IsEndOfFile() const { return m_SourceView.length() == 0; }
    private:
        Token ParseNextToken();
        size_t ParseIdentifier(std::string& ident);
        size_t ParseIntegerLiteral(int64_t& val);
        size_t ParseDoubleLiteral(double& val);
        size_t SkipWhitespaces();
        size_t SkipComments();
    private:
        const std::string m_Source;
        std::string_view m_SourceView;
        Token m_Token = Token(TokenType::None);
    };
}


#endif // _PULSAR_LEXER_H
