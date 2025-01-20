#ifndef _PULSARTOOLS_BINDINGS_ERROR_H
#define _PULSARTOOLS_BINDINGS_ERROR_H

#include "pulsar-tools/binding.h"

namespace PulsarTools::Bindings
{
    class Error :
        public IBinding
    {
    public:
        Error();

    public:
        static Pulsar::RuntimeState FError(Pulsar::ExecutionContext& eContext);
        static Pulsar::RuntimeState FType(Pulsar::ExecutionContext& eContext);
        static Pulsar::RuntimeState FSafeCall(Pulsar::ExecutionContext& eContext);
        static Pulsar::RuntimeState FTryCall(Pulsar::ExecutionContext& eContext);
    };
}

#endif // _PULSARTOOLS_BINDINGS_ERROR_H
