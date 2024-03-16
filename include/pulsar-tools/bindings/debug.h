#ifndef _PULSARTOOLS_BINDINGS_DEBUG_H
#define _PULSARTOOLS_BINDINGS_DEBUG_H

#include "pulsar-tools/core.h"

#include "pulsar/runtime.h"

namespace PulsarTools
{
    namespace DebugNativeBindings
    {
        void BindToModule(Pulsar::Module& module, bool declare = true);

        Pulsar::RuntimeState Debug_StackDump(Pulsar::ExecutionContext& eContext);
        Pulsar::RuntimeState Debug_TraceCall(Pulsar::ExecutionContext& eContext);
    }
}

#endif // _PULSARTOOLS_BINDINGS_DEBUG_H
