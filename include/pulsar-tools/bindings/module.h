#ifndef _PULSARTOOLS_BINDINGS_MODULE_H
#define _PULSARTOOLS_BINDINGS_MODULE_H

#include <unordered_map>

#include "pulsar-tools/core.h"

#include "pulsar/runtime.h"

namespace PulsarTools
{
    class ModuleNativeBindings
    {
    public:
        ModuleNativeBindings() = default;
        ~ModuleNativeBindings() = default;

        void BindToModule(Pulsar::Module& module);

        Pulsar::RuntimeState Module_FromFile(Pulsar::ExecutionContext& eContext, uint64_t type);
        Pulsar::RuntimeState Module_Run(Pulsar::ExecutionContext& eContext, uint64_t type) const;
        Pulsar::RuntimeState Module_Free(Pulsar::ExecutionContext& eContext, uint64_t type);
        Pulsar::RuntimeState Module_IsValid(Pulsar::ExecutionContext& eContext, uint64_t type);

    private:
        int64_t m_NextHandle = 1;
        std::unordered_map<int64_t, Pulsar::Module> m_Modules;
    };
}

#endif // _PULSARTOOLS_BINDINGS_MODULE_H
