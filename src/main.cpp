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

int main(int argc, const char** argv)
{
    const char* executable = *argv++;
    --argc;

    if (argc == 0) {
        std::cout << "No input file provided to " << executable << "." << std::endl;
        return 1;
    }

    std::filesystem::path program = *argv;
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
