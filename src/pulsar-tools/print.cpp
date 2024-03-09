#include "pulsar-tools/print.h"

size_t PulsarTools::PrintTokenView(const Pulsar::String& source, const Pulsar::Token& token, TokenViewRange viewRange)
{
    Pulsar::StringView errorView(source);
    errorView.RemovePrefix(token.SourcePos.Index-token.SourcePos.Char);
    size_t lineChars = 0;
    for (; lineChars < errorView.Length() && errorView[lineChars] != '\r' && errorView[lineChars] != '\n'; lineChars++);
    errorView.RemoveSuffix(errorView.Length()-lineChars);

    size_t charsAfterToken = errorView.Length()-token.SourcePos.Char - token.SourcePos.CharSpan;
    size_t trimmedFromStart = 0;
    if (token.SourcePos.Char > viewRange.Before) {
        trimmedFromStart = token.SourcePos.Char - viewRange.Before;
        errorView.RemovePrefix(trimmedFromStart);
    }

    size_t trimmedFromEnd = 0;
    if (charsAfterToken > viewRange.After) {
        trimmedFromEnd = charsAfterToken - viewRange.After;
        errorView.RemoveSuffix(trimmedFromEnd);
    }
    
    if (trimmedFromStart > 0)
        PULSARTOOLS_PRINTF("... ");
    PULSARTOOLS_PRINTF("{}", errorView);
    if (trimmedFromEnd > 0)
        PULSARTOOLS_PRINTF(" ...");
    return trimmedFromStart;
}

void PulsarTools::PrintPrettyError(
    const Pulsar::String& source, const Pulsar::String& filepath,
    const Pulsar::Token& token, const Pulsar::String& message,
    TokenViewRange viewRange)
{
    PULSARTOOLS_PRINTF("{}:{}:{}: Error: {}\n", filepath, token.SourcePos.Line+1, token.SourcePos.Char+1, message);
    size_t trimmedFromStart = PrintTokenView(source, token, viewRange);
    size_t charsToToken = token.SourcePos.Char-trimmedFromStart + (trimmedFromStart > 0 ? 4 : 0);
    if (token.SourcePos.CharSpan > 0)
        PULSARTOOLS_PRINTF(fmt::fg(fmt::color::red), "\n{0: ^{1}}^{0:~^{2}}", "", charsToToken, token.SourcePos.CharSpan-1);
}

void PulsarTools::PrintPrettyRuntimeError(const Pulsar::ExecutionContext& context, TokenViewRange viewRange)
{
    if (context.CallStack.Size() == 0) {
        PULSARTOOLS_PRINTF("No Runtime Error Information.");
        return;
    }

    Pulsar::String stackTrace = context.GetStackTrace(10);
    const Pulsar::Frame& frame = context.CallStack.CurrentFrame();
    if (!context.OwnerModule->HasSourceDebugSymbols() || !frame.Function->HasDebugSymbol()) {
        PULSARTOOLS_PRINTF(
            "Error: Within function {}\n{}",
            frame.Function->Name, stackTrace);
        return;
    } else if (!frame.Function->HasCodeDebugSymbols()) {
        const auto& srcSymbol = context.OwnerModule->SourceDebugSymbols[frame.Function->DebugSymbol.SourceIdx];
        PrintPrettyError(
            srcSymbol.Source, srcSymbol.Path,
            frame.Function->DebugSymbol.Token,
            "Within function " + frame.Function->Name,
            viewRange);
        PULSARTOOLS_PRINTF("\n{}", stackTrace);
        return;
    }

    size_t symbolIdx = 0;
    for (size_t i = 0; i < frame.Function->CodeDebugSymbols.Size(); i++) {
        // frame.InstructionIndex points to the NEXT instruction
        if (frame.Function->CodeDebugSymbols[i].StartIdx >= frame.InstructionIndex)
            break;
        symbolIdx = i;
    }

    const auto& srcSymbol = context.OwnerModule->SourceDebugSymbols[frame.Function->DebugSymbol.SourceIdx];
    if (symbolIdx > 0) {
        PrintPrettyError(
            srcSymbol.Source, srcSymbol.Path,
            frame.Function->CodeDebugSymbols[symbolIdx].Token,
            "In function " + frame.Function->Name,
            viewRange);
    } else {
        PrintPrettyError(
            srcSymbol.Source, srcSymbol.Path,
            frame.Function->DebugSymbol.Token,
            "In function call " + frame.Function->Name,
            viewRange);
        return;
    }
    PULSARTOOLS_PRINTF("\n{}", stackTrace);
}
