#ifndef _PULSARTOOLS_BINDINGS_FILESYSTEM_H
#define _PULSARTOOLS_BINDINGS_FILESYSTEM_H

#include "pulsar-tools/core.h"

#include "pulsar/runtime.h"

namespace PulsarTools
{
    namespace FileSystemNativeBindings
    {
        void BindToModule(Pulsar::Module& module);

        Pulsar::RuntimeState FS_Exists(Pulsar::ExecutionContext& eContext);
        Pulsar::RuntimeState FS_ReadAll(Pulsar::ExecutionContext& eContext);
    }
}

#endif // _PULSARTOOLS_BINDINGS_FILESYSTEM_H
