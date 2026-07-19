#include "pulsar-bindings/std/print.h"

PulsarBindings::Std::Print::Print() :
    IBinding()
{
    BindNativeFunction({ "print!",   1, 0 }, FPrint);
    BindNativeFunction({ "println!", 1, 0 }, FPrintln);
}

Pulsar::RuntimeState PulsarBindings::Std::Print::FPrint(Pulsar::ExecutionContext& eContext)
{
    Pulsar::Frame& frame = eContext.CurrentFrame();
    Pulsar::Value& val = frame.Locals[0];
    Pulsar::String out = val.ToString({ .Module = &eContext.GetModule() });
    fputs(out.CString(), stdout);
    return Pulsar::RuntimeState::OK;
}

Pulsar::RuntimeState PulsarBindings::Std::Print::FPrintln(Pulsar::ExecutionContext& eContext)
{
    Pulsar::Frame& frame = eContext.CurrentFrame();
    Pulsar::Value& val = frame.Locals[0];
    Pulsar::String out = val.ToString({ .Module = &eContext.GetModule() });
    out += '\n';
    fputs(out.CString(), stdout);
    return Pulsar::RuntimeState::OK;
}
