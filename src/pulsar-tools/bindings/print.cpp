#include "pulsar-tools/bindings/print.h"

#include "pulsar-tools/print.h"

void PulsarTools::PrintNativeBindings::BindToModule(Pulsar::Module& module) const
{
    module.BindNativeFunction({ "hello-from-cpp!", 0, 0 }, Print_HelloFromCpp);
    module.BindNativeFunction({ "print!",   1, 0 }, Print_Print);
    module.BindNativeFunction({ "println!", 1, 0 }, Print_Println);
}

Pulsar::RuntimeState PulsarTools::PrintNativeBindings::Print_HelloFromCpp(Pulsar::ExecutionContext& eContext)
{
    (void) eContext;
    PULSARTOOLS_PRINTF("Hello from C++!\n");
    return Pulsar::RuntimeState::OK;
}

Pulsar::RuntimeState PulsarTools::PrintNativeBindings::Print_Print(Pulsar::ExecutionContext& eContext)
{
    Pulsar::Frame& frame = eContext.CallStack.CurrentFrame();
    Pulsar::Value& val = frame.Locals[0];
    if (val.Type() == Pulsar::ValueType::String)
        PULSARTOOLS_PRINTF("{}", val.AsString());
    else PULSARTOOLS_PRINTF("{}", val);
    return Pulsar::RuntimeState::OK;
}

Pulsar::RuntimeState PulsarTools::PrintNativeBindings::Print_Println(Pulsar::ExecutionContext& eContext)
{
    Pulsar::Frame& frame = eContext.CallStack.CurrentFrame();
    Pulsar::Value& val = frame.Locals[0];
    if (val.Type() == Pulsar::ValueType::String)
        PULSARTOOLS_PRINTF("{}\n", val.AsString());
    else PULSARTOOLS_PRINTF("{}\n", val);
    return Pulsar::RuntimeState::OK;
}
