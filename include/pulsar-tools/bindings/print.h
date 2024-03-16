#ifndef _PULSARTOOLS_BINDINGS_PRINT_H
#define _PULSARTOOLS_BINDINGS_PRINT_H

#include "pulsar-tools/core.h"

#include "pulsar/runtime.h"

namespace PulsarTools
{
    namespace PrintNativeBindings
    {
        void BindToModule(Pulsar::Module& module);

        Pulsar::RuntimeState Print_HelloFromCpp(Pulsar::ExecutionContext& eContext);
        Pulsar::RuntimeState Print_Print(Pulsar::ExecutionContext& eContext);
        Pulsar::RuntimeState Print_Println(Pulsar::ExecutionContext& eContext);
    }
}

#endif // _PULSARTOOLS_BINDINGS_PRINT_H
