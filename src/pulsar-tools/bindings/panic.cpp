#include "pulsar-tools/bindings/panic.h"

void PulsarTools::PanicNativeBindings::BindToModule(Pulsar::Module& module)
{
    module.BindNativeFunction({ "panic!",      0, 0 }, Panic_Panic);
    module.BindNativeFunction({ "panic/type!", 0, 0 }, Panic_Type);
}

Pulsar::RuntimeState PulsarTools::PanicNativeBindings::Panic_Panic(Pulsar::ExecutionContext& eContext)
{
    eContext.CallStack.PopBack();
    return Pulsar::RuntimeState::Error;
}

Pulsar::RuntimeState PulsarTools::PanicNativeBindings::Panic_Type(Pulsar::ExecutionContext& eContext)
{
    eContext.CallStack.PopBack();
    return Pulsar::RuntimeState::TypeError;
}
