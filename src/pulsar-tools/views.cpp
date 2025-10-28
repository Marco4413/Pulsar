#include "pulsar-tools/views.h"

#include <fmt/format.h>

#include "pulsar/utf8.h"
#include "pulsar/structures/stringview.h"

#include "pulsar-tools/fmt.h"

static Pulsar::SourcePosition ConvertDefaultPositionWith(
        const Pulsar::String& source,
        Pulsar::SourcePosition position,
        PulsarTools::PositionSettings settings)
{
    if (settings.Encoding != PulsarTools::PositionEncoding::UTF32) {
        Pulsar::SourceViewer sourceViewer(source);
        sourceViewer.ConvertPositionTo(position, settings.Encoding, position);
    }
    position.Line += settings.LineIndexedFrom;
    position.Char += settings.CharIndexedFrom;
    return position;
}

PulsarTools::TokenView PulsarTools::CreateTokenView(const Pulsar::String& source, const Pulsar::Token& token, TokenViewRange viewRange)
{
    Pulsar::SourceViewer sourceViewer(source);
    auto rangeView = sourceViewer.ComputeRangeView(token.SourcePos, viewRange);

    std::string lineContents;
    if (rangeView.HasTrimmedBefore)
        lineContents += "... ";
    lineContents.append(rangeView.View.Data(), rangeView.View.Length());
    if (rangeView.HasTrimmedAfter)
        lineContents += " ...";

    return {
        .Contents          = std::move(lineContents),
        .WidthToTokenStart = rangeView.WidthToTokenStart + (rangeView.HasTrimmedBefore ? 4 : 0),
        .TokenWidth        = rangeView.TokenWidth,
    };
}

std::string PulsarTools::CreateSourceMessageReport(
        MessageReportKind reportKind,
        const Pulsar::String* source, const Pulsar::String* filepath,
        const Pulsar::Token& token, const Pulsar::String& message,
        PositionSettings outPositionSettings,
        bool enableColors, TokenViewRange viewRange)
{
    std::string result;
    if (filepath) {
        Pulsar::SourcePosition sourcePos = source
            ? ConvertDefaultPositionWith(*source, token.SourcePos, outPositionSettings)
            : token.SourcePos;

        result += fmt::format(
                "{}:{}:{}: {}: {}\n",
                *filepath, sourcePos.Line, sourcePos.Char,
                reportKind.Name, message);
    } else {
        result += fmt::format(
                "{}: {}\n",
                reportKind.Name, message);
    }
    if (source) {
        TokenView tokenView = CreateTokenView(*source, token, viewRange);
        result += tokenView.Contents;
        result += '\n';

        size_t cursorWidth = tokenView.TokenWidth > 0 ? tokenView.TokenWidth : 1;
        std::string tokenCursor = fmt::format("{0: ^{1}}^{0:~^{2}}", "", tokenView.WidthToTokenStart, cursorWidth-1);

        if (enableColors) {
            result += fmt::format(fmt::fg(reportKind.Color), "{}", tokenCursor);
        } else {
            result += tokenCursor;
        }
    } else {
        result += "No source to show.";
    }
    return result;
}

std::string PulsarTools::CreateParserMessageReport(
        const Pulsar::Parser& parser,
        MessageReportKind reportKind,
        const Pulsar::Parser::Message& message,
        PositionSettings outPositionSettings,
        bool enableColors, TokenViewRange viewRange)
{
    return CreateSourceMessageReport(
            reportKind,
            parser.GetSourceFromIndex(message.SourceIndex),
            parser.GetPathFromIndex(message.SourceIndex),
            message.Token,
            message.Message,
            outPositionSettings,
            enableColors, viewRange);
}

std::string PulsarTools::CreateRuntimeErrorMessageReport(
        const Pulsar::ExecutionContext& context, size_t stackTraceDepth,
        PositionSettings outPositionSettings,
        bool enableColors, TokenViewRange viewRange)
{
    const Pulsar::Module& module = context.GetModule();
    const Pulsar::CallStack& callStack = context.GetCallStack();

    if (callStack.Size() == 0) {
        return "No Runtime Error Information.";
    }

    Pulsar::String stackTrace = context.GetStackTrace(
            stackTraceDepth,
            [outPositionSettings](const auto& source, auto position)
            {
                return ConvertDefaultPositionWith(source.Source, position, outPositionSettings);
            });

    const Pulsar::Frame& frame = callStack.CurrentFrame();
    if (!module.HasSourceDebugSymbols()
        || !frame.Function->HasDebugSymbol()
        || frame.Function->DebugSymbol.SourceIdx >= module.SourceDebugSymbols.Size()) {
        std::string result = fmt::format(
                "Error: Within function {}",
                frame.Function->Name, stackTrace);
        if (stackTrace.Length() > 0)
            result += fmt::format("\n{}", stackTrace);
        return result;
    } else if (!frame.Function->HasCodeDebugSymbols()) {
        const Pulsar::SourceDebugSymbol& srcSymbol = module.SourceDebugSymbols[frame.Function->DebugSymbol.SourceIdx];
        std::string result = CreateSourceMessageReport(
                MessageReportKind_Error,
                &srcSymbol.Source, &srcSymbol.Path,
                frame.Function->DebugSymbol.Token,
                "Within function " + frame.Function->Name,
                outPositionSettings,
                enableColors, viewRange);
        if (stackTrace.Length() > 0)
            result += fmt::format("\n{}", stackTrace);
        return result;
    }

    const Pulsar::SourceDebugSymbol& srcSymbol = module.SourceDebugSymbols[frame.Function->DebugSymbol.SourceIdx];
    size_t codeSymbolIdx = 0;
    if (frame.InstructionIndex > 0 && frame.Function->FindCodeDebugSymbolFor(frame.InstructionIndex-1, codeSymbolIdx)) {
        std::string result = CreateSourceMessageReport(
                MessageReportKind_Error,
                &srcSymbol.Source, &srcSymbol.Path,
                frame.Function->CodeDebugSymbols[codeSymbolIdx].Token,
                "In function " + frame.Function->Name,
                outPositionSettings,
                enableColors, viewRange);
        if (stackTrace.Length() > 0)
            result += fmt::format("\n{}", stackTrace);
        return result;
    }

    return CreateSourceMessageReport(
            MessageReportKind_Error,
            &srcSymbol.Source, &srcSymbol.Path,
            frame.Function->DebugSymbol.Token,
            "In function call " + frame.Function->Name,
            outPositionSettings,
            enableColors, viewRange);
}
