#ifndef _PULSARTOOLS_BINDINGS_MODULE_H
#define _PULSARTOOLS_BINDINGS_MODULE_H

#include "pulsar-tools/core.h"

#include "pulsar/runtime.h"
#include "pulsar/structures/hashmap.h"

namespace PulsarTools
{
    namespace ModuleNativeBindings
    {
        class ModuleTypeData : public Pulsar::CustomTypeData
        {
        public:
            int64_t NextHandle = 1;
            Pulsar::HashMap<int64_t, Pulsar::Module> Modules;
        };

        void BindToModule(Pulsar::Module& module);

        Pulsar::RuntimeState Module_FromFile(Pulsar::ExecutionContext& eContext, uint64_t type);
        Pulsar::RuntimeState Module_Run(Pulsar::ExecutionContext& eContext, uint64_t type);
        Pulsar::RuntimeState Module_Free(Pulsar::ExecutionContext& eContext, uint64_t type);
        Pulsar::RuntimeState Module_IsValid(Pulsar::ExecutionContext& eContext, uint64_t type);
    }
}

#endif // _PULSARTOOLS_BINDINGS_MODULE_H
