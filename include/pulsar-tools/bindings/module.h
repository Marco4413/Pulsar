#ifndef _PULSARTOOLS_BINDINGS_MODULE_H
#define _PULSARTOOLS_BINDINGS_MODULE_H

#include <memory>
#include <mutex>

#include "pulsar-tools/core.h"

#include "pulsar/runtime.h"
#include "pulsar/structures/hashmap.h"

namespace PulsarTools
{
    class ModuleNativeBindings
    {
    public:
        ModuleNativeBindings() = default;
        ~ModuleNativeBindings() = default;

        void BindToModule(Pulsar::Module& module);

        Pulsar::RuntimeState Module_FromFile(Pulsar::ExecutionContext& eContext, uint64_t type);
        Pulsar::RuntimeState Module_Run(Pulsar::ExecutionContext& eContext, uint64_t type);
        Pulsar::RuntimeState Module_Free(Pulsar::ExecutionContext& eContext, uint64_t type);
        Pulsar::RuntimeState Module_IsValid(Pulsar::ExecutionContext& eContext, uint64_t type);

    private:
        std::mutex m_Mutex;
        int64_t m_NextHandle = 1;
        Pulsar::HashMap<int64_t, std::shared_ptr<Pulsar::Module>> m_Modules;
    };
}

#endif // _PULSARTOOLS_BINDINGS_MODULE_H
