#ifndef _PULSARTOOLS_BINDINGS_MODULE_H
#define _PULSARTOOLS_BINDINGS_MODULE_H

#include "pulsar-tools/core.h"

#include "pulsar/runtime.h"
#include "pulsar/structures/hashmap.h"

namespace PulsarTools
{
    namespace ModuleNativeBindings
    {
        class ModuleType : public Pulsar::CustomDataHolder, public Pulsar::Module
        {
        public:
            using Ref_T = Pulsar::SharedRef<ModuleType>;
            using Pulsar::Module::Module;
        };

        void BindToModule(Pulsar::Module& module);

        Pulsar::RuntimeState Module_FromFile(Pulsar::ExecutionContext& eContext, uint64_t type);
        Pulsar::RuntimeState Module_Run(Pulsar::ExecutionContext& eContext, uint64_t type);
        Pulsar::RuntimeState Module_IsValid(Pulsar::ExecutionContext& eContext, uint64_t type);
    }
}

#endif // _PULSARTOOLS_BINDINGS_MODULE_H
