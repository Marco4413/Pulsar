#include "pulsar-tools/bindings/print.h"

#include <fmt/base.h>

#include "pulsar-tools/fmt.h"

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
    fmt::print(stdout, "{}", val.ToString({ .Module = &eContext.GetModule() }));
    return Pulsar::RuntimeState::OK;
}

Pulsar::RuntimeState PulsarTools::Bindings::Print::FPrintln(Pulsar::ExecutionContext& eContext)
{
    Pulsar::Frame& frame = eContext.CurrentFrame();
    Pulsar::Value& val = frame.Locals[0];
    fmt::println(stdout, "{}", val.ToString({ .Module = &eContext.GetModule() }));
    return Pulsar::RuntimeState::OK;
}
