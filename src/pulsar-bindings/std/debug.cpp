#include "pulsar-bindings/std/debug.h"

PulsarBindings::Std::Debug::Debug() :
    IBinding()
{
    BindNativeFunction({ "stack-dump!", 0, 0 }, FStackDump);
    BindNativeFunction({ "trace-call!", 0, 0 }, FTraceCall);
}

Pulsar::RuntimeState PulsarBindings::Std::Debug::FStackDump(Pulsar::ExecutionContext& eContext)
{
    Pulsar::Frame& frame = eContext.GetCallStack().CallingFrame();
    Pulsar::String dump("Stack Dump: [");
    for (size_t i = 0; i < frame.Stack.Size(); i++) {
        if (i > 0) dump += ',';
        dump += ' ';
        dump += frame.Stack[i].ToRepr({ .Module = &eContext.GetModule() });
    }
    dump += " ]";
    std::fputs(dump.CString(), stdout);
    return Pulsar::RuntimeState::OK;
}

Pulsar::RuntimeState PulsarBindings::Std::Debug::FTraceCall(Pulsar::ExecutionContext& eContext)
{
    Pulsar::String stackTrace = "Stack Trace:\n";
    stackTrace += eContext.GetStackTrace(~(size_t)0);
    std::fputs(stackTrace.CString(), stdout);
    return Pulsar::RuntimeState::OK;
}
