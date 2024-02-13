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
    std::string ident;
    int64_t intLit = 0;
    double dblLit = 0;
    while (m_SourceView.length() > 0) {
        if (SkipWhitespaces() > 0)
            continue;
        else if (SkipComments() > 0)
            continue;
        else if (ParseIntegerLiteral(intLit) > 0)
            return Token(TokenType::IntegerLiteral, intLit);
        else if (ParseDoubleLiteral(dblLit) > 0)
            return Token(TokenType::DoubleLiteral, dblLit);
        else if (ParseIdentifier(ident) > 0) {
            auto it = Keywords.find(ident);
            if (it == Keywords.end())
                return Token(TokenType::Identifier, std::move(ident));
            return Token((*it).second);
        }
        
        char symbol = m_SourceView[0];
        m_SourceView.remove_prefix(1);
        switch (symbol) {
        case '*':
            return Token(TokenType::Star);
        case '(':
            return Token(TokenType::OpenParenth);
        case ')':
            return Token(TokenType::CloseParenth);
        case ':':
            return Token(TokenType::Colon);
        case '!':
            if (m_SourceView.length() > 0 && m_SourceView[0] == '=') {
                m_SourceView.remove_prefix(1);
                return Token(TokenType::NotEquals);
            }
            return Token(TokenType::Negate);
        case '.':
            return Token(TokenType::FullStop);
        case '+':
            return Token(TokenType::Plus);
        case '-':
            if (m_SourceView.length() > 0 && m_SourceView[0] == '>') {
                m_SourceView.remove_prefix(1);
                return Token(TokenType::RightArrow);
            }
            return Token(TokenType::Minus);
        case '=':
            return Token(TokenType::Equals);
        case '<':
            if (m_SourceView.length() > 0 && m_SourceView[0] == '=') {
                m_SourceView.remove_prefix(1);
                return Token(TokenType::LessOrEqual);
            }
            return Token(TokenType::Less);
        case '>':
            if (m_SourceView.length() > 0 && m_SourceView[0] == '=') {
                m_SourceView.remove_prefix(1);
                return Token(TokenType::MoreOrEqual);
            }
            return Token(TokenType::More);
        default:
            return Token(TokenType::None);
        }
    }
    return Token(TokenType::EndOfFile);
}

size_t Pulsar::Lexer::ParseIdentifier(std::string& ident)
{
    if (!IsIdentifierStart(m_SourceView[0]))
        return 0;
    size_t count = 0;
    for (; count < m_SourceView.length() && IsIdentifierContinuation(m_SourceView[count]); count++)
        ident += m_SourceView[count];
    m_SourceView.remove_prefix(count);
    return count;
}

size_t Pulsar::Lexer::ParseIntegerLiteral(int64_t& val)
{
    size_t count = 0;
    bool negative = m_SourceView[count] == '-';
    if (negative && ++count >= m_SourceView.length())
        return 0;
    else if (!std::isdigit(m_SourceView[count]))
        return 0;

    for (; count < m_SourceView.length(); count++) {
        if (IsIdentifierStart(m_SourceView[count])) {
            return 0;
        } else if (m_SourceView[count] == '.') {
            if (m_SourceView.length() <= count+1 || !std::isdigit(m_SourceView[count+1]))
                break;
            return 0;
        } else if (!std::isdigit(m_SourceView[count]))
            break;
        val *= 10;
        val += m_SourceView[count] - '0';
    }
    if (negative)
        val *= -1;
    m_SourceView.remove_prefix(count);
    return count;
}

size_t Pulsar::Lexer::ParseDoubleLiteral(double& val)
{
    size_t count = 0;
    double exp = m_SourceView[count] == '-' ? -1 : 1;
    if (exp < 0 && ++count >= m_SourceView.length())
        return 0;
    else if (!std::isdigit(m_SourceView[count]))
        return 0;

    bool decimal = false;
    for (; count < m_SourceView.length(); count++) {
        if (IsIdentifierStart(m_SourceView[count]))
            return 0;
        else if (m_SourceView[count] == '.') {
            if (decimal)
                return 0;
            decimal = true;
            count++;
            if (count >= m_SourceView.length() || !std::isdigit(m_SourceView[count]))
                return 0;
        } if (!std::isdigit(m_SourceView[count]))
            break;
        
        if (decimal)
            exp /= 10;
        val *= 10;
        val += m_SourceView[count] - '0';
    }
    val *= exp;
    m_SourceView.remove_prefix(count);
    return count;
}

size_t Pulsar::Lexer::SkipWhitespaces()
{
    size_t count = 0;
    for (; count < m_SourceView.length() && std::isspace(m_SourceView[count]); count++);
    m_SourceView.remove_prefix(count);
    return count;
}

size_t Pulsar::Lexer::SkipComments()
{
    if (m_SourceView.length() < 2)
        return 0;
    else if (m_SourceView[0] != '/' || m_SourceView[1] != '/')
        return 0;
    size_t count = 0;
    for (; count < m_SourceView.length() && m_SourceView[count] != '\n'; count++);
    m_SourceView.remove_prefix(count);
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
    case TokenType::KW_If:
        return "KW_If";
    case TokenType::KW_Else:
        return "KW_Else";
    case TokenType::KW_End:
        return "KW_End";
    case TokenType::EndOfFile:
        return "EndOfFile";
    }
    return "Unknown";
}
