#include "pulsar/lexer.h"

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
            token = ParseHexIntegerLiteral();
            if (token.Type != TokenType::None)
                return token;
            token = ParseOctIntegerLiteral();
            if (token.Type != TokenType::None)
                return token;
            token = ParseBinIntegerLiteral();
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
                    auto kwNameTypePair = Keywords.Find(token.StringVal);
                    if (!kwNameTypePair)
                        return token;
                    token.Type = *kwNameTypePair.Value;
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
        case '&':
            return TrimToToken(1, TokenType::BitAnd);
        case '|':
            return TrimToToken(1, TokenType::BitOr);
        case '~':
            return TrimToToken(1, TokenType::BitNot);
        case '^':
            return TrimToToken(1, TokenType::BitXor);
        case '=':
            return TrimToToken(1, TokenType::Equals);
        case '<':
            if (m_SourceView.Length() > 1) {
                if (m_SourceView[1] == '=')
                    return TrimToToken(2, TokenType::LessOrEqual);
                else if (m_SourceView[1] == '&')
                    return TrimToToken(2, TokenType::PushReference);
                else if (m_SourceView[1] == '<')
                    return TrimToToken(2, TokenType::BitShiftLeft);
                else if (m_SourceView[1] == '-') {
                    if (m_SourceView.Length() > 2
                        && m_SourceView[2] == '>')
                        return TrimToToken(3, TokenType::BothArrows);
                    return TrimToToken(2, TokenType::LeftArrow);
                }
            }
            return TrimToToken(1, TokenType::Less);
        case '>':
            if (m_SourceView.Length() > 1) {
                if (m_SourceView[1] == '=')
                    return TrimToToken(2, TokenType::MoreOrEqual);
                else if (m_SourceView[1] == '>')
                    return TrimToToken(2, TokenType::BitShiftRight);
            }
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

    auto cdNameValPair = CompilerDirectives.Find(token.StringVal);
    if (cdNameValPair)
        token.IntegerVal = *cdNameValPair.Value;
    return token;
}

Pulsar::Token Pulsar::Lexer::ParseIntegerLiteral()
{
    StringView view(m_SourceView);
    bool negative = view[0] == '-';
    if (negative || view[0] == '+')
        view.RemovePrefix(1);
    if (view.Empty() || !IsDigit(view[0]))
        return CreateNoneToken();
    int64_t val = 0;
    while (!view.Empty()) {
        if (IsIdentifierStart(view[0])) {
            return CreateNoneToken();
        } else if (view[0] == '.') {
            if (view.Length() < 2 || !IsDigit(view[1]))
                break;
            return CreateNoneToken();
        } else if (!IsDigit(view[0]))
            break;
        val *= 10;
        val += view[0] - '0';
        view.RemovePrefix(1);
    }
    if (negative)
        val *= -1;
    return TrimToToken(view.GetStart()-m_SourceView.GetStart(),
        TokenType::IntegerLiteral, val);
}

Pulsar::Token Pulsar::Lexer::ParseHexIntegerLiteral()
{
    StringView view(m_SourceView);
    if (view.Length() < 3) // 0xX
        return CreateNoneToken();
    else if (view[0] != '0' || view[1] != 'x' || !IsHexDigit(view[2]))
        return CreateNoneToken();
    view.RemovePrefix(2);
    int64_t val = 0;
    while (!view.Empty()) {
        char digit = ToLowerCase(view[0]);
        if (!IsHexDigit(digit)) {
            if (IsIdentifierStart(digit))
                return CreateNoneToken();
            break;
        }
        val *= 16;
        if (digit >= 'a')
            val += digit - 'a' + 10;
        else val += digit - '0';
        view.RemovePrefix(1);
    }
    return TrimToToken(view.GetStart()-m_SourceView.GetStart(),
        TokenType::IntegerLiteral, val);
}

Pulsar::Token Pulsar::Lexer::ParseOctIntegerLiteral()
{
    StringView view(m_SourceView);
    if (view.Length() < 3) // 0oX
        return CreateNoneToken();
    else if (view[0] != '0' || view[1] != 'o' || !IsOctDigit(view[2]))
        return CreateNoneToken();
    view.RemovePrefix(2);
    int64_t val = 0;
    while (!view.Empty()) {
        char digit = view[0];
        if (IsIdentifierStart(digit))
            return CreateNoneToken();
        else if (!IsOctDigit(digit))
            break;
        val *= 8;
        val += digit - '0';
        view.RemovePrefix(1);
    }
    return TrimToToken(view.GetStart()-m_SourceView.GetStart(),
        TokenType::IntegerLiteral, val);
}

Pulsar::Token Pulsar::Lexer::ParseBinIntegerLiteral()
{
    StringView view(m_SourceView);
    if (view.Length() < 3) // 0bX
        return CreateNoneToken();
    else if (view[0] != '0' || view[1] != 'b' || !IsBinDigit(view[2]))
        return CreateNoneToken();
    view.RemovePrefix(2);
    int64_t val = 0;
    while (!view.Empty()) {
        char digit = view[0];
        if (IsIdentifierStart(digit))
            return CreateNoneToken();
        else if (!IsBinDigit(digit))
            break;
        val *= 2;
        val += digit - '0';
        view.RemovePrefix(1);
    }
    return TrimToToken(view.GetStart()-m_SourceView.GetStart(),
        TokenType::IntegerLiteral, val);
}

Pulsar::Token Pulsar::Lexer::ParseDoubleLiteral()
{
    StringView view(m_SourceView);
    double exp = view[0] == '-' ? -1 : 1;
    if (exp < 0 || view[0] == '+')
        view.RemovePrefix(1);
    if (view.Empty() || !IsDigit(view[0]))
        return CreateNoneToken();
    bool decimal = false;
    double val = 0.0;
    while (!view.Empty()) {
        if (IsIdentifierStart(view[0])) {
            return CreateNoneToken();
        } else if (view[0] == '.') {
            if (decimal)
                return CreateNoneToken();
            decimal = true;
            view.RemovePrefix(1);
            if (view.Empty() || !IsDigit(view[0]))
                return CreateNoneToken();
        } else if (!IsDigit(view[0]))
            break;
        
        if (decimal)
            exp /= 10;
        val *= 10;
        val += view[0] - '0';
        view.RemovePrefix(1);
    }

    val *= exp;
    return TrimToToken(view.GetStart()-m_SourceView.GetStart(),
        TokenType::DoubleLiteral, std::move(val));
}

Pulsar::Token Pulsar::Lexer::ParseStringLiteral()
{
    StringView view(m_SourceView);
    if (view.Empty() || view[0] != '"')
        return CreateNoneToken();
    view.RemovePrefix(1);

    String val = "";
    while (!view.Empty()) {
        if (IsControlCharacter(view[0]))
            return CreateNoneToken();
        else if (view[0] == '"')
            break;
        else if (view[0] == '\\') {
            view.RemovePrefix(1);
            if (view.Empty())
                return CreateNoneToken();
            char ch = view[0];
            if (IsControlCharacter(ch))
                return CreateNoneToken();
            view.RemovePrefix(1);
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
                if (view.Length() < digits)
                    digits = view.Length();
                uint8_t code = 0;
                for (size_t i = 0; i < digits; i++) {
                    char digit = ToLowerCase(view[i]);
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
                view.RemovePrefix(digits);
                if (!view.Empty() && view[0] == ';')
                    view.RemovePrefix(1);
            } break;
            default:
                val += ch;
            }
        } else {
            val += view[0];
            view.RemovePrefix(1);
        }
    }
    if (view.Empty() || view[0] != '"')
        return CreateNoneToken();
    view.RemovePrefix(1);
    
    return TrimToToken(view.GetStart()-m_SourceView.GetStart(),
        TokenType::StringLiteral, std::move(val));
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
    } else if (m_SourceView.Length() < 2 || !(
        m_SourceView[0] == '/' && (m_SourceView[1] == '/' || m_SourceView[1] == '*')
    )) return 0;
    
    StringView commentView = m_SourceView;
    commentView.RemovePrefix(1); // The '/' char

    if (commentView[0] == '/') {
        while (commentView.Length() > 0 && commentView[0] != '\n')
            commentView.RemovePrefix(1);
        commentView.RemovePrefix(1); // Remove new line char
        m_Line++;
        m_LineStartIdx = commentView.GetStart();
    } else if (commentView[0] == '*') {
        while (commentView.Length() > 0) {
            if (commentView[0] == '\n') {
                m_Line++;
                m_LineStartIdx = commentView.GetStart()+1;
            } else if (commentView.Length() >= 2 &&
                commentView[0] == '*' && commentView[1] == '/') {
                commentView.RemovePrefix(2);
                break;
            }
            commentView.RemovePrefix(1);
        }
    }
    
    size_t count = commentView.GetStart() - m_SourceView.GetStart();
    m_SourceView = commentView;
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
    case TokenType::BitAnd:
        return "BitAnd";
    case TokenType::BitOr:
        return "BitOr";
    case TokenType::BitNot:
        return "BitNot";
    case TokenType::BitXor:
        return "BitXor";
    case TokenType::BitShiftLeft:
        return "BitShiftLeft";
    case TokenType::BitShiftRight:
        return "BitShiftRight";
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
    case TokenType::KW_Not:
        return "KW_Not";
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
