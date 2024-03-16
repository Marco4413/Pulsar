#include "pulsar-tools/bindings/debug.h"

#include "pulsar-tools/print.h"

void PulsarTools::DebugNativeBindings::BindToModule(Pulsar::Module& module, bool declare)
{
    if (declare) {
        module.DeclareAndBindNativeFunction({ "stack-dump!", 0, 0 }, Debug_StackDump);
        return;
    }

    module.BindNativeFunction({ "stack-dump!", 0, 0 }, Debug_StackDump);
}

Pulsar::RuntimeState PulsarTools::DebugNativeBindings::Debug_StackDump(Pulsar::ExecutionContext& eContext)
{
    Pulsar::Frame& frame = eContext.CallStack.CallingFrame();
    PULSARTOOLS_PRINTF("Stack Dump: [");
    for (size_t i = 0; i < frame.Stack.Size(); i++) {
        if (i > 0) PULSARTOOLS_PRINTF(",");
        PULSARTOOLS_PRINTF(" {}", frame.Stack[i]);
    }
    PULSARTOOLS_PRINTF(" ]\n");
    return Pulsar::RuntimeState::OK;
}
