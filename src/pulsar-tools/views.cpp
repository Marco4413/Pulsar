#include "pulsar-tools/views.h"

#include <string_view>

#include "fmt/color.h"
#include "fmt/format.h"

#include "pulsar-tools/fmt.h"

PulsarTools::TokenViewLine PulsarTools::CreateTokenView(const Pulsar::String& source, const Pulsar::Token& token, TokenViewRange viewRange)
{
    std::string_view errorView(source.CString());
    errorView.remove_prefix(token.SourcePos.Index-token.SourcePos.Char);
    size_t lineChars = 0;
    for (; lineChars < errorView.length() && errorView[lineChars] != '\r' && errorView[lineChars] != '\n'; lineChars++);
    errorView.remove_suffix(errorView.length()-lineChars);

    size_t charsAfterToken = errorView.length()-token.SourcePos.Char - token.SourcePos.CharSpan;
    size_t trimmedFromStart = 0;
    if (token.SourcePos.Char > viewRange.Before) {
        trimmedFromStart = token.SourcePos.Char - viewRange.Before;
        errorView.remove_prefix(trimmedFromStart);
    }

    size_t trimmedFromEnd = 0;
    if (charsAfterToken > viewRange.After) {
        trimmedFromEnd = charsAfterToken - viewRange.After;
        errorView.remove_suffix(trimmedFromEnd);
    }

    std::string lineContents;
    if (trimmedFromStart > 0)
        lineContents += "... ";
    lineContents += errorView;
    if (trimmedFromEnd > 0)
        lineContents += " ...";

    return {
        .Contents = std::move(lineContents),
        .TokenStart = token.SourcePos.Char-trimmedFromStart + ((trimmedFromStart > 0) * 4),
    };
}

std::string PulsarTools::CreateSourceErrorMessage(
        const Pulsar::String* source, const Pulsar::String* filepath,
        const Pulsar::Token& token, const Pulsar::String& message,
        TokenViewRange viewRange)
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
        if (token.SourcePos.CharSpan > 0) {
            result += fmt::format(fmt::fg(fmt::color::red), "\n{0: ^{1}}^{0:~^{2}}", "", tokenView.TokenStart, token.SourcePos.CharSpan-1);
        } else {
            result += fmt::format(fmt::fg(fmt::color::red), "\n{0: ^{1}}^", "", tokenView.TokenStart);
        }
    } else {
        result += "No source to show.";
    }
    return result;
}

std::string PulsarTools::CreateParserErrorMessage(const Pulsar::Parser& parser, TokenViewRange viewRange)
{
    return CreateSourceErrorMessage(
        parser.GetErrorSource(), parser.GetErrorPath(),
        parser.GetErrorToken(), parser.GetErrorMessage(),
        viewRange
    );
}

std::string PulsarTools::CreateRuntimeErrorMessage(const Pulsar::ExecutionContext& context, TokenViewRange viewRange)
{
    const Pulsar::Module& module = context.GetModule();
    const Pulsar::CallStack& callStack = context.GetCallStack();

    if (callStack.Size() == 0) {
        return "No Runtime Error Information.";
    }

    Pulsar::String stackTrace = context.GetStackTrace(10);
    const Pulsar::Frame& frame = callStack.CurrentFrame();
    if (!module.HasSourceDebugSymbols()
        || !frame.Function->HasDebugSymbol()
        || frame.Function->DebugSymbol.SourceIdx >= module.SourceDebugSymbols.Size()) {
        return fmt::format(
            "Error: Within function {}\n{}",
            frame.Function->Name, stackTrace);
    } else if (!frame.Function->HasCodeDebugSymbols()) {
        const Pulsar::SourceDebugSymbol& srcSymbol = module.SourceDebugSymbols[frame.Function->DebugSymbol.SourceIdx];
        std::string result = CreateSourceErrorMessage(
            &srcSymbol.Source, &srcSymbol.Path,
            frame.Function->DebugSymbol.Token,
            "Within function " + frame.Function->Name,
            viewRange);
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
            viewRange);
        result += fmt::format("\n{}", stackTrace);
        return result;
    }

    return CreateSourceErrorMessage(
        &srcSymbol.Source, &srcSymbol.Path,
        frame.Function->DebugSymbol.Token,
        "In function call " + frame.Function->Name,
        viewRange);
}
