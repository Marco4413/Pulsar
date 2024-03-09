#include "pulsar-tools/bindings/debug.h"

#include "pulsar-tools/print.h"

void PulsarTools::DebugNativeBindings::BindToModule(Pulsar::Module& module, bool declare) const
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
    fmt::print("Stack Dump: [");
    for (size_t i = 0; i < frame.Stack.Size(); i++) {
        if (i > 0) fmt::print(",");
        fmt::print(" {}", frame.Stack[i]);
    }
    fmt::println(" ]");
    return Pulsar::RuntimeState::OK;
}
