#ifndef _PULSARBINDINGS_BINDINGSREGISTER_H
#define _PULSARBINDINGS_BINDINGSREGISTER_H

#include "pulsar/structures/list.h"
#include "pulsar/structures/ref.h"

#include "pulsar-bindings/binding.h"

namespace PulsarBindings
{
    class BindingsRegister final : public Binding
    {
    public:
        BindingsRegister() = default;
        virtual ~BindingsRegister() = default;

        template<typename T, typename ...Args>
        T& Add(Args&& ...args);
    };
}

template<typename T, typename ...Args>
T& PulsarBindings::BindingsRegister::Add(Args&& ...args)
{
    return CreateDependency<T>(std::forward<Args>(args)...);
}

#endif // _PULSARBINDINGS_BINDINGSREGISTER_H
