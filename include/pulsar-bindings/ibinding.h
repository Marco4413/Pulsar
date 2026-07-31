#ifndef _PULSARBINDINGS_IBINDING_H
#define _PULSARBINDINGS_IBINDING_H

#include "pulsar/runtime/module.h"

namespace PulsarBindings
{
    /**
     * An instance of this class MAY be bound to multiple modules.
     * Any instance of this class MUST be fixed in memory.
     * Any instance of this class MUST outlive all modules it was bound to.
     */
    class IBinding
    {
    public:
        IBinding() = default;
        virtual ~IBinding() = default;

        void BindAll(Pulsar::Module& module) const
        {
            BindTypes(module);
            BindFunctions(module);
        }

        virtual void BindTypes(Pulsar::Module& module) const = 0;
        virtual void BindFunctions(Pulsar::Module& module) const = 0;
    };
}

#endif // _PULSARBINDINGS_IBINDING_H
