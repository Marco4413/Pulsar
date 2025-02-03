#include "pulsar-tools/views.h"

#include <string_view>

#include "fmt/color.h"
#include "fmt/format.h"

#include "pulsar/utf8.h"
#include "pulsar/structures/stringview.h"

#include "pulsar-tools/fmt.h"

PulsarTools::TokenViewLine PulsarTools::CreateTokenView(const Pulsar::String& source, const Pulsar::Token& token, TokenViewRange viewRange)
{
    Pulsar::StringView errorView = source;
    { // Trim errorView to only contain the token's line
        size_t prevLineIndex = token.SourcePos.Index;
        for (size_t i = prevLineIndex; i > 0 && errorView[i] != '\r' && errorView[i] != '\n'; --i)
            prevLineIndex = i; // Only save i for previous iteration, we don't want to stop at '\n'
        errorView.RemovePrefix(prevLineIndex);

        size_t lineBytes = 0;
        while (lineBytes < errorView.Length() && errorView[lineBytes] != '\r' && errorView[lineBytes] != '\n')
            ++lineBytes;
        errorView.RemoveSuffix(errorView.Length()-lineBytes);
    }

    size_t charsTrimmedFromStart = 0;
    size_t charsTrimmedFromEnd = 0;
    { // Trim errorView to fit viewRange
        Pulsar::UTF8::Decoder decoder(errorView);
        size_t errorViewLen = Pulsar::UTF8::Length(errorView);

        PULSAR_ASSERT((token.SourcePos.Char + token.SourcePos.CharSpan) <= errorViewLen, "Error line is smaller than what advertised. We don't like false advertising.");
        size_t charsAfterToken = errorViewLen - (token.SourcePos.Char + token.SourcePos.CharSpan);

        if (token.SourcePos.Char > viewRange.Before) {
            charsTrimmedFromStart = token.SourcePos.Char - viewRange.Before;
            for (size_t i = 0; i < charsTrimmedFromStart; ++i)
                errorView.RemovePrefix(decoder.Skip());
        }

        for (size_t i = 0; i < token.SourcePos.CharSpan; ++i)
            decoder.Skip();

        if (charsAfterToken > viewRange.After) {
            charsTrimmedFromEnd = charsAfterToken - viewRange.After;
            for (size_t i = 0; i < charsTrimmedFromEnd; ++i)
                errorView.RemoveSuffix(decoder.Skip());
        }
    }

    size_t tokenStart = token.SourcePos.Char-charsTrimmedFromStart;
    size_t tokenStartWidth = 0;
    { // Calculate char width
        Pulsar::UTF8::Decoder decoder(errorView);
        for (size_t i = 0; i < tokenStart; ++i) {
            tokenStartWidth += Pulsar::Unicode::Width(decoder.Next());
        }
    }

    std::string lineContents;
    if (charsTrimmedFromStart > 0)
        lineContents += "... ";
    lineContents.append(errorView.DataFromStart(), errorView.Length());
    if (charsTrimmedFromEnd > 0)
        lineContents += " ...";

    return {
        .Contents = std::move(lineContents),
        .TokenStart = tokenStartWidth + ((charsTrimmedFromStart > 0) * 4),
    };
}

std::string PulsarTools::CreateSourceErrorMessage(
        const Pulsar::String* source, const Pulsar::String* filepath,
        const Pulsar::Token& token, const Pulsar::String& message,
        bool enableColors, TokenViewRange viewRange)
{
    std::string result;
    if (filepath) {
        result += fmt::format("{}:{}:{}: Error: {}\n", *filepath, token.SourcePos.Line+1, token.SourcePos.Char+1, message);
    } else {
        result += fmt::format("Error: {}\n", message);
    }
    if (source) {
        TokenViewLine tokenView = CreateTokenView(*source, token, viewRange);
        result += tokenView.Contents;
        result += '\n';

        std::string tokenCursor;
        if (token.SourcePos.CharSpan > 0) {
            tokenCursor = fmt::format("{0: ^{1}}^{0:~^{2}}", "", tokenView.TokenStart, token.SourcePos.CharSpan-1);
        } else {
            tokenCursor = fmt::format("{0: ^{1}}^", "", tokenView.TokenStart);
        }

        if (enableColors) {
            result += fmt::format(fmt::fg(fmt::color::red), "{}", tokenCursor);
        } else {
            result += tokenCursor;
        }
    } else {
        result += "No source to show.";
    }
    return result;
}

std::string PulsarTools::CreateParserErrorMessage(const Pulsar::Parser& parser, bool enableColors, TokenViewRange viewRange)
{
    return CreateSourceErrorMessage(
        parser.GetErrorSource(), parser.GetErrorPath(),
        parser.GetErrorToken(), parser.GetErrorMessage(),
        enableColors, viewRange
    );
}

std::string PulsarTools::CreateRuntimeErrorMessage(const Pulsar::ExecutionContext& context, size_t stackTraceDepth, bool enableColors, TokenViewRange viewRange)
{
    const Pulsar::Module& module = context.GetModule();
    const Pulsar::CallStack& callStack = context.GetCallStack();

    if (callStack.Size() == 0) {
        return "No Runtime Error Information.";
    }

    Pulsar::String stackTrace = context.GetStackTrace(stackTraceDepth);

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
        std::string result = CreateSourceErrorMessage(
            &srcSymbol.Source, &srcSymbol.Path,
            frame.Function->DebugSymbol.Token,
            "Within function " + frame.Function->Name,
            enableColors, viewRange);
        if (stackTrace.Length() > 0)
            result += fmt::format("\n{}", stackTrace);
        return result;
    }

    const Pulsar::SourceDebugSymbol& srcSymbol = module.SourceDebugSymbols[frame.Function->DebugSymbol.SourceIdx];
    size_t codeSymbolIdx = 0;
    if (frame.InstructionIndex > 0 && frame.Function->FindCodeDebugSymbolFor(frame.InstructionIndex-1, codeSymbolIdx)) {
        std::string result = CreateSourceErrorMessage(
            &srcSymbol.Source, &srcSymbol.Path,
            frame.Function->CodeDebugSymbols[codeSymbolIdx].Token,
            "In function " + frame.Function->Name,
            enableColors, viewRange);
        if (stackTrace.Length() > 0)
            result += fmt::format("\n{}", stackTrace);
        return result;
    }

    return CreateSourceErrorMessage(
        &srcSymbol.Source, &srcSymbol.Path,
        frame.Function->DebugSymbol.Token,
        "In function call " + frame.Function->Name,
        enableColors, viewRange);
}
