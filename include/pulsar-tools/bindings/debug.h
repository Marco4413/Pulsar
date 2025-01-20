#ifndef _PULSARTOOLS_BINDINGS_DEBUG_H
#define _PULSARTOOLS_BINDINGS_DEBUG_H

#include "pulsar-tools/binding.h"

namespace PulsarTools::Bindings
{
    class Debug :
        public IBinding
    {
    public:
        Debug();

    public:
        static Pulsar::RuntimeState FStackDump(Pulsar::ExecutionContext& eContext);
        static Pulsar::RuntimeState FTraceCall(Pulsar::ExecutionContext& eContext);
    };
}

#endif // _PULSARTOOLS_BINDINGS_DEBUG_H
