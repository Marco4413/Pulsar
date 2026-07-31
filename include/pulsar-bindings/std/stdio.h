#ifndef _PULSARBINDINGS_STD_STDIO_H
#define _PULSARBINDINGS_STD_STDIO_H

#include "pulsar-bindings/binding.h"

namespace PulsarBindings::Std
{
    class Stdio : public Binding
    {
    public:
        Stdio();

    public:
        static Pulsar::RuntimeState FInRead(Pulsar::ExecutionContext& eContext);
        static Pulsar::RuntimeState FOutWrite(Pulsar::ExecutionContext& eContext);
        static Pulsar::RuntimeState FOutWriteLn(Pulsar::ExecutionContext& eContext);
    };
}

#endif // _PULSARBINDINGS_STD_STDIO_H
