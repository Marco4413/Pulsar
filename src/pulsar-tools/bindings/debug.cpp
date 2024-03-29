#include "pulsar-tools/bindings/debug.h"

#include "pulsar-tools/print.h"

void PulsarTools::DebugNativeBindings::BindToModule(Pulsar::Module& module, bool declare)
{
    if (declare) {
        module.DeclareAndBindNativeFunction({ "stack-dump!", 0, 0 }, Debug_StackDump);
        module.DeclareAndBindNativeFunction({ "trace-call!", 0, 0 }, Debug_TraceCall);
        return;
    }

    module.BindNativeFunction({ "stack-dump!", 0, 0 }, Debug_StackDump);
    module.BindNativeFunction({ "trace-call!", 0, 0 }, Debug_TraceCall);
}

Pulsar::RuntimeState PulsarTools::DebugNativeBindings::Debug_StackDump(Pulsar::ExecutionContext& eContext)
{
    Pulsar::Frame& frame = eContext.CallStack.CallingFrame();
    std::string dump("Stack Dump: [");
    for (size_t i = 0; i < frame.Stack.Size(); i++) {
        if (i > 0) dump += ',';
        dump += fmt::format(" {}", frame.Stack[i]);
    }
    PULSARTOOLS_PRINTF("{} ]\n", dump);
    return Pulsar::RuntimeState::OK;
}

Pulsar::RuntimeState PulsarTools::DebugNativeBindings::Debug_TraceCall(Pulsar::ExecutionContext& eContext)
{
    Pulsar::String stackTrace = eContext.GetStackTrace(~(size_t)0);
    PULSARTOOLS_PRINTF("Stack Trace:\n{}\n", stackTrace);
    return Pulsar::RuntimeState::OK;
}
