#ifndef _PULSARTOOLS_BINDINGS_TIME_H
#define _PULSARTOOLS_BINDINGS_TIME_H

#include "pulsar-tools/binding.h"

namespace PulsarTools::Bindings
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

#endif // _PULSARTOOLS_BINDINGS_TIME_H
