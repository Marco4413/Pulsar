#include "pulsar-tools/bindings/stdio.h"

#include <iostream>

void PulsarTools::STDIONativeBindings::BindToModule(Pulsar::Module& module)
{
    module.BindNativeFunction({ "stdin/read!",     0, 1 }, STDIN_Read);
    module.BindNativeFunction({ "stdout/write!",   1, 0 }, STDOUT_Write);
    module.BindNativeFunction({ "stdout/writeln!", 1, 0 }, STDOUT_WriteLn);
}

Pulsar::RuntimeState PulsarTools::STDIONativeBindings::STDIN_Read(Pulsar::ExecutionContext& eContext)
{
    constexpr size_t BUF_CAP = 256;
    char buf[BUF_CAP];
    Pulsar::String line;
    do { /* Repeat while count-1 chars extracted */
        std::cin.clear(); // Clear fail bit
        std::cin.getline(buf, BUF_CAP);
        line += buf;
    } while (std::cin.fail() && !std::cin.eof() && !std::cin.bad());
    eContext.CallStack.CurrentFrame()
        .Stack.EmplaceBack()
        .SetString(std::move(line));
    return Pulsar::RuntimeState::OK;
}

Pulsar::RuntimeState PulsarTools::STDIONativeBindings::STDOUT_Write(Pulsar::ExecutionContext& eContext)
{
    Pulsar::Frame& frame = eContext.CallStack.CurrentFrame();
    Pulsar::Value& str = frame.Locals[0];
    if (str.Type() != Pulsar::ValueType::String)
        return Pulsar::RuntimeState::TypeError;
    std::cout << str.AsString().Data();
    return Pulsar::RuntimeState::OK;
}

Pulsar::RuntimeState PulsarTools::STDIONativeBindings::STDOUT_WriteLn(Pulsar::ExecutionContext& eContext)
{
    Pulsar::Frame& frame = eContext.CallStack.CurrentFrame();
    Pulsar::Value& str = frame.Locals[0];
    if (str.Type() != Pulsar::ValueType::String)
        return Pulsar::RuntimeState::TypeError;
    std::cout << str.AsString().Data() << std::endl;
    return Pulsar::RuntimeState::OK;
}
