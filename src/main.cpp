#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

#include "pulsar/parser.h"

std::ostream& operator<<(std::ostream& ostream, const Pulsar::Value& val)
{
    if (val.Type == Pulsar::ValueType::Double)
        return ostream << val.AsDouble;
    return ostream << val.AsInteger;
}

void PrintTokenView(const std::string& source, const Pulsar::Token& token)
{
    Pulsar::StringView errorView(source);
    errorView.RemovePrefix(token.SourcePos.Index-token.SourcePos.Char);
    size_t lineChars = 0;
    for (; lineChars < errorView.Length() && errorView[lineChars] != '\r' && errorView[lineChars] != '\n'; lineChars++);
    std::printf("%.*s\n", (int)lineChars, errorView.CStringFrom(0));
}

void PrintPrettyError(const std::string& source, const char* filepath, const Pulsar::Token& token, const std::string& message)
{
    std::printf("%s:%lu:%lu: Error: %s\n", filepath, token.SourcePos.Line+1, token.SourcePos.Char, message.c_str());
    PrintTokenView(source, token);
    const std::string tokenUnderline(token.SourcePos.CharSpan-1, '~');
    if (token.SourcePos.Char > 0)
        std::printf("%*s", (int)token.SourcePos.Char, "");
    std::printf("^%s\n", tokenUnderline.c_str());
}

void PrintPrettyRuntimeError(const std::string& source, const char* filepath, const Pulsar::ExecutionContext& context)
{
    if (context.CallStack.size() == 0) {
        std::cout << "No Runtime Error Information." << std::endl;
        return;
    }

    const Pulsar::Frame& frame = context.GetCurrentFrame();
    if (!frame.Function.HasDebugSymbol()) {
        std::cout << "Error: Within function " << frame.Function.Name << std::endl;
        std::cout << "    No Code Debug Symbols found." << std::endl;
        return;
    } else if (!frame.Function.HasCodeDebugSymbols()) {
        PrintPrettyError(
            source, filepath,
            frame.Function.FunctionDebugSymbol.Token,
            "Within function " + frame.Function.Name);
        return;
    }

    size_t symbolIdx = 0;
    for (size_t i = 0; i < frame.Function.CodeDebugSymbols.size(); i++) {
        // frame.InstructionIndex points to the NEXT instruction
        if (frame.Function.CodeDebugSymbols[i].StartIdx >= frame.InstructionIndex)
            break;
        symbolIdx = i;
    }

    PrintPrettyError(source, filepath, frame.Function.CodeDebugSymbols[symbolIdx].Token, "In function " + frame.Function.Name);
}

int main(int argc, const char** argv)
{
    const char* executable = *argv++;
    --argc;

    if (argc == 0) {
        std::cout << "No input file provided to " << executable << "." << std::endl;
        return 1;
    }

    const char* program = *argv;
    if (!std::filesystem::exists(program)) {
        std::cout << program << " not found." << std::endl;
        return 1;
    }

    std::ifstream file(program, std::ios::binary);
    size_t fileSize = std::filesystem::file_size(program);

    std::string source;
    source.resize(fileSize);
    file.read((char*)source.c_str(), fileSize);

    Pulsar::Module module;
    Pulsar::Parser parser(source);
    auto result = parser.ParseIntoModule(module, true);
    if (result != Pulsar::ParseResult::OK) {
        PrintPrettyError(parser.GetSource(), program, parser.GetLastErrorToken(), parser.GetLastErrorMessage());
        std::cout << "Parse Error: " << (int)result << std::endl;
        return 1;
    }

    module.BindNativeFunction({ "print!", 1, 0 },
        [](Pulsar::ExecutionContext& eContext)
        {
            Pulsar::Frame& frame = eContext.GetCurrentFrame();
            Pulsar::Value& val = frame.Locals[0];
            std::cout << val << std::endl;
            return Pulsar::RuntimeState::OK;
        });

    module.BindNativeFunction({ "hello-from-cpp", 0, 0 },
        [](Pulsar::ExecutionContext& eContext)
        {
            (void) eContext;
            std::cout << "Hello from C++!" << std::endl;
            return Pulsar::RuntimeState::OK;
        });

    Pulsar::Stack stack;
    Pulsar::ExecutionContext context = module.CreateExecutionContext();
    auto runtimeState = module.CallFunctionByName("main", stack, context);

    std::cout << "Runtime State: " << Pulsar::RuntimeStateToString(runtimeState) << std::endl;
    if (runtimeState != Pulsar::RuntimeState::OK) {
        PrintPrettyRuntimeError(parser.GetSource(), program, context);
        return 1;
    }

    std::cout << "Stack Dump:" << std::endl;
    for (size_t i = 0; i < stack.size(); i++)
        std::cout << (i+1) << ". " << stack[i] << std::endl;

    return 0;
}
