#ifndef _PULSARTOOLS_BINDING_H
#define _PULSARTOOLS_BINDING_H

#include <functional>
#include <optional>
#include <vector>

#include "pulsar/runtime.h"

namespace PulsarTools
{
    class CustomTypeResolver
    {
    public:
        CustomTypeResolver(Pulsar::Module& module)
            : m_Module(module) {}

        std::optional<uint64_t> ResolveType(const Pulsar::String& typeName) const;

    private:
        Pulsar::Module& m_Module;
    };

    class IBinding
    {
    public:
        using NativeFunction = Pulsar::Module::NativeFunction;
        using NativeFunctionFactoryFn = std::function<NativeFunction(const CustomTypeResolver&)>;

        template<typename ...Types>
        using ExpandedNativeFunction = std::function<Pulsar::RuntimeState(Pulsar::ExecutionContext&, Types...)>;

        struct NativeFunctionBinding
        {
            Pulsar::FunctionDefinition Definition;
            NativeFunctionFactoryFn CreateFunction;
        };

    public:
        IBinding() = default;
        virtual ~IBinding() = default;

        void BindAll(Pulsar::Module& module, bool declareAndBind=false) const
        {
            BindTypes(module);
            BindFunctions(module, declareAndBind);
        }

        void BindTypes(Pulsar::Module& module) const;
        void BindFunctions(Pulsar::Module& module, bool declareAndBind) const;

    public:
        // TODO: These may become a template, I'm not smart enough to do that though.

        // These methods will wrap native functions that take 1 or 2 type ids.
        // Those Ids will be resolved at bind time.
        static NativeFunctionFactoryFn CreateMonoTypeBoundFactory(
                ExpandedNativeFunction<uint64_t> nativeFn,
                Pulsar::String type1);

        static NativeFunctionFactoryFn CreateBiTypeBoundFactory(
                ExpandedNativeFunction<uint64_t, uint64_t> nativeFn,
                Pulsar::String type1, Pulsar::String type2);

    protected:
        void BindCustomType(const Pulsar::String& name, Pulsar::CustomType::DataFactoryFn dataFactory = nullptr)
        {
            m_CustomTypesPool.emplace_back(name, dataFactory);
        }

        void BindNativeFunction(const Pulsar::FunctionDefinition& def, NativeFunctionFactoryFn factory)
        {
            m_NativeFunctionsPool.emplace_back(def, factory);
        }

        void BindNativeFunction(const Pulsar::FunctionDefinition& def, NativeFunction func)
        {
            BindNativeFunction(def, [func = std::move(func)](const CustomTypeResolver&) { return func; });
        }

    private:
        std::vector<Pulsar::CustomType> m_CustomTypesPool;
        std::vector<NativeFunctionBinding> m_NativeFunctionsPool;
    };
}

#endif // _PULSARTOOLS_BINDING_H
