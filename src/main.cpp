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

void PrintPrettyError(const std::string& source, const char* filepath, const Pulsar::Token& token, const char* message)
{
    std::printf("%s:%lu:%lu: Error: %s\n", filepath, token.SourcePos.Line+1, token.SourcePos.Char, message);

    Pulsar::Structures::StringView errorView(source);
    errorView.RemovePrefix(token.SourcePos.Index-token.SourcePos.Char+1);
    size_t lineChars = 0;
    for (; lineChars < errorView.Length() && errorView[lineChars] != '\r' && errorView[lineChars] != '\n'; lineChars++);
    std::printf("%.*s\n", (int)lineChars, errorView.CStringFrom(0));
    
    const std::string tokenUnderline(token.SourcePos.CharSpan-1, '~');
    if (token.SourcePos.Char > 0)
        std::printf("%*s", (int)token.SourcePos.Char-1, "");
    std::printf("^%s\n", tokenUnderline.c_str());
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
    auto result = parser.ParseIntoModule(module);
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
    auto runtimeState = module.CallFunctionByName("main", stack);
    std::cout << "Runtime State: " << Pulsar::RuntimeStateToString(runtimeState) << std::endl;
    if (runtimeState != Pulsar::RuntimeState::OK)
        return 1;

    std::cout << "Stack Dump:" << std::endl;
    for (size_t i = 0; i < stack.size(); i++)
        std::cout << (i+1) << ". " << stack[i] << std::endl;

    return 0;
}
