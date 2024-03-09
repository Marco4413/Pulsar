#ifndef _PULSARTOOLS_BINDINGS_PRINT_H
#define _PULSARTOOLS_BINDINGS_PRINT_H

#include <unordered_map>

#include "pulsar-tools/core.h"

#include "pulsar/runtime.h"

namespace PulsarTools
{
    class PrintNativeBindings
    {
    public:
        PrintNativeBindings() = default;
        ~PrintNativeBindings() = default;

        void BindToModule(Pulsar::Module& module) const;

        static Pulsar::RuntimeState Print_HelloFromCpp(Pulsar::ExecutionContext& eContext);
        static Pulsar::RuntimeState Print_Print(Pulsar::ExecutionContext& eContext);
        static Pulsar::RuntimeState Print_Println(Pulsar::ExecutionContext& eContext);
    };
}

#endif // _PULSARTOOLS_BINDINGS_PRINT_H
