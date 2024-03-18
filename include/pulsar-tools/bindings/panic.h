#ifndef _PULSARTOOLS_BINDINGS_PANIC_H
#define _PULSARTOOLS_BINDINGS_PANIC_H

#include "pulsar-tools/core.h"

#include "pulsar/runtime.h"

namespace PulsarTools
{
    namespace PanicNativeBindings
    {
        void BindToModule(Pulsar::Module& module);

        Pulsar::RuntimeState Panic_Panic(Pulsar::ExecutionContext& eContext);
        Pulsar::RuntimeState Panic_Type(Pulsar::ExecutionContext& eContext);
    }
}

#endif // _PULSARTOOLS_BINDINGS_PANIC_H
