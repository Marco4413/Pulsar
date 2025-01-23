#ifndef _PULSARTOOLS_BINDINGS_PRINT_H
#define _PULSARTOOLS_BINDINGS_PRINT_H

#include "pulsar-tools/binding.h"

namespace PulsarTools::Bindings
{
    class Print :
        public IBinding
    {
    public:
        Print();

    public:
        static Pulsar::RuntimeState FPrint(Pulsar::ExecutionContext& eContext);
        static Pulsar::RuntimeState FPrintln(Pulsar::ExecutionContext& eContext);
    };
}

#endif // _PULSARTOOLS_BINDINGS_PRINT_H
