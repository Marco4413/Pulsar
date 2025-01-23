#ifndef _PULSARTOOLS_BINDINGS_FILESYSTEM_H
#define _PULSARTOOLS_BINDINGS_FILESYSTEM_H

#include "pulsar-tools/binding.h"

namespace PulsarTools::Bindings
{
    class FileSystem :
        public IBinding
    {
    public:
        FileSystem();

    public:
        static Pulsar::RuntimeState FExists(Pulsar::ExecutionContext& eContext);
        static Pulsar::RuntimeState FReadAll(Pulsar::ExecutionContext& eContext);
    };
}

#endif // _PULSARTOOLS_BINDINGS_FILESYSTEM_H
