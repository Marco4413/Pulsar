#include "pulsar-tools/bindings/print.h"

#include "pulsar-tools/fmt.h"

#include "fmt/base.h"

PulsarTools::Bindings::Print::Print() :
    IBinding()
{
    BindNativeFunction({ "print!",   1, 0 }, FPrint);
    BindNativeFunction({ "println!", 1, 0 }, FPrintln);
}

Pulsar::RuntimeState PulsarTools::Bindings::Print::FPrint(Pulsar::ExecutionContext& eContext)
{
    Pulsar::Frame& frame = eContext.CurrentFrame();
    Pulsar::Value& val = frame.Locals[0];
    if (val.Type() == Pulsar::ValueType::String)
        fmt::print("{}", val.AsString());
    else fmt::print("{}", val);
    return Pulsar::RuntimeState::OK;
}

Pulsar::RuntimeState PulsarTools::Bindings::Print::FPrintln(Pulsar::ExecutionContext& eContext)
{
    Pulsar::Frame& frame = eContext.CurrentFrame();
    Pulsar::Value& val = frame.Locals[0];
    if (val.Type() == Pulsar::ValueType::String)
        fmt::println(stdout, "{}", val.AsString());
    else fmt::println(stdout, "{}", val);
    return Pulsar::RuntimeState::OK;
}
