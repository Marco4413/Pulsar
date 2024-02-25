#include "fmt/color.h"
#include "pulsar/parser.h"

template <>
struct fmt::formatter<Pulsar::String> : formatter<string_view>
{
    auto format(const Pulsar::String& val, format_context& ctx) const
    {
        return fmt::format_to(ctx.out(), "{:.{}}", val.Data(), val.Length());
    }
};

template <>
struct fmt::formatter<Pulsar::StringView> : formatter<string_view>
{
    auto format(const Pulsar::StringView& val, format_context& ctx) const
    {
        return fmt::format_to(ctx.out(), "{:.{}}", val.CStringFrom(0), val.Length());
    }
};

template <>
struct fmt::formatter<Pulsar::Value> : formatter<string_view>
{
    auto format(const Pulsar::Value& val, format_context& ctx) const
    {
        switch (val.Type()) {
        case Pulsar::ValueType::Void:
            return fmt::format_to(ctx.out(), "Void");
        case Pulsar::ValueType::Custom:
            return fmt::format_to(ctx.out(), "Custom(.Type={},.Handle={})", val.AsCustom().Type, val.AsCustom().Handle);
        case Pulsar::ValueType::Integer:
            return fmt::format_to(ctx.out(), "{}", val.AsInteger());
        case Pulsar::ValueType::FunctionReference:
            return fmt::format_to(ctx.out(), "<&(@{})", val.AsInteger());
        case Pulsar::ValueType::NativeFunctionReference:
            return fmt::format_to(ctx.out(), "<&(*@{})", val.AsInteger());
        case Pulsar::ValueType::Double:
            return fmt::format_to(ctx.out(), "{}", val.AsDouble());
        case Pulsar::ValueType::String:
            return fmt::format_to(ctx.out(), "{}", Pulsar::ToStringLiteral(val.AsString()));
        case Pulsar::ValueType::List: {
            auto start = val.AsList().Front();
            if (!start) return fmt::format_to(ctx.out(), "[ ]");
            fmt::format_to(ctx.out(), "[ {}", start->Value());
            auto next = start->Next();
            while (next) {
                fmt::format_to(ctx.out(), ", {}", next->Value());
                next = next->Next();
            }
            return fmt::format_to(ctx.out(), " ]");
        }
        }
        return fmt::format_to(ctx.out(), "FormatError");
    }
};

struct TokenViewRange { size_t Before; size_t After; };
#define DEFAULT_VIEW_RANGE TokenViewRange{20, 20}

size_t PrintTokenView(const Pulsar::String& source, const Pulsar::Token& token, TokenViewRange viewRange)
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
        fmt::print("... ");
    fmt::print("{}", errorView);
    if (trimmedFromEnd > 0)
        fmt::print(" ...");
    return trimmedFromStart;
}

void PrintPrettyError(
    const Pulsar::String& source, const Pulsar::String& filepath,
    const Pulsar::Token& token, const Pulsar::String& message,
    TokenViewRange viewRange)
{
    fmt::print("{}:{}:{}: Error: {}\n", filepath, token.SourcePos.Line+1, token.SourcePos.Char+1, message);
    size_t trimmedFromStart = PrintTokenView(source, token, viewRange);
    size_t charsToToken = token.SourcePos.Char-trimmedFromStart + (trimmedFromStart > 0 ? 4 : 0);
    fmt::print(fmt::fg(fmt::color::red), "\n{0: ^{1}}^{0:~^{2}}", "", charsToToken, token.SourcePos.CharSpan-1);
}

void PrintPrettyRuntimeError(
    const Pulsar::ExecutionContext& context,
    TokenViewRange viewRange)
{
    if (context.CallStack.Size() == 0) {
        fmt::print("No Runtime Error Information.");
        return;
    }

    Pulsar::String stackTrace = context.GetStackTrace(10);
    const Pulsar::Frame& frame = context.CallStack.CurrentFrame();
    if (!context.OwnerModule->HasSourceDebugSymbols() || !frame.Function->HasDebugSymbol()) {
        fmt::print(
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
        fmt::print("\n{}", stackTrace);
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
    PrintPrettyError(
        srcSymbol.Source, srcSymbol.Path,
        frame.Function->CodeDebugSymbols[symbolIdx].Token,
        "In function " + frame.Function->Name,
        viewRange);
    fmt::print("\n{}", stackTrace);
}
