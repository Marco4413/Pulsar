#ifndef _PULSARBINDINGS_STD_MODULE_H
#define _PULSARBINDINGS_STD_MODULE_H

#include "pulsar-bindings/binding.h"

namespace PulsarBindings::Std
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

#endif // _PULSARBINDINGS_STD_MODULE_H
