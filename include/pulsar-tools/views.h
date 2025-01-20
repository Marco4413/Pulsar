#ifndef _PULSARTOOLS_VIEWS_H
#define _PULSARTOOLS_VIEWS_H

#include <string>

#include "pulsar/lexer.h"
#include "pulsar/parser.h"
#include "pulsar/runtime.h"

namespace PulsarTools
{
    struct TokenViewRange { size_t Before; size_t After; };
    constexpr TokenViewRange TokenViewRange_Default{20, 20};

    struct TokenViewLine
    {
        std::string Contents;
        size_t TokenStart;
    };

    TokenViewLine CreateTokenView(const Pulsar::String& source, const Pulsar::Token& token, TokenViewRange viewRange = TokenViewRange_Default);
    // TODO: Add option to disable colors
    std::string CreateSourceErrorMessage(
            const Pulsar::String* source, const Pulsar::String* filepath,
            const Pulsar::Token& token, const Pulsar::String& message,
            TokenViewRange viewRange = TokenViewRange_Default);

    std::string CreateParserErrorMessage(const Pulsar::Parser& parser, TokenViewRange viewRange = TokenViewRange_Default);
    std::string CreateRuntimeErrorMessage(const Pulsar::ExecutionContext& context, TokenViewRange viewRange = TokenViewRange_Default);
}

#endif // _PULSARTOOLS_VIEWS_H
