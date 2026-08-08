#ifndef _PULSAR_LEXER_H
#define _PULSAR_LEXER_H

#include "pulsar/core.h"

#include "pulsar/utf8.h"
#include "pulsar/lexer/token.h"
#include "pulsar/lexer/utils.h"
#include "pulsar/structures/hashmap.h"
#include "pulsar/structures/stringview.h"

namespace Pulsar
{
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

    class LexerDecoder
    {
    public:
        using Decoder   = UTF8::Decoder;
        using Codepoint = UTF8::Codepoint;

    public:
        LexerDecoder(StringView src)
            : m_Decoder(src) {}

        LexerDecoder(const LexerDecoder&) = default;

        ~LexerDecoder() = default;

        LexerDecoder& operator=(const LexerDecoder&) = default;

        operator bool() const { return m_Decoder; }

        bool HasData() const           { return m_Decoder.HasData(); }
        bool IsInvalidEncoding() const { return m_Decoder.IsInvalidEncoding(); }

        size_t GetDecodedBytes() const      { return m_Decoder.GetDecodedBytes(); }
        size_t GetDecodedCodepoints() const { return m_Decoder.GetDecodedCodepoints(); }

        StringView Data() const { return m_Decoder.Data(); }

        // Normalizes line endings to '\n'
        Codepoint Next();
        Codepoint Peek() { return m_Decoder.Peek(); }
        size_t Skip();

        Codepoint Peek() const                 { return m_Decoder.Peek(); }
        Codepoint Peek(size_t lookAhead) const { return m_Decoder.Peek(lookAhead); }

        void SkipUntilNewline();
        size_t GetLine() const               { return m_Line; }
        size_t GetLineStartCodepoint() const { return m_LineStartCodepoint; }

    private:
        Decoder m_Decoder;

        size_t m_Line = 0;
        size_t m_LineStartCodepoint = 0;
    };

    class Lexer
    {
    public:
        using Decoder   = LexerDecoder;
        using Codepoint = LexerDecoder::Codepoint;

    public:
        Lexer(const String& src, bool emitComments=false)
            : m_Decoder(src), m_EmitComments(emitComments)
        {}

        Lexer(const Lexer& other) = default;

        ~Lexer() = default;

        Lexer& operator=(const Lexer&) = default;

        // Skips a sha-bang at the current position
        // Use this after creating a Lexer to ignore it
        bool SkipShaBang(Token* token=nullptr);

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

        bool SkipWhiteSpaces();
        bool SkipComments(Token* token);

        template<typename ...Args>
        Token PullToken(Decoder decoder, Args&& ...args)
        {
            // size_t tokenBytes = decoder.GetDecodedBytes() - m_Decoder.GetDecodedBytes();
            size_t charSpan = decoder.GetDecodedCodepoints() - m_Decoder.GetDecodedCodepoints();

            Token token(std::forward<Args>(args)...);
            token.SourcePos = {
                .Line = m_Decoder.GetLine(),
                .Char = m_Decoder.GetDecodedCodepoints() - m_Decoder.GetLineStartCodepoint(),
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
        bool m_EmitComments;
    };
}


#endif // _PULSAR_LEXER_H
