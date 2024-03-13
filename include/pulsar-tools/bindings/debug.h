#ifndef _PULSARTOOLS_BINDINGS_DEBUG_H
#define _PULSARTOOLS_BINDINGS_DEBUG_H

#include "pulsar-tools/core.h"

#include "pulsar/runtime.h"

namespace PulsarTools
{
    class DebugNativeBindings
    {
    public:
        DebugNativeBindings() = default;
        ~DebugNativeBindings() = default;

        void BindToModule(Pulsar::Module& module, bool declare = true) const;

        static Pulsar::RuntimeState Debug_StackDump(Pulsar::ExecutionContext& eContext);
    };
}

#endif // _PULSARTOOLS_BINDINGS_DEBUG_H
