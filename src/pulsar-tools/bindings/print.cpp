#include "pulsar-tools/bindings/print.h"

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
    fputs(std::format("{}", val.ToString({ .Module = &eContext.GetModule() })).c_str(), stdout);
    return Pulsar::RuntimeState::OK;
}

Pulsar::RuntimeState PulsarTools::Bindings::Print::FPrintln(Pulsar::ExecutionContext& eContext)
{
    Pulsar::Frame& frame = eContext.CurrentFrame();
    Pulsar::Value& val = frame.Locals[0];
    fputs(std::format("{}\n", val.ToString({ .Module = &eContext.GetModule() })).c_str(), stdout);
    return Pulsar::RuntimeState::OK;
}
