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
