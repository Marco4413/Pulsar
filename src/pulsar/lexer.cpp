#include "pulsar/lexer.h"

bool Pulsar::Lexer::SkipShaBang()
{
    Decoder decoder = m_Decoder;
    
    if (decoder.Next() != '#' || decoder.Next() != '!')
        return false;

    SkipUntilNewline();
    return true;
}

Pulsar::Token Pulsar::Lexer::NextToken()
{
    while (SkipWhiteSpaces() || SkipComments());

    if (IsEndOfFile())
        return PullToken(m_Decoder, TokenType::EndOfFile);

    Decoder decoder = m_Decoder;
    Codepoint codepoint = decoder.Next();

    // Parse non-symbols
    // Try to predict what's next to prevent unnecessary function calls
    // This is better than what we had before, but needs to be kept up to date
    if (Unicode::IsDecimalDigit(codepoint) || codepoint == '+' || codepoint == '-') {
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
        switch (codepoint) {
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
        case '@': {
            Pulsar::Token token = ParseLabel();
            if (token.Type != TokenType::None)
                return token;
        } break;
        default: {
            Pulsar::Token token = ParseIdentifier();
            if (token.Type != TokenType::None) {
                auto kwNameTypePair = Keywords.Find(token.StringVal);
                if (!kwNameTypePair)
                    return token;
                token.Type = kwNameTypePair->Value();
                return token;
            }
        }
        }
    }

    switch (codepoint) {
    case '*': return PullToken(decoder, TokenType::Star);
    case '(': return PullToken(decoder, TokenType::OpenParenth);
    case ')': return PullToken(decoder, TokenType::CloseParenth);
    case '[': return PullToken(decoder, TokenType::OpenBracket);
    case ']': return PullToken(decoder, TokenType::CloseBracket);
    case ',': return PullToken(decoder, TokenType::Comma);
    case ':': return PullToken(decoder, TokenType::Colon);
    case '!':
        if (decoder.Peek() == '=') {
            decoder.Skip();
            return PullToken(decoder, TokenType::NotEquals);
        }
        return PullToken(decoder, TokenType::Negate);
    case '.': return PullToken(decoder, TokenType::FullStop);
    case '+': return PullToken(decoder, TokenType::Plus);
    case '-':
        if (decoder.Peek() == '>') {
            decoder.Skip();
            return PullToken(decoder, TokenType::RightArrow);
        }
        return PullToken(decoder, TokenType::Minus);
    case '/': return PullToken(decoder, TokenType::Slash);
    case '%': return PullToken(decoder, TokenType::Modulus);
    case '&': return PullToken(decoder, TokenType::BitAnd);
    case '|': return PullToken(decoder, TokenType::BitOr);
    case '~': return PullToken(decoder, TokenType::BitNot);
    case '^': return PullToken(decoder, TokenType::BitXor);
    case '=': return PullToken(decoder, TokenType::Equals);
    case '<':
        if (decoder.HasData()) {
            switch (decoder.Peek()) {
            case '=': decoder.Skip(); return PullToken(decoder, TokenType::LessOrEqual);
            case '&': decoder.Skip(); return PullToken(decoder, TokenType::PushReference);
            case '<': decoder.Skip(); return PullToken(decoder, TokenType::BitShiftLeft);
            case '-':
                decoder.Skip();
                if (decoder.Peek() == '>') {
                    decoder.Skip();
                    return PullToken(decoder, TokenType::BothArrows);
                }
                return PullToken(decoder, TokenType::LeftArrow);
            default: break;
            }
        }
        return PullToken(decoder, TokenType::Less);
    case '>':
        if (decoder.HasData()) {
            switch (decoder.Peek()) {
            case '=': decoder.Skip(); return PullToken(decoder, TokenType::MoreOrEqual);
            case '>': decoder.Skip(); return PullToken(decoder, TokenType::BitShiftRight);
            default: break;
            }
        }
        return PullToken(decoder, TokenType::More);
    default:
        return PullToken(decoder, TokenType::None);
    }
}

Pulsar::Token Pulsar::Lexer::ParseIdentifier()
{
    Decoder decoder = m_Decoder;
    if (!IsIdentifierStart(decoder.Next()))
        return CreateNoneToken();

    while (decoder && IsIdentifierContinuation(decoder.Peek()))
        decoder.Skip();

    if (decoder.IsInvalidEncoding())
        return CreateNoneToken();

    size_t idBytes = decoder.GetDecodedBytes() - m_Decoder.GetDecodedBytes();
    String id = m_Decoder.Data().PrefixToString(idBytes);

    return PullToken(decoder, TokenType::Identifier, std::move(id));
}

Pulsar::Token Pulsar::Lexer::ParseLabel()
{
    Decoder decoder = m_Decoder;
    if (decoder.Next() != '@' || !IsIdentifierStart(decoder.Next()))
        return CreateNoneToken();

    while (decoder && IsIdentifierContinuation(decoder.Peek()))
        decoder.Skip();

    if (decoder.IsInvalidEncoding())
        return CreateNoneToken();

    size_t idBytes = decoder.GetDecodedBytes() - m_Decoder.GetDecodedBytes() - 1;
    StringView idView = m_Decoder.Data();
    idView.RemovePrefix(1); // '@'
    idView.RemoveSuffix(idView.Length()-idBytes);

    return PullToken(decoder, TokenType::Label, idView.PrefixToString(idView.Length()));
}

Pulsar::Token Pulsar::Lexer::ParseCompilerDirective()
{
    Decoder decoder = m_Decoder;
    if (decoder.Next() != '#' || !IsIdentifierStart(decoder.Next()))
        return CreateNoneToken();

    while (decoder && IsIdentifierContinuation(decoder.Peek()))
        decoder.Skip();

    if (decoder.IsInvalidEncoding())
        return CreateNoneToken();

    size_t idBytes = decoder.GetDecodedBytes() - m_Decoder.GetDecodedBytes() - 1;
    StringView idView = m_Decoder.Data();
    idView.RemovePrefix(1); // '#'
    idView.RemoveSuffix(idView.Length()-idBytes);

    Token token = PullToken(decoder, TokenType::CompilerDirective, idView.PrefixToString(idView.Length()));
    token.IntegerVal = TOKEN_CD_GENERIC;

    auto cdNameValPair = CompilerDirectives.Find(token.StringVal);
    if (cdNameValPair) {
        token.IntegerVal = cdNameValPair->Value();
    }

    return token;
}

Pulsar::Token Pulsar::Lexer::ParseIntegerLiteral()
{
    Decoder decoder = m_Decoder;

    Codepoint signCode = decoder.Peek();
    bool isNegative = signCode == '-';
    if (isNegative || signCode == '+')
        decoder.Skip();

    if (!Unicode::IsDecimalDigit(decoder.Peek()))
        return CreateNoneToken();

    int64_t val = 0;
    while (decoder) {
        Codepoint digit = decoder.Peek();
        if (IsIdentifierStart(digit)) {
            return CreateNoneToken();
        } else if (digit == '.') {
            if (!Unicode::IsDecimalDigit(decoder.Peek(2)))
                break;
            return CreateNoneToken();
        } else if (!Unicode::IsDecimalDigit(digit))
            break;

        val *= 10;
        val += digit - '0';
        decoder.Skip();
    }

    if (isNegative)
        val *= -1;

    return PullToken(decoder, TokenType::IntegerLiteral, val);
}

Pulsar::Token Pulsar::Lexer::ParseHexIntegerLiteral()
{
    Decoder decoder = m_Decoder;

    if (decoder.Next() != '0' || decoder.Next() != 'x' || !Unicode::IsHexDigit(decoder.Peek()))
        return CreateNoneToken();

    int64_t val = 0;
    while (decoder) {
        Codepoint digit = Unicode::ToLowerCase(decoder.Peek());
        if (IsIdentifierStart(digit) && !Unicode::IsHexDigit(digit))
            return CreateNoneToken();
        else if (!Unicode::IsHexDigit(digit))
            break;

        val *= 16;
        if (digit >= 'a')
            val += digit - 'a' + 10;
        else val += digit - '0';
        decoder.Skip();
    }

    return PullToken(decoder, TokenType::IntegerLiteral, val);
}

Pulsar::Token Pulsar::Lexer::ParseOctIntegerLiteral()
{
    Decoder decoder = m_Decoder;

    if (decoder.Next() != '0' || decoder.Next() != 'o' || !Unicode::IsOctalDigit(decoder.Peek()))
        return CreateNoneToken();

    int64_t val = 0;
    while (decoder) {
        Codepoint digit = decoder.Peek();
        if (IsIdentifierStart(digit))
            return CreateNoneToken();
        else if (!Unicode::IsOctalDigit(digit))
            break;

        val *= 8;
        val += digit - '0';
        decoder.Skip();
    }

    return PullToken(decoder, TokenType::IntegerLiteral, val);
}

Pulsar::Token Pulsar::Lexer::ParseBinIntegerLiteral()
{
    Decoder decoder = m_Decoder;

    if (decoder.Next() != '0' || decoder.Next() != 'b' || !Unicode::IsBinaryDigit(decoder.Peek()))
        return CreateNoneToken();

    int64_t val = 0;
    while (decoder) {
        Codepoint digit = decoder.Peek();
        if (IsIdentifierStart(digit))
            return CreateNoneToken();
        else if (!Unicode::IsBinaryDigit(digit))
            break;

        val *= 2;
        val += digit - '0';
        decoder.Skip();
    }

    return PullToken(decoder, TokenType::IntegerLiteral, val);
}

Pulsar::Token Pulsar::Lexer::ParseDoubleLiteral()
{
    Decoder decoder = m_Decoder;

    Codepoint signCode = decoder.Peek();
    bool isNegative = signCode == '-';
    if (isNegative || signCode == '+')
        decoder.Skip();

    if (!Unicode::IsDecimalDigit(decoder.Peek()))
        return CreateNoneToken();

    bool isFloating = false;
    double fexp = 1;
    double i = 0.0, f = 0.0;
    while (decoder) {
        Codepoint digit = decoder.Peek();
        if (IsIdentifierStart(digit)) {
            return CreateNoneToken();
        } else if (digit == '.') {
            if (isFloating)
                return CreateNoneToken();
            isFloating = true;
            decoder.Skip();
            digit = decoder.Peek();
            if (!Unicode::IsDecimalDigit(digit))
                return CreateNoneToken();
        } else if (!Unicode::IsDecimalDigit(digit))
            break;

        if (isFloating) {
            fexp /= 10;
            f *= 10;
            f += digit - '0';
        } else {
            i *= 10;
            i += digit - '0';
        }

        decoder.Skip();
    }

    double val = i + f*fexp;
    if (isNegative) val = -val;
    return PullToken(decoder, TokenType::DoubleLiteral, val);
}

Pulsar::Token Pulsar::Lexer::ParseStringLiteral()
{
    Decoder decoder = m_Decoder;
    if (decoder.Next() != '"')
        return CreateNoneToken();

    String val = "";
    while (decoder) {
        Codepoint ch = decoder.Peek();
        if (Unicode::IsControl(ch))
            return CreateNoneToken();
        else if (ch == '"')
            break;
        else if (ch == '\\') {
            decoder.Skip();
            ch = decoder.Next();
            if (Unicode::IsControl(ch))
                return CreateNoneToken();
            switch (ch) {
            case 't': val += '\t'; break;
            case 'r': val += '\r'; break;
            case 'n': val += '\n'; break;
            case 'x': {
                size_t digits = 2;
                uint8_t code = 0;
                for (size_t i = 0; i < digits; i++) {
                    Codepoint digit = Unicode::ToLowerCase(decoder.Peek());
                    if (digit >= 'a' && digit <= 'f') {
                        code = (code << 4) + (uint8_t)(digit-'a'+10);
                        decoder.Skip();
                    } else if (digit >= '0' && digit <= '9') {
                        code = (code << 4) + (uint8_t)(digit-'0');
                        decoder.Skip();
                    } else {
                        digits = i;
                        break;
                    }
                }
                if (digits == 0) {
                    val += 'x';
                } else {
                    val += (char)(code);
                    if (decoder.Peek() == ';')
                        decoder.Skip();
                }
            } break;
            case 'u': {
                // See UTF8::MAX_CODEPOINT for digits
                size_t digits = 6;
                Codepoint code = 0;
                for (size_t i = 0; i < digits; i++) {
                    Codepoint digit = Unicode::ToLowerCase(decoder.Peek());
                    if (digit >= 'a' && digit <= 'f') {
                        code = (code << 4) + (uint8_t)(digit-'a'+10);
                        decoder.Skip();
                    } else if (digit >= '0' && digit <= '9') {
                        code = (code << 4) + (uint8_t)(digit-'0');
                        decoder.Skip();
                    } else {
                        digits = i;
                        break;
                    }
                }
                if (digits == 0) {
                    val += 'u';
                } else {
                    val += UTF8::Encode(code);
                    if (decoder.Peek() == ';')
                        decoder.Skip();
                }
            } break;
            default: {
                StringView data = decoder.Data();
                size_t codepointBytes = decoder.Skip();
                val += data.PrefixToString(codepointBytes);
            } break;
            }
        } else {
            StringView data = decoder.Data();
            size_t codepointBytes = decoder.Skip();
            val += data.PrefixToString(codepointBytes);
        }
    }
    if (decoder.Next() != '"')
        return CreateNoneToken();
    
    Token strToken = PullToken(decoder, TokenType::StringLiteral, std::move(val));

    // Multi-line String Literals (\ and \n)
    bool isValidMultiLine = false;
    Decoder oldDecoder = m_Decoder;
    size_t oldLine = m_Line;
    size_t oldLineStartCodepoint = m_LineStartCodepoint;

    SkipWhiteSpaces();
    if (m_Decoder.Next() == '\\') {
        bool appendNewLine = m_Decoder.Peek() == 'n';
        if (appendNewLine) m_Decoder.Skip();
        SkipWhiteSpaces();
        Token nextStrToken = ParseStringLiteral();
        if (nextStrToken.Type != TokenType::None) {
            isValidMultiLine = true;
            if (appendNewLine)
                strToken.StringVal += '\n';
            strToken.StringVal += std::move(nextStrToken.StringVal);
            if (strToken.SourcePos.Line == nextStrToken.SourcePos.Line) {
                // Same line: Extend CharSpan to include the whole String
                // "Foo" \ "Bar"
                // ^       ^
                // |       nextStrToken.Char
                // strToken.Char
                // ~~~~~~~~ = nextStrToken.Char - strToken.Char
                //         ~~~~~ = nextStrToken.CharSpan
                strToken.SourcePos.CharSpan = (
                    nextStrToken.SourcePos.Char
                    - strToken.SourcePos.Char
                    + nextStrToken.SourcePos.CharSpan);
            } else {
                // If it's not the same line take the last SourcePos
                strToken.SourcePos = nextStrToken.SourcePos;
            }
        }
    }

    // Restore Lexer Context
    // TODO: Maybe add a proper way to do this
    //   Lexer::SaveContext
    //   Lexer::RestoreContext
    //   Lexer::DropContext
    if (!isValidMultiLine) {
        m_Decoder = oldDecoder;
        m_Line = oldLine;
        m_LineStartCodepoint = oldLineStartCodepoint;
    }

    return strToken;
}

Pulsar::Token Pulsar::Lexer::ParseCharacterLiteral()
{
    Decoder decoder = m_Decoder;
    if (decoder.Next() != '\'')
        return CreateNoneToken();

    Codepoint ch = decoder.Next();
    // There must be either another ' or an escaped character
    if (!decoder)
        return CreateNoneToken();

    if (ch == '\\') {
        ch = decoder.Next();
        if (Unicode::IsControl(ch))
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
            uint8_t code = 0;
            for (size_t i = 0; i < digits; i++) {
                Codepoint digit = Unicode::ToLowerCase(decoder.Peek());
                if (digit >= 'a' && digit <= 'f') {
                    code = (code << 4) + (uint8_t)(digit-'a'+10);
                    decoder.Skip();
                } else if (digit >= '0' && digit <= '9') {
                    code = (code << 4) + (uint8_t)(digit-'0');
                    decoder.Skip();
                } else {
                    digits = i;
                    break;
                }
            }
            if (digits == 0)
                break;
            ch = (unsigned char)(code);
            if (decoder.Peek() == ';')
                decoder.Skip();
        } break;
        default:
            break;
        }
    } else if (Unicode::IsControl(ch) || ch == '\'')
        return CreateNoneToken();

    if (decoder.Next() != '\'')
        return CreateNoneToken();

    return PullToken(decoder, TokenType::IntegerLiteral, (int64_t)ch);
}

void Pulsar::Lexer::SkipUntilNewline()
{
    while (m_Decoder) {
        Codepoint ch = m_Decoder.Next();
        if (ch == '\n') {
            break;
        } else if (ch == '\r') {
            if (m_Decoder.Peek() == '\n')
                m_Decoder.Skip();
            break;
        }
    }

    ++m_Line;
    m_LineStartCodepoint = m_Decoder.GetDecodedCodepoints();
}

bool Pulsar::Lexer::SkipWhiteSpaces()
{
    bool hasSkipped = false;
    for (Codepoint ch = m_Decoder.Peek(); m_Decoder && Unicode::IsWhiteSpace(ch); ch = m_Decoder.Peek()) {
        hasSkipped = true;
        m_Decoder.Skip();
        if (ch == '\n') {
            ++m_Line;
            m_LineStartCodepoint = m_Decoder.GetDecodedCodepoints();
        } else if (ch == '\r') {
            if (m_Decoder.Peek() == '\n')
                m_Decoder.Skip();
            ++m_Line;
            m_LineStartCodepoint = m_Decoder.GetDecodedCodepoints();
        }
    }
    return hasSkipped;
}

bool Pulsar::Lexer::SkipComments()
{
    if (m_Decoder.Peek() == ';') {
        m_Decoder.Skip();
        return true;
    }

    if (m_Decoder.Peek() != '/')
        return false;

    if (m_Decoder.Peek(2) == '/') {
        SkipUntilNewline();
        return true;
    }

    if (m_Decoder.Peek(2) != '*')
        return false;

    // We must skip "/*", otherwise "/*/" would be valid.
    m_Decoder.Skip();
    m_Decoder.Skip();

    while (m_Decoder) {
        Codepoint ch = m_Decoder.Next();
        if (ch == '\n') {
            ++m_Line;
            m_LineStartCodepoint = m_Decoder.GetDecodedCodepoints();
        } else if (ch == '\r') {
            if (m_Decoder.Peek() == '\n')
                m_Decoder.Skip();
            ++m_Line;
            m_LineStartCodepoint = m_Decoder.GetDecodedCodepoints();
        } else if (ch == '*' && m_Decoder.Peek() == '/') {
            m_Decoder.Skip();
            break;
        }
    }

    return true;
}
