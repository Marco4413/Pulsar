#ifndef _PULSAR_RUNTIME_CUSTOMTYPE_H
#define _PULSAR_RUNTIME_CUSTOMTYPE_H

#include "pulsar/core.h"

#include "pulsar/structures/ref.h"
#include "pulsar/structures/string.h"

namespace Pulsar
{
    // Extend this class to store your custom type's global data
    class CustomTypeGlobalData
    {
    public:
        using Ref = SharedRef<Pulsar::CustomTypeGlobalData>;

        virtual ~CustomTypeGlobalData() = default;
        /**
         * When a new sandboxed ExecutionContext is created from an already existing one
         *  this function is called.
         * This function must return a "copy" of the data which won't affect the original
         *  context.
         * If this function returns nullptr, it means that the data held doesn't require
         *  to be sandboxed, this may be useful for stats tracking.
         * It's advised to make the data thread-safe if it's meant to be shared.
         */
        virtual Ref Fork() const = 0;
    };

    struct CustomType
    {
        using GlobalDataFactoryFn = std::function<CustomTypeGlobalData::Ref()>;

        String Name;
        // Method that generates a new instance of a derived class from CustomTypeGlobalData
        GlobalDataFactoryFn GlobalDataFactory = nullptr;
    };
}

#endif // _PULSAR_RUNTIME_CUSTOMTYPE_H
