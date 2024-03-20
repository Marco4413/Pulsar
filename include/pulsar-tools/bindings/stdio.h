#ifndef _PULSARTOOLS_BINDINGS_STDIO_H
#define _PULSARTOOLS_BINDINGS_STDIO_H

#include "pulsar-tools/core.h"

#include "pulsar/runtime.h"

namespace PulsarTools
{
    namespace STDIONativeBindings
    {
        void BindToModule(Pulsar::Module& module);

        Pulsar::RuntimeState STDIN_Read(Pulsar::ExecutionContext& eContext);
        Pulsar::RuntimeState STDOUT_Write(Pulsar::ExecutionContext& eContext);
        Pulsar::RuntimeState STDOUT_WriteLn(Pulsar::ExecutionContext& eContext);
    }
}

#endif // _PULSARTOOLS_BINDINGS_STDIO_H
