#ifndef _PULSARTOOLS_VIEWS_H
#define _PULSARTOOLS_VIEWS_H

#include <string>

#include <fmt/color.h>

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

    struct MessageReportKind
    {
        const char* Name;
        fmt::color Color;
    };

    constexpr auto MessageReportKind_Error   = MessageReportKind{ "Error",   fmt::color::red    };
    constexpr auto MessageReportKind_Warning = MessageReportKind{ "Warning", fmt::color::orange };

    TokenViewLine CreateTokenView(const Pulsar::String& source, const Pulsar::Token& token, TokenViewRange viewRange = TokenViewRange_Default);
    // TODO: Add option to disable colors
    std::string CreateSourceMessageReport(
            MessageReportKind reportKind,
            const Pulsar::String* source, const Pulsar::String* filepath,
            const Pulsar::Token& token, const Pulsar::String& message,
            bool enableColors=true,
            TokenViewRange viewRange=TokenViewRange_Default);

    std::string CreateParserMessageReport(
            const Pulsar::Parser& parser,
            MessageReportKind reportKind,
            const Pulsar::Parser::Message& message,
            bool enableColors=true,
            TokenViewRange viewRange=TokenViewRange_Default);

    std::string CreateRuntimeErrorMessageReport(
            const Pulsar::ExecutionContext& context,
            size_t stackTraceDepth=10,
            bool enableColors=true,
            TokenViewRange viewRange=TokenViewRange_Default);
}

#endif // _PULSARTOOLS_VIEWS_H
