#ifndef _PULSARTOOLS_BINDINGS_TIME_H
#define _PULSARTOOLS_BINDINGS_TIME_H

#include "pulsar-tools/core.h"

#include "pulsar/runtime.h"

namespace PulsarTools
{
    namespace TimeNativeBindings
    {
        void BindToModule(Pulsar::Module& module);

        Pulsar::RuntimeState Time_Time(Pulsar::ExecutionContext& eContext);
        Pulsar::RuntimeState Time_Steady(Pulsar::ExecutionContext& eContext);
        Pulsar::RuntimeState Time_Micros(Pulsar::ExecutionContext& eContext);
    }
}

#endif // _PULSARTOOLS_BINDINGS_TIME_H
