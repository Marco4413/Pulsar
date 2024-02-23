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
    const char* filepath,
    const Pulsar::ExecutionContext& context,
    TokenViewRange viewRange)
{
    if (context.CallStack.Size() == 0) {
        fmt::format_to(std::back_inserter(out), "No Runtime Error Information.");
        return;
    }

    const Pulsar::Frame& frame = context.CallStack.CurrentFrame();
    if (!context.OwnerModule->HasSourceDebugSymbols() || !frame.Function->HasDebugSymbol()) {
        fmt::format_to(
            std::back_inserter(out),
            "Error: Within function {}\n"
            "    No Code Debug Symbols found.",
            frame.Function->Name);
        return;
    } else if (!frame.Function->HasCodeDebugSymbols()) {
        const auto& srcSymbol = context.OwnerModule->SourceDebugSymbols[frame.Function->DebugSymbol.SourceIdx];
        PrintPrettyError(
            out, srcSymbol.Source, filepath,
            frame.Function->DebugSymbol.Token,
            "Within function " + frame.Function->Name,
            viewRange);
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
        out, srcSymbol.Source, filepath,
        frame.Function->CodeDebugSymbols[symbolIdx].Token,
        "In function " + frame.Function->Name,
        viewRange);
}

class ModuleNativeBindings
{
public:
    ModuleNativeBindings() = default;
    ~ModuleNativeBindings() = default;

    void BindToModule(Pulsar::Module& module)
    {
        uint64_t type = module.BindCustomType("ModuleHandle");
        module.BindNativeFunction({ "module/from-file", 1, 1 },
            [&, type](auto& ctx) { return Module_FromFile(ctx, type); });
        module.BindNativeFunction({ "module/run", 1, 2 },
            [&, type](auto& ctx) { return Module_Run(ctx, type); });
        module.BindNativeFunction({ "module/free", 1, 0 },
            [&, type](auto& ctx) { return Module_Free(ctx, type); });
    }

    Pulsar::RuntimeState Module_FromFile(Pulsar::ExecutionContext& eContext, uint64_t type)
    {
        Pulsar::Frame& frame = eContext.CallStack.CurrentFrame();
        Pulsar::Value& modulePath = frame.Locals[0];
        if (modulePath.Type() != Pulsar::ValueType::String)
            return Pulsar::RuntimeState::TypeError;

        std::ifstream file(modulePath.AsString().Data(), std::ios::binary);
        size_t fileSize = std::filesystem::file_size(modulePath.AsString().Data());

        Pulsar::String source;
        source.Resize(fileSize);
        file.read((char*)source.Data(), fileSize);

        Pulsar::Module module;
        Pulsar::Parser parser(source);
        auto result = parser.ParseIntoModule(module, true);
        if (result != Pulsar::ParseResult::OK)
            return Pulsar::RuntimeState::Error;
        
        int64_t handle = m_NextHandle++;
        frame.Stack.EmplaceBack()
            .SetCustom({ .Type=type, .Handle=handle });
        m_Modules[handle] = std::move(module);
        return Pulsar::RuntimeState::OK;
    }

    Pulsar::RuntimeState Module_Run(Pulsar::ExecutionContext& eContext, uint64_t type) const
    {
        Pulsar::Frame& frame = eContext.CallStack.CurrentFrame();
        Pulsar::Value& moduleHandle = frame.Locals[0];
        if (moduleHandle.Type() != Pulsar::ValueType::Custom
            || moduleHandle.AsCustom().Type != type)
            return Pulsar::RuntimeState::TypeError;

        const Pulsar::Module& module = (*m_Modules.find(moduleHandle.AsCustom().Handle)).second;
        Pulsar::ValueStack stack;
        Pulsar::ExecutionContext context = module.CreateExecutionContext();
        auto runtimeState = module.CallFunctionByName("main", stack, context);
        if (runtimeState != Pulsar::RuntimeState::OK)
            return Pulsar::RuntimeState::Error;

        frame.Stack.EmplaceBack(moduleHandle);
        Pulsar::ValueList retValues;
        for (size_t i = 0; i < stack.Size(); i++)
            retValues.Append()->Value() = std::move(stack[i]);
        frame.Stack.EmplaceBack()
            .SetList(std::move(retValues));
        return Pulsar::RuntimeState::OK;
    }

    Pulsar::RuntimeState Module_Free(Pulsar::ExecutionContext& eContext, uint64_t type)
    {
        Pulsar::Frame& frame = eContext.CallStack.CurrentFrame();
        Pulsar::Value& moduleHandle = frame.Locals[0];
        if (moduleHandle.Type() != Pulsar::ValueType::Custom
            || moduleHandle.AsCustom().Type != type)
            return Pulsar::RuntimeState::TypeError;

        m_Modules.erase(moduleHandle.AsCustom().Handle);
        return Pulsar::RuntimeState::OK;
    }

private:
    int64_t m_NextHandle = 0;
    std::unordered_map<int64_t, Pulsar::Module> m_Modules;
};

class LexerNativeBindings
{
public:
    LexerNativeBindings() = default;
    ~LexerNativeBindings() = default;

    void BindToModule(Pulsar::Module& module)
    {
        uint64_t type = module.BindCustomType("LexerHandle");
        module.BindNativeFunction({ "lexer/from-file", 1, 1 },
            [&, type](auto& ctx) { return Lexer_FromFile(ctx, type); });
        module.BindNativeFunction({ "lexer/next-token", 1, 2 },
            [&, type](auto& ctx) { return Lexer_NextToken(ctx, type); });
        module.BindNativeFunction({ "lexer/free", 1, 0 },
            [&, type](auto& ctx) { return Lexer_Free(ctx, type); });
        module.BindNativeFunction({ "lexer/valid?", 1, 2 },
            [&, type](auto& ctx) { return Lexer_IsValid(ctx, type); });
    }

    Pulsar::RuntimeState Lexer_FromFile(Pulsar::ExecutionContext& eContext, uint64_t type)
    {
        Pulsar::Frame& frame = eContext.CallStack.CurrentFrame();
        Pulsar::Value& filePath = frame.Locals[0];
        if (filePath.Type() != Pulsar::ValueType::String)
            return Pulsar::RuntimeState::TypeError;

        if (!std::filesystem::exists(filePath.AsString().Data())) {
            frame.Stack.EmplaceBack()
                .SetCustom({ .Type=type, .Handle=0 });
            return Pulsar::RuntimeState::OK;
        }

        std::ifstream file(filePath.AsString().Data(), std::ios::binary);
        size_t fileSize = std::filesystem::file_size(filePath.AsString().Data());

        Pulsar::String source;
        source.Resize(fileSize);
        if (!file.read((char*)source.Data(), fileSize)) {
            frame.Stack.EmplaceBack()
                .SetCustom({ .Type=type, .Handle=0 });
            return Pulsar::RuntimeState::OK;
        }

        int64_t handle = m_NextHandle++;
        frame.Stack.EmplaceBack()
            .SetCustom({ .Type=type, .Handle=handle });
        m_Lexers.emplace(handle, std::move(source));
        return Pulsar::RuntimeState::OK;
    }

    Pulsar::RuntimeState Lexer_NextToken(Pulsar::ExecutionContext& eContext, uint64_t type)
    {
        Pulsar::Frame& frame = eContext.CallStack.CurrentFrame();
        Pulsar::Value& lexerHandle = frame.Locals[0];
        if (lexerHandle.Type() != Pulsar::ValueType::Custom
            || lexerHandle.AsCustom().Type != type)
            return Pulsar::RuntimeState::TypeError;

        Pulsar::Lexer& lexer = (*m_Lexers.find(lexerHandle.AsCustom().Handle)).second;
        Pulsar::Token token = lexer.NextToken();
        
        Pulsar::ValueList tokenAsList;
        tokenAsList.Append()->Value().SetInteger((int64_t)token.Type);
        tokenAsList.Append()->Value().SetString(Pulsar::TokenTypeToString(token.Type));
        switch (token.Type) {
        case Pulsar::TokenType::IntegerLiteral:
            tokenAsList.Append()->Value().SetInteger(token.IntegerVal);
            break;
        case Pulsar::TokenType::DoubleLiteral:
            tokenAsList.Append()->Value().SetInteger(token.DoubleVal);
            break;
        default:
            if (token.StringVal.Length() > 0)
                tokenAsList.Append()->Value().SetString(token.StringVal);
        }

        frame.Stack.EmplaceBack(lexerHandle);
        frame.Stack.EmplaceBack()
            .SetList(std::move(tokenAsList));

        return Pulsar::RuntimeState::OK;
    }

    Pulsar::RuntimeState Lexer_Free(Pulsar::ExecutionContext& eContext, uint64_t type)
    {
        Pulsar::Frame& frame = eContext.CallStack.CurrentFrame();
        Pulsar::Value& lexerHandle = frame.Locals[0];
        if (lexerHandle.Type() != Pulsar::ValueType::Custom
            || lexerHandle.AsCustom().Type != type)
            return Pulsar::RuntimeState::TypeError;
        m_Lexers.erase(lexerHandle.AsCustom().Handle);
        return Pulsar::RuntimeState::OK;
    }

    Pulsar::RuntimeState Lexer_IsValid(Pulsar::ExecutionContext& eContext, uint64_t type)
    {
        Pulsar::Frame& frame = eContext.CallStack.CurrentFrame();
        Pulsar::Value& lexerHandle = frame.Locals[0];
        if (lexerHandle.Type() != Pulsar::ValueType::Custom
            || lexerHandle.AsCustom().Type != type)
            return Pulsar::RuntimeState::TypeError;
        frame.Stack.EmplaceBack(lexerHandle);
        frame.Stack.EmplaceBack()
            .SetInteger(lexerHandle.AsCustom().Handle);
        return Pulsar::RuntimeState::OK;
    }

private:
    int64_t m_NextHandle = 1;
    std::unordered_map<int64_t, Pulsar::Lexer> m_Lexers;
};

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
    { // Parse Module
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
    }

    ModuleNativeBindings moduleBindings;
    moduleBindings.BindToModule(module);

    LexerNativeBindings lexerBindings;
    lexerBindings.BindToModule(module);

    module.BindNativeFunction({ "print!", 1, 0 },
        [](Pulsar::ExecutionContext& eContext)
        {
            Pulsar::Frame& frame = eContext.CallStack.CurrentFrame();
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
            Pulsar::Frame& frame = eContext.CallStack.CallingFrame();
            fmt::print("Stack Dump: [");
            for (size_t i = 0; i < frame.Stack.Size(); i++) {
                if (i > 0) fmt::print(",");
                fmt::print(" {}", frame.Stack[i]);
            }
            fmt::println(" ]");
            return Pulsar::RuntimeState::OK;
        });

    auto startTime = std::chrono::high_resolution_clock::now();
    Pulsar::ValueStack stack;
    { // Push argv into the Stack.
        Pulsar::ValueList argList;
        for (int i = 0; i < argc; i++)
            argList.Append()->Value().SetString(argv[i]);
        stack.EmplaceBack().SetList(std::move(argList));
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
            prettyError, program,
            context, DEFAULT_VIEW_RANGE);
        fmt::println("{:.{}}", prettyError.data(), prettyError.size());
        return 1;
    }

    fmt::println("Stack Dump:");
    for (size_t i = 0; i < stack.Size(); i++)
        fmt::println("{}. {}", i+1, stack[i]);

    return 0;
}
