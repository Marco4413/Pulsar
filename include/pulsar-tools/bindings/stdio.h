#ifndef _PULSARTOOLS_BINDINGS_STDIO_H
#define _PULSARTOOLS_BINDINGS_STDIO_H

#include "pulsar-tools/binding.h"

namespace PulsarTools::Bindings
{
    class Stdio :
        public IBinding
    {
    public:
        Stdio();

    public:
        static Pulsar::RuntimeState FInRead(Pulsar::ExecutionContext& eContext);
        static Pulsar::RuntimeState FOutWrite(Pulsar::ExecutionContext& eContext);
        static Pulsar::RuntimeState FOutWriteLn(Pulsar::ExecutionContext& eContext);
    };
}

#endif // _PULSARTOOLS_BINDINGS_STDIO_H
