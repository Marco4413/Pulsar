#ifndef _PULSARBINDINGS_STD_DEBUG_H
#define _PULSARBINDINGS_STD_DEBUG_H

#include "pulsar-bindings/binding.h"

namespace PulsarBindings::Std
{
    class Debug : public Binding
    {
    public:
        Debug();

    public:
        static Pulsar::RuntimeState FStackDump(Pulsar::ExecutionContext& eContext);
        static Pulsar::RuntimeState FTraceCall(Pulsar::ExecutionContext& eContext);
    };
}

#endif // _PULSARBINDINGS_STD_DEBUG_H
