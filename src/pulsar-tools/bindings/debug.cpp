#include "pulsar-tools/bindings/debug.h"

#include <string>

#include "pulsar-tools/fmt.h"

PulsarTools::Bindings::Debug::Debug() :
    IBinding()
{
    BindNativeFunction({ "stack-dump!", 0, 0 }, FStackDump);
    BindNativeFunction({ "trace-call!", 0, 0 }, FTraceCall);
}

Pulsar::RuntimeState PulsarTools::Bindings::Debug::FStackDump(Pulsar::ExecutionContext& eContext)
{
    Pulsar::Frame& frame = eContext.GetCallStack().CallingFrame();
    std::string dump("Stack Dump: [");
    for (size_t i = 0; i < frame.Stack.Size(); i++) {
        if (i > 0) dump += ',';
        dump += fmt::format(" {}", frame.Stack[i]);
    }
    dump += " ]";
    fmt::println(stdout, "{}", dump);
    return Pulsar::RuntimeState::OK;
}

Pulsar::RuntimeState PulsarTools::Bindings::Debug::FTraceCall(Pulsar::ExecutionContext& eContext)
{
    Pulsar::String stackTrace = eContext.GetStackTrace(~(size_t)0);
    fmt::println(stdout, "Stack Trace:\n{}", stackTrace);
    return Pulsar::RuntimeState::OK;
}
