#include "pulsar-tools/bindings/stdio.h"

#include <iostream>

void PulsarTools::STDIONativeBindings::BindToModule(Pulsar::Module& module)
{
    module.BindNativeFunction({ "stdin/read",      0, 1 }, STDIN_Read);
    module.BindNativeFunction({ "stdout/write!",   1, 0 }, STDOUT_Write);
    module.BindNativeFunction({ "stdout/writeln!", 1, 0 }, STDOUT_WriteLn);
}

Pulsar::RuntimeState PulsarTools::STDIONativeBindings::STDIN_Read(Pulsar::ExecutionContext& eContext)
{
    constexpr size_t BUFFER_CAPACITY = 256;
    char buffer[BUFFER_CAPACITY];

    Pulsar::String line;
    do { /* Repeat while count-1 chars extracted */
        std::cin.clear(); // Clear fail bit
        std::cin.getline(buffer, BUFFER_CAPACITY);
        line += buffer;
    } while (std::cin.fail() && !std::cin.eof() && !std::cin.bad());

    eContext.CurrentFrame().Stack
        .EmplaceBack().SetString(std::move(line));

    return Pulsar::RuntimeState::OK;
}

Pulsar::RuntimeState PulsarTools::STDIONativeBindings::STDOUT_Write(Pulsar::ExecutionContext& eContext)
{
    Pulsar::Frame& frame = eContext.CurrentFrame();
    Pulsar::Value& message = frame.Locals[0];
    if (message.Type() != Pulsar::ValueType::String)
        return Pulsar::RuntimeState::TypeError;
    std::cout << message.AsString().CString() << std::flush;
    return Pulsar::RuntimeState::OK;
}

Pulsar::RuntimeState PulsarTools::STDIONativeBindings::STDOUT_WriteLn(Pulsar::ExecutionContext& eContext)
{
    Pulsar::Frame& frame = eContext.CurrentFrame();
    Pulsar::Value& message = frame.Locals[0];
    if (message.Type() != Pulsar::ValueType::String)
        return Pulsar::RuntimeState::TypeError;
    std::cout << message.AsString().CString() << std::endl;
    return Pulsar::RuntimeState::OK;
}
