#ifndef _PULSARTOOLS_VIEWS_H
#define _PULSARTOOLS_VIEWS_H

#include <string>

#include <fmt/color.h>

#include "pulsar/lexer.h"
#include "pulsar/parser.h"
#include "pulsar/runtime.h"

#include "pulsar/sourceviewer.h"

namespace PulsarTools
{
    using PositionEncoding = Pulsar::SourceViewer::PositionEncoding;
    struct PositionSettings
    {
        PositionEncoding Encoding;
        size_t LineIndexedFrom;
        size_t CharIndexedFrom;
    };

    constexpr PositionSettings PositionSettings_Default{
        .Encoding        = PositionEncoding::UTF32,
        .LineIndexedFrom = 1,
        .CharIndexedFrom = 1,
    };

    using TokenViewRange = Pulsar::SourceViewer::Range;
    constexpr TokenViewRange TokenViewRange_Default = {20,20};

    struct TokenView
    {
        std::string Contents;
        size_t WidthToTokenStart;
        size_t TokenWidth;
    };

    struct MessageReportKind
    {
        const char* Name;
        fmt::color Color;
    };

    constexpr auto MessageReportKind_Error   = MessageReportKind{ "Error",   fmt::color::red    };
    constexpr auto MessageReportKind_Warning = MessageReportKind{ "Warning", fmt::color::orange };

    TokenView CreateTokenView(
            const Pulsar::String& source,
            const Pulsar::Token& token,
            TokenViewRange viewRange = TokenViewRange_Default);

    std::string CreateSourceMessageReport(
            MessageReportKind reportKind,
            const Pulsar::String* source, const Pulsar::String* filepath,
            const Pulsar::Token& token, const Pulsar::String& message,
            PositionSettings outPositionSettings=PositionSettings_Default,
            bool enableColors=true,
            TokenViewRange viewRange=TokenViewRange_Default);

    std::string CreateParserMessageReport(
            const Pulsar::Parser& parser,
            MessageReportKind reportKind,
            const Pulsar::Parser::Message& message,
            PositionSettings outPositionSettings=PositionSettings_Default,
            bool enableColors=true,
            TokenViewRange viewRange=TokenViewRange_Default);

    std::string CreateRuntimeErrorMessageReport(
            const Pulsar::ExecutionContext& context,
            size_t stackTraceDepth=10,
            PositionSettings outPositionSettings=PositionSettings_Default,
            bool enableColors=true,
            TokenViewRange viewRange=TokenViewRange_Default);
}

#endif // _PULSARTOOLS_VIEWS_H
