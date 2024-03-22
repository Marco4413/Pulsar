#ifndef _PULSARTOOLS_BINDINGS_ERROR_H
#define _PULSARTOOLS_BINDINGS_ERROR_H

#include "pulsar-tools/core.h"

#include "pulsar/runtime.h"

namespace PulsarTools
{
    namespace ErrorNativeBindings
    {
        void BindToModule(Pulsar::Module& module);

        Pulsar::RuntimeState Error_Error(Pulsar::ExecutionContext& eContext);
        Pulsar::RuntimeState Error_Type(Pulsar::ExecutionContext& eContext);
        Pulsar::RuntimeState Error_SafeCall(Pulsar::ExecutionContext& eContext);
        Pulsar::RuntimeState Error_TryCall(Pulsar::ExecutionContext& eContext);
    }
}

#endif // _PULSARTOOLS_BINDINGS_ERROR_H
