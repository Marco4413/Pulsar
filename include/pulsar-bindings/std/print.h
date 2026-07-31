#ifndef _PULSARBINDINGS_STD_PRINT_H
#define _PULSARBINDINGS_STD_PRINT_H

#include "pulsar-bindings/binding.h"

namespace PulsarBindings::Std
{
    class Print : public Binding
    {
    public:
        Print();

    public:
        static Pulsar::RuntimeState FPrint(Pulsar::ExecutionContext& eContext);
        static Pulsar::RuntimeState FPrintln(Pulsar::ExecutionContext& eContext);
    };
}

#endif // _PULSARBINDINGS_STD_PRINT_H
