#include "pulsar-tools/bindings/print.h"

#include "pulsar-tools/print.h"

void PulsarTools::PrintNativeBindings::BindToModule(Pulsar::Module& module)
{
    module.BindNativeFunction({ "print!",   1, 0 }, Print_Print);
    module.BindNativeFunction({ "println!", 1, 0 }, Print_Println);
}

Pulsar::RuntimeState PulsarTools::PrintNativeBindings::Print_Print(Pulsar::ExecutionContext& eContext)
{
    Pulsar::Frame& frame = eContext.CurrentFrame();
    Pulsar::Value& val = frame.Locals[0];
    if (val.Type() == Pulsar::ValueType::String)
        PULSARTOOLS_PRINTF("{}", val.AsString());
    else PULSARTOOLS_PRINTF("{}", val);
    return Pulsar::RuntimeState::OK;
}

Pulsar::RuntimeState PulsarTools::PrintNativeBindings::Print_Println(Pulsar::ExecutionContext& eContext)
{
    Pulsar::Frame& frame = eContext.CurrentFrame();
    Pulsar::Value& val = frame.Locals[0];
    if (val.Type() == Pulsar::ValueType::String)
        PULSARTOOLS_PRINTF("{}\n", val.AsString());
    else PULSARTOOLS_PRINTF("{}\n", val);
    return Pulsar::RuntimeState::OK;
}
