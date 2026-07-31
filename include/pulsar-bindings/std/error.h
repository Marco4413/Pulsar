#ifndef _PULSARBINDINGS_STD_ERROR_H
#define _PULSARBINDINGS_STD_ERROR_H

#include "pulsar-bindings/binding.h"

namespace PulsarBindings::Std
{
    class Error : public Binding
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

#endif // _PULSARBINDINGS_STD_ERROR_H
