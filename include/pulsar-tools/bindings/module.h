#ifndef _PULSARTOOLS_BINDINGS_MODULE_H
#define _PULSARTOOLS_BINDINGS_MODULE_H

#include "pulsar-tools/binding.h"

namespace PulsarTools::Bindings
{
    class Module :
        public IBinding
    {
    public:
        class ModuleType :
            public Pulsar::CustomDataHolder,
            public Pulsar::Module
        {
        public:
            using Ref = Pulsar::SharedRef<ModuleType>;
            using Pulsar::Module::Module;
        };

    public:
        Module();

    public:
        static Pulsar::RuntimeState FFromFile(Pulsar::ExecutionContext& eContext, uint64_t moduleTypeId);
        static Pulsar::RuntimeState FRun(Pulsar::ExecutionContext& eContext, uint64_t moduleTypeId);
        static Pulsar::RuntimeState FIsValid(Pulsar::ExecutionContext& eContext, uint64_t moduleTypeId);
    };
}

#endif // _PULSARTOOLS_BINDINGS_MODULE_H
