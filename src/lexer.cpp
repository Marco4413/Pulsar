#include "pulsar/lexer.h"

#include <cctype>

bool Pulsar::IsIdentifierStart(int ch)
{
    return std::isalpha(ch) || ch == '_';
}

bool Pulsar::IsIdentifierContinuation(int ch)
{
    return (
        IsIdentifierStart(ch) || std::isdigit(ch)
            || (ch >= 60 && ch <= 63) // < = > ?
            || ch == '+' || ch == '-'
            || ch == '*' || ch == '/'
            || ch == '!'
    );
}

Pulsar::Token Pulsar::Lexer::ParseNextToken()
{
    while (m_SourceView.Length() > 0) {
        if (SkipWhitespaces() > 0)
            continue;
        else if (SkipComments() > 0)
            continue;
        
        { // Parse non-symbols
            Pulsar::Token token = ParseIntegerLiteral();
            if (token.Type != TokenType::None)
                return token;
            token = ParseDoubleLiteral();
            if (token.Type != TokenType::None)
                return token;
            token = ParseIdentifier();
            if (token.Type != TokenType::None) {
                auto it = Keywords.find(token.StringVal);
                if (it == Keywords.end())
                    return token;
                token.Type = (*it).second;
                return token;
            }
        }
        
        char symbol = m_SourceView[0];
        switch (symbol) {
        case '*':
            return TrimToToken(1, TokenType::Star);
        case '(':
            return TrimToToken(1, TokenType::OpenParenth);
        case ')':
            return TrimToToken(1, TokenType::CloseParenth);
        case ':':
            return TrimToToken(1, TokenType::Colon);
        case '!':
            if (m_SourceView.Length() > 1 && m_SourceView[1] == '=')
                return TrimToToken(2, TokenType::NotEquals);
            return TrimToToken(1, TokenType::Negate);
        case '.':
            return TrimToToken(1, TokenType::FullStop);
        case '+':
            return TrimToToken(1, TokenType::Plus);
        case '-':
            if (m_SourceView.Length() > 1 && m_SourceView[1] == '>')
                return TrimToToken(2, TokenType::RightArrow);
            return TrimToToken(1, TokenType::Minus);
        case '=':
            return TrimToToken(1, TokenType::Equals);
        case '<':
            if (m_SourceView.Length() > 1) {
                if (m_SourceView[1] == '=')
                    return TrimToToken(2, TokenType::LessOrEqual);
                else if (m_SourceView[1] == '&')
                    return TrimToToken(2, TokenType::PushReference);
            }
            return TrimToToken(1, TokenType::Less);
        case '>':
            if (m_SourceView.Length() > 1 && m_SourceView[1] == '=')
                return TrimToToken(2, TokenType::MoreOrEqual);
            return TrimToToken(1, TokenType::More);
        default:
            return TrimToToken(1, TokenType::None);
        }
    }
    return TrimToToken(0, TokenType::EndOfFile);
}

Pulsar::Token Pulsar::Lexer::ParseIdentifier()
{
    if (!IsIdentifierStart(m_SourceView[0]))
        return TrimToToken(0, TokenType::None);
    size_t count = 0;
    for (; count < m_SourceView.Length() && IsIdentifierContinuation(m_SourceView[count]); count++);
    return TrimToToken(count, TokenType::Identifier, m_SourceView.GetPrefix(count));
}

Pulsar::Token Pulsar::Lexer::ParseIntegerLiteral()
{
    size_t count = 0;
    bool negative = m_SourceView[count] == '-';
    if (negative && ++count >= m_SourceView.Length())
        return CreateNoneToken();
    else if (!std::isdigit(m_SourceView[count]))
        return CreateNoneToken();
    int64_t val = 0;
    for (; count < m_SourceView.Length(); count++) {
        if (IsIdentifierStart(m_SourceView[count])) {
            return CreateNoneToken();
        } else if (m_SourceView[count] == '.') {
            if (m_SourceView.Length() <= count+1 || !std::isdigit(m_SourceView[count+1]))
                break;
            return CreateNoneToken();
        } else if (!std::isdigit(m_SourceView[count]))
            break;
        val *= 10;
        val += m_SourceView[count] - '0';
    }
    if (negative)
        val *= -1;
    return TrimToToken(count, TokenType::IntegerLiteral, val);
}

Pulsar::Token Pulsar::Lexer::ParseDoubleLiteral()
{
    size_t count = 0;
    double exp = m_SourceView[count] == '-' ? -1 : 1;
    if (exp < 0 && ++count >= m_SourceView.Length())
        return CreateNoneToken();
    else if (!std::isdigit(m_SourceView[count]))
        return CreateNoneToken();
    bool decimal = false;
    double val = 0.0;
    for (; count < m_SourceView.Length(); count++) {
        if (IsIdentifierStart(m_SourceView[count]))
            return CreateNoneToken();
        else if (m_SourceView[count] == '.') {
            if (decimal)
                return CreateNoneToken();
            decimal = true;
            count++;
            if (count >= m_SourceView.Length() || !std::isdigit(m_SourceView[count]))
                return CreateNoneToken();
        } if (!std::isdigit(m_SourceView[count]))
            break;
        
        if (decimal)
            exp /= 10;
        val *= 10;
        val += m_SourceView[count] - '0';
    }
    val *= exp;
    return TrimToToken(count, TokenType::DoubleLiteral, val);
}

size_t Pulsar::Lexer::SkipWhitespaces()
{
    size_t count = 0;
    for (; count < m_SourceView.Length() && std::isspace(m_SourceView[count]); count++) {
        if (m_SourceView[count] == '\n') {
            m_Line++;
            m_LineStartIdx = m_SourceView.GetStart() + count;
        }
    }
    m_SourceView.RemovePrefix(count);
    return count;
}

size_t Pulsar::Lexer::SkipComments()
{
    if (m_SourceView[0] == ';') {
        m_SourceView.RemovePrefix(1);
        return 1;
    } else if (
        m_SourceView.Length() < 2
        || m_SourceView[0] != '/' || m_SourceView[1] != '/'
    ) return 0;
    size_t count = 0;
    for (; count < m_SourceView.Length() && m_SourceView[count] != '\n'; count++);
    m_SourceView.RemovePrefix(count);
    m_Line++;
    m_LineStartIdx = m_SourceView.GetStart();
    return count;
}

const char* Pulsar::TokenTypeToString(TokenType ttype)
{
    switch (ttype) {
    case TokenType::None:
        return "None";
    case TokenType::Identifier:
        return "Identifier";
    case TokenType::OpenParenth:
        return "OpenParenth";
    case TokenType::CloseParenth:
        return "CloseParenth";
    case TokenType::IntegerLiteral:
        return "IntegerLiteral";
    case TokenType::DoubleLiteral:
        return "DoubleLiteral";
    case TokenType::Plus:
        return "Plus";
    case TokenType::Minus:
        return "Minus";
    case TokenType::Star:
        return "Star";
    case TokenType::FullStop:
        return "FullStop";
    case TokenType::Negate:
        return "Negate";
    case TokenType::Colon:
        return "Colon";
    case TokenType::RightArrow:
        return "RightArrow";
    case TokenType::Equals:
        return "Equals";
    case TokenType::NotEquals:
        return "NotEquals";
    case TokenType::Less:
        return "Less";
    case TokenType::LessOrEqual:
        return "LessOrEqual";
    case TokenType::More:
        return "More";
    case TokenType::MoreOrEqual:
        return "MoreOrEqual";
    case TokenType::PushReference:
        return "PushReference";
    case TokenType::KW_If:
        return "KW_If";
    case TokenType::KW_Else:
        return "KW_Else";
    case TokenType::KW_End:
        return "KW_End";
    case TokenType::KW_ICall:
        return "KW_ICall";
    case TokenType::EndOfFile:
        return "EndOfFile";
    }
    return "Unknown";
}
