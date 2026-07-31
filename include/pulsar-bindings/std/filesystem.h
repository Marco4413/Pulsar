#ifndef _PULSARBINDINGS_STD_FILESYSTEM_H
#define _PULSARBINDINGS_STD_FILESYSTEM_H

#include "pulsar-bindings/binding.h"

namespace PulsarBindings::Std
{
    class FileSystem : public Binding
    {
    public:
        FileSystem();

    public:
        static Pulsar::RuntimeState FExists(Pulsar::ExecutionContext& eContext);
        static Pulsar::RuntimeState FReadAll(Pulsar::ExecutionContext& eContext);
    };
}

#endif // _PULSARBINDINGS_STD_FILESYSTEM_H
