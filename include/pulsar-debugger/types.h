#ifndef _PULSARDEBUGGER_TYPES_H
#define _PULSARDEBUGGER_TYPES_H

#include <pulsar/runtime.h>

namespace PulsarDebugger
{
    using Id                = int64_t;
    using ThreadId          = Id;
    using FrameId           = Id;
    using ScopeId           = Id;

    using Reference          = int64_t;
    using VariablesReference = Reference;
    using SourceReference    = Reference;

    constexpr Reference NULL_REFERENCE = 0;

    // Thread Ids are based on the address of their ExecutionContext.
    // So make sure not to move it in memory to keep the ThreadId valid.
    ThreadId ComputeThreadId(const Pulsar::ExecutionContext& thread);
}

#endif // _PULSARDEBUGGER_TYPES_H
