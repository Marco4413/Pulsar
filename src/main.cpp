#include <chrono>
#include <filesystem>
#include <fstream>

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
        case Pulsar::ValueType::Integer:
        case Pulsar::ValueType::FunctionReference:
        case Pulsar::ValueType::NativeFunctionReference:
            break;
        case Pulsar::ValueType::Double:
            return fmt::format_to(ctx.out(), "{}", val.AsDouble());
        case Pulsar::ValueType::String:
            return fmt::format_to(ctx.out(), "\"{}\"", val.AsString());
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
        return fmt::format_to(ctx.out(), "{}", val.AsInteger());
    }
};

struct TokenViewRange { size_t Before; size_t After; };
#define DEFAULT_VIEW_RANGE TokenViewRange{20, 20}

size_t PrintTokenView(fmt::memory_buffer& out, const Pulsar::String& source, const Pulsar::Token& token, TokenViewRange viewRange)
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
    
    auto outIt = std::back_inserter(out);
    if (trimmedFromStart > 0)
        fmt::format_to(outIt, "... ");
    fmt::format_to(outIt, "{}", errorView);
    if (trimmedFromEnd > 0)
        fmt::format_to(outIt, " ...");
    return trimmedFromStart;
}

void PrintPrettyError(
    fmt::memory_buffer& out,
    const Pulsar::String& source, const char* filepath,
    const Pulsar::Token& token, const Pulsar::String& message,
    TokenViewRange viewRange)
{
    fmt::format_to(std::back_inserter(out), "{}:{}:{}: Error: {}\n", filepath, token.SourcePos.Line+1, token.SourcePos.Char+1, message.Data());
    size_t trimmedFromStart = PrintTokenView(out, source, token, viewRange);
    size_t charsToToken = token.SourcePos.Char-trimmedFromStart + (trimmedFromStart > 0 ? 4 : 0);
    fmt::format_to(std::back_inserter(out), fmt::fg(fmt::color::red), "\n{0: ^{1}}^{0:~^{2}}", "", charsToToken, token.SourcePos.CharSpan-1);
}

void PrintPrettyRuntimeError(
    fmt::memory_buffer& out,
    const Pulsar::String& source, const char* filepath,
    const Pulsar::ExecutionContext& context,
    TokenViewRange viewRange)
{
    if (context.CallStack.size() == 0) {
        fmt::format_to(std::back_inserter(out), "No Runtime Error Information.");
        return;
    }

    const Pulsar::Frame& frame = context.GetCurrentFrame();
    if (!frame.Function->HasDebugSymbol()) {
        fmt::format_to(
            std::back_inserter(out),
            "Error: Within function {}\n"
            "    No Code Debug Symbols found.",
            frame.Function->Name);
        return;
    } else if (!frame.Function->HasCodeDebugSymbols()) {
        PrintPrettyError(
            out, source, filepath,
            frame.Function->FunctionDebugSymbol.Token,
            "Within function " + frame.Function->Name,
            viewRange);
        return;
    }

    size_t symbolIdx = 0;
    for (size_t i = 0; i < frame.Function->CodeDebugSymbols.size(); i++) {
        // frame.InstructionIndex points to the NEXT instruction
        if (frame.Function->CodeDebugSymbols[i].StartIdx >= frame.InstructionIndex)
            break;
        symbolIdx = i;
    }

    PrintPrettyError(
        out, source, filepath,
        frame.Function->CodeDebugSymbols[symbolIdx].Token,
        "In function " + frame.Function->Name,
        viewRange);
}

int main(int argc, const char** argv)
{
    const char* executable = *argv++;
    --argc;

    if (argc == 0) {
        fmt::println("No input file provided to {}.", executable);
        return 1;
    }

    const char* program = *argv;
    if (!std::filesystem::exists(program)) {
        fmt::println("{} not found.", program);
        return 1;
    }

    std::ifstream file(program, std::ios::binary);
    size_t fileSize = std::filesystem::file_size(program);

    Pulsar::String source;
    source.Resize(fileSize);
    file.read((char*)source.Data(), fileSize);

    Pulsar::Module module;
    Pulsar::Parser parser(source);
    auto result = parser.ParseIntoModule(module, true);
    if (result != Pulsar::ParseResult::OK) {
        fmt::memory_buffer prettyError;
        PrintPrettyError(
            prettyError, parser.GetSource(), program,
            parser.GetLastErrorToken(), parser.GetLastErrorMessage(),
            DEFAULT_VIEW_RANGE);
        fmt::println("{:.{}}", prettyError.data(), prettyError.size());
        fmt::println("Parse Error: {}", (int)result);
        return 1;
    }

    module.BindNativeFunction({ "print!", 1, 0 },
        [](Pulsar::ExecutionContext& eContext)
        {
            Pulsar::Frame& frame = eContext.GetCurrentFrame();
            Pulsar::Value& val = frame.Locals[0];
            fmt::println("{}", val);
            return Pulsar::RuntimeState::OK;
        });

    module.BindNativeFunction({ "hello-from-cpp", 0, 0 },
        [](Pulsar::ExecutionContext& eContext)
        {
            (void) eContext;
            fmt::println("Hello from C++!");
            return Pulsar::RuntimeState::OK;
        });

    module.BindNativeFunction({ "stack-dump", 0, 0 },
        [](Pulsar::ExecutionContext& eContext)
        {
            Pulsar::Frame* frame = eContext.GetCallingFrame();
            fmt::print("Stack Dump: [");
            for (size_t i = 0; i < frame->OperandStack.size(); i++) {
                if (i > 0) fmt::print(",");
                fmt::print(" {}", frame->OperandStack[i]);
            }
            fmt::println(" ]");
            return Pulsar::RuntimeState::OK;
        });

    auto startTime = std::chrono::high_resolution_clock::now();
    Pulsar::Stack stack;
    {
        Pulsar::ValueList argList;
        for (int i = 0; i < argc; i++)
            argList.Append()->Value().SetString(argv[i]);
        stack.emplace_back().SetList(std::move(argList));
    }
    Pulsar::ExecutionContext context = module.CreateExecutionContext();
    auto runtimeState = module.CallFunctionByName("main", stack, context);
    auto stopTime = std::chrono::high_resolution_clock::now();

    auto execTime = std::chrono::duration_cast<std::chrono::microseconds>(stopTime-startTime);
    fmt::println("Execution took: {}us", execTime.count());

    fmt::println("Runtime State: {}", Pulsar::RuntimeStateToString(runtimeState));
    if (runtimeState != Pulsar::RuntimeState::OK) {
        fmt::memory_buffer prettyError;
        PrintPrettyRuntimeError(
            prettyError, parser.GetSource(), program,
            context, DEFAULT_VIEW_RANGE);
        fmt::println("{:.{}}", prettyError.data(), prettyError.size());
        return 1;
    }

    fmt::println("Stack Dump:");
    for (size_t i = 0; i < stack.size(); i++)
        fmt::println("{}. {}", i+1, stack[i]);

    return 0;
}
