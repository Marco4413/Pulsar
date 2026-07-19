#ifndef _PULSARBINDINGS_STD_TIME_H
#define _PULSARBINDINGS_STD_TIME_H

#include "pulsar-bindings/binding.h"

namespace PulsarBindings::Std
{
    class Time :
        public IBinding
    {
    public:
        Time();

    public:
        static Pulsar::RuntimeState FTime(Pulsar::ExecutionContext& eContext);
        static Pulsar::RuntimeState FSteady(Pulsar::ExecutionContext& eContext);
        static Pulsar::RuntimeState FMicros(Pulsar::ExecutionContext& eContext);
    };
}

#endif // _PULSARBINDINGS_STD_TIME_H
