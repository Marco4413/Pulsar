#include "pulsar/lexer.h"

#include <cctype>

Pulsar::String Pulsar::ToStringLiteral(const String& str)
{
    String lit(1, '"');
    for (size_t i = 0; i < str.Length(); i++) {
        switch (str[i]) {
        case '"':
            lit += "\\\"";
            break;
        case '\n':
            lit += "\\n";
            break;
        case '\r':
            lit += "\\r";
            break;
        case '\t':
            lit += "\\t";
            break;
        default:
            if (IsControlCharacter(str[i])) {
                lit += "\\x";
                uint8_t digit2 = (str[i] >> 8) & 0x0F;
                if (digit2 < 0xA)
                    lit += (char)('0'+digit2);
                else lit += (char)('A'+digit2-0xA);
                uint8_t digit1 = str[i] & 0x0F;
                if (digit1 < 0xA)
                    lit += (char)('0'+digit1);
                else lit += (char)('A'+digit1-0xA);
                lit += ';';
                break;
            }
            lit += str[i];
        }
    }
    lit += '"';
    return lit;
}

Pulsar::Token Pulsar::Lexer::ParseNextToken()
{
    while (m_SourceView.Length() > 0) {
        if (SkipWhitespaces() > 0)
            continue;
        else if (SkipComments() > 0)
            continue;
        
        // Parse non-symbols
        // Try to predict what's next to prevent unnecessary function calls
        // This is better than what we had before, but needs to be kept up to date
        if (IsDigit(m_SourceView[0]) || m_SourceView[0] == '+' || m_SourceView[0] == '-') {
            Pulsar::Token token = ParseIntegerLiteral();
            if (token.Type != TokenType::None)
                return token;
            token = ParseDoubleLiteral();
            if (token.Type != TokenType::None)
                return token;
        } else {
            switch (m_SourceView[0]) {
            case '"': {
                Pulsar::Token token = ParseStringLiteral();
                if (token.Type != TokenType::None)
                    return token;
            } break;
            case '\'': {
                Pulsar::Token token = ParseCharacterLiteral();
                if (token.Type != TokenType::None)
                    return token;
            } break;
            case '#': {
                Pulsar::Token token = ParseCompilerDirective();
                if (token.Type != TokenType::None)
                    return token;
            } break;
            default: {
                Pulsar::Token token = ParseIdentifier();
                if (token.Type != TokenType::None) {
                    auto it = Keywords.find(token.StringVal);
                    if (it == Keywords.end())
                        return token;
                    token.Type = (*it).second;
                    return token;
                }
            }
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
        case '[':
            return TrimToToken(1, TokenType::OpenBracket);
        case ']':
            return TrimToToken(1, TokenType::CloseBracket);
        case ',':
            return TrimToToken(1, TokenType::Comma);
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
        case '/':
            return TrimToToken(1, TokenType::Slash);
        case '%':
            return TrimToToken(1, TokenType::Modulus);
        case '=':
            return TrimToToken(1, TokenType::Equals);
        case '<':
            if (m_SourceView.Length() > 1) {
                if (m_SourceView[1] == '=')
                    return TrimToToken(2, TokenType::LessOrEqual);
                else if (m_SourceView[1] == '&')
                    return TrimToToken(2, TokenType::PushReference);
                else if (m_SourceView[1] == '-') {
                    if (m_SourceView.Length() > 2
                        && m_SourceView[2] == '>')
                        return TrimToToken(3, TokenType::BothArrows);
                    return TrimToToken(2, TokenType::LeftArrow);
                }
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

Pulsar::Token Pulsar::Lexer::ParseCompilerDirective()
{
    if (m_SourceView.Length() < 2)
        return TrimToToken(0, TokenType::None);
    if (m_SourceView[0] != '#' || !IsIdentifierStart(m_SourceView[1]))
        return TrimToToken(0, TokenType::None);
    size_t count = 1;
    for (; count < m_SourceView.Length() && IsIdentifierContinuation(m_SourceView[count]); count++);
    
    StringView idView = m_SourceView;
    idView.RemovePrefix(1);
    Token token = TrimToToken(count, TokenType::CompilerDirective, idView.GetPrefix(count-1));
    token.IntegerVal = TOKEN_CD_GENERIC;

    const auto& it = CompilerDirectives.find(token.StringVal);
    if (it != CompilerDirectives.end())
        token.IntegerVal = (*it).second;
    return token;
}

Pulsar::Token Pulsar::Lexer::ParseIntegerLiteral()
{
    size_t count = 0;
    bool negative = m_SourceView[count] == '-';
    if (negative && ++count >= m_SourceView.Length())
        return CreateNoneToken();
    else if (!IsDigit(m_SourceView[count]))
        return CreateNoneToken();
    int64_t val = 0;
    for (; count < m_SourceView.Length(); count++) {
        if (IsIdentifierStart(m_SourceView[count])) {
            return CreateNoneToken();
        } else if (m_SourceView[count] == '.') {
            if (m_SourceView.Length() <= count+1 || !IsDigit(m_SourceView[count+1]))
                break;
            return CreateNoneToken();
        } else if (!IsDigit(m_SourceView[count]))
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
    else if (!IsDigit(m_SourceView[count]))
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
            if (count >= m_SourceView.Length() || !IsDigit(m_SourceView[count]))
                return CreateNoneToken();
        } if (!IsDigit(m_SourceView[count]))
            break;
        
        if (decimal)
            exp /= 10;
        val *= 10;
        val += m_SourceView[count] - '0';
    }
    val *= exp;
    return TrimToToken(count, TokenType::DoubleLiteral, val);
}

Pulsar::Token Pulsar::Lexer::ParseStringLiteral()
{
    if (m_SourceView[0] != '"')
        return CreateNoneToken();
    size_t count = 1;
    String val = "";
    for (; count < m_SourceView.Length(); count++) {
        if (IsControlCharacter(m_SourceView[count]))
            break;
        else if (m_SourceView[count] == '"')
            return TrimToToken(count+1, TokenType::StringLiteral, val);
        else if (m_SourceView[count] == '\\') {
            if (m_SourceView.Length()-count < 2)
                break;
            char ch = m_SourceView[++count];
            if (IsControlCharacter(ch))
                break;
            switch (ch) {
            case 't':
                val += '\t';
                break;
            case 'r':
                val += '\r';
                break;
            case 'n':
                val += '\n';
                break;
            case 'x': {
                size_t digits = 2;
                if (m_SourceView.Length() - (count+1) < digits)
                    digits = m_SourceView.Length()- (count+1);
                uint8_t code = 0;
                for (size_t i = 0; i < digits; i++) {
                    char digit = ToLowerCase(m_SourceView[count+1+i]);
                    if (digit >= 'a' && digit <= 'f') {
                        code = (code << 4) + digit-'a'+10;
                    } else if (digit >= '0' && digit <= '9') {
                        code = (code << 4) + digit-'0';
                    } else {
                        digits = i;
                        break;
                    }
                }
                if (digits == 0) {
                    val += ch;
                    break;
                }
                val += (char)(code);
                count += digits;
                if (count+1 < m_SourceView.Length() && m_SourceView[count+1] == ';')
                    count++;
            } break;
            default:
                val += ch;
            }
            continue;
        }
        val += m_SourceView[count];
    }
    return CreateNoneToken();
}

Pulsar::Token Pulsar::Lexer::ParseCharacterLiteral()
{
    // TODO: Either switch to this style of lexing or the other way around.
    if (m_SourceView.Length() < 3
        || m_SourceView[0] != '\'')
        return CreateNoneToken();
    StringView view = m_SourceView;
    view.RemovePrefix(1);
    char ch = view[0];
    view.RemovePrefix(1);
    // There must be either another ' or an escaped character
    if (view.Empty())
        return CreateNoneToken();
    else if (ch == '\\') {
        ch = view[0];
        view.RemovePrefix(1);
        if (IsControlCharacter(ch))
            return CreateNoneToken();
        switch (ch) {
        case 'n':
            ch = '\n';
            break;
        case 'r':
            ch = '\r';
            break;
        case 't':
            ch = '\t';
            break;
        case 'x': {
            size_t digits = 2;
            if (view.Length() < digits)
                digits = view.Length();
            uint8_t code = 0;
            for (size_t i = 0; i < digits; i++) {
                char digit = ToLowerCase(view[0]);
                if (digit >= 'a' && digit <= 'f') {
                    code = (code << 4) + digit-'a'+10;
                    view.RemovePrefix(1);
                } else if (digit >= '0' && digit <= '9') {
                    code = (code << 4) + digit-'0';
                    view.RemovePrefix(1);
                } else {
                    digits = i;
                    break;
                }
            }
            if (digits == 0)
                break;
            ch = (char)(code);
            if (!view.Empty() && view[0] == ';')
                view.RemovePrefix(1);
        } break;
        default:
            break;
        }
    } else if (IsControlCharacter(ch) || ch == '\'')
        return CreateNoneToken();

    if (view.Empty() || view[0] != '\'')
        return CreateNoneToken();
    view.RemovePrefix(1);
    return TrimToToken(view.GetStart()-m_SourceView.GetStart(), TokenType::IntegerLiteral, (int64_t)ch);
}

size_t Pulsar::Lexer::SkipWhitespaces()
{
    size_t count = 0;
    for (; count < m_SourceView.Length() && IsSpace(m_SourceView[count]); count++) {
        if (m_SourceView[count] == '\n') {
            m_Line++;
            m_LineStartIdx = m_SourceView.GetStart() + count+1 /* skip new line char */;
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
    count++; // Skip new line
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
    case TokenType::OpenBracket:
        return "OpenBracket";
    case TokenType::CloseBracket:
        return "CloseBracket";
    case TokenType::IntegerLiteral:
        return "IntegerLiteral";
    case TokenType::DoubleLiteral:
        return "DoubleLiteral";
    case TokenType::StringLiteral:
        return "StringLiteral";
    case TokenType::Plus:
        return "Plus";
    case TokenType::Minus:
        return "Minus";
    case TokenType::Star:
        return "Star";
    case TokenType::Slash:
        return "Slash";
    case TokenType::Modulus:
        return "Modulus";
    case TokenType::FullStop:
        return "FullStop";
    case TokenType::Negate:
        return "Negate";
    case TokenType::Colon:
        return "Colon";
    case TokenType::Comma:
        return "Comma";
    case TokenType::RightArrow:
        return "RightArrow";
    case TokenType::LeftArrow:
        return "LeftArrow";
    case TokenType::BothArrows:
        return "BothArrows";
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
    case TokenType::KW_Global:
        return "KW_Global";
    case TokenType::KW_Const:
        return "KW_Const";
    case TokenType::KW_Do:
        return "KW_Do";
    case TokenType::KW_While:
        return "KW_While";
    case TokenType::KW_Break:
        return "KW_Break";
    case TokenType::KW_Continue:
        return "KW_Continue";
    case TokenType::CompilerDirective:
        return "CompilerDirective";
    case TokenType::EndOfFile:
        return "EndOfFile";
    }
    return "Unknown";
}
