#include "pulsar-bindings/std/stdio.h"

#include <iostream>

PulsarBindings::Std::Stdio::Stdio()
    : Binding()
{
    BindNativeFunction({ "stdin/read",      0, 1 }, FInRead);
    BindNativeFunction({ "stdout/write!",   1, 0 }, FOutWrite);
    BindNativeFunction({ "stdout/writeln!", 1, 0 }, FOutWriteLn);
}

Pulsar::RuntimeState PulsarBindings::Std::Stdio::FInRead(Pulsar::ExecutionContext& eContext)
{
    constexpr size_t BUFFER_CAPACITY = 256;
    char buffer[BUFFER_CAPACITY];

    Pulsar::String line;
    do { /* Repeat while count-1 chars extracted */
        std::cin.clear(); // Clear fail bit
        std::cin.getline(buffer, BUFFER_CAPACITY);
        line += buffer;
    } while (std::cin.fail() && !std::cin.eof() && !std::cin.bad());

    eContext.CurrentFrame()
        .Stack.EmplaceString(std::move(line));

    return Pulsar::RuntimeState::OK;
}

Pulsar::RuntimeState PulsarBindings::Std::Stdio::FOutWrite(Pulsar::ExecutionContext& eContext)
{
    Pulsar::Frame& frame = eContext.CurrentFrame();
    Pulsar::Value& message = frame.Locals[0];
    if (message.Type() != Pulsar::ValueType::String)
        return Pulsar::RuntimeState::TypeError;
    std::cout << message.AsString().CString() << std::flush;
    return Pulsar::RuntimeState::OK;
}

Pulsar::RuntimeState PulsarBindings::Std::Stdio::FOutWriteLn(Pulsar::ExecutionContext& eContext)
{
    Pulsar::Frame& frame = eContext.CurrentFrame();
    Pulsar::Value& message = frame.Locals[0];
    if (message.Type() != Pulsar::ValueType::String)
        return Pulsar::RuntimeState::TypeError;
    std::cout << message.AsString().CString() << std::endl;
    return Pulsar::RuntimeState::OK;
}
