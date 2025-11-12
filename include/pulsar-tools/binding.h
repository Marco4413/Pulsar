#ifndef _PULSARTOOLS_BINDING_H
#define _PULSARTOOLS_BINDING_H

#include <array>
#include <functional>
#include <optional>
#include <type_traits>
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

        struct NativeFunctionBinding
        {
            Pulsar::FunctionDefinition Definition;
            NativeFunctionFactoryFn CreateFunction;
        };

    public:
        IBinding() = default;
        virtual ~IBinding();

        void BindAll(Pulsar::Module& module, bool declareAndBind=false) const
        {
            BindTypes(module);
            BindFunctions(module, declareAndBind);
        }

        void BindTypes(Pulsar::Module& module) const;
        void BindFunctions(Pulsar::Module& module, bool declareAndBind) const;

    public:
        /**
         * Given a function and some type names, this function creates a NativeFunctionFactoryFn
         * that will generate a NativeFunction which calls nativeFn with the ExecutionContext
         * and the resolved type ids of the specified type names.
         * 
         * See Thread bindings for examples.
         */
        template<typename Fn, typename ...Args>
        static NativeFunctionFactoryFn CreateTypeBoundFactory(Fn nativeFn, Args&& ..._typeNames)
        {
            std::array<Pulsar::String, sizeof...(Args)> typeNames{ _typeNames... };
            return CreateTypeBoundFactoryImpl(std::forward<Fn>(nativeFn), std::move(typeNames), std::make_index_sequence<sizeof...(Args)>{});
        }

        template<typename Fn, size_t N, size_t ...Is>
        static NativeFunctionFactoryFn CreateTypeBoundFactoryImpl(
                Fn nativeFn,
                std::array<Pulsar::String, N>&& typeNames,
                std::index_sequence<Is...>)
        {
            return [nativeFn = std::move(nativeFn), typeNames = std::move(typeNames)](const CustomTypeResolver& typeResolver) {
                std::array<uint64_t, N> typeIds;
                ([&typeNames, &typeResolver, &typeIds](size_t idx) {
                    auto typeId = typeResolver.ResolveType(typeNames[idx]);
                    PULSAR_ASSERT(typeId, "Trying to access invalid type.");
                    typeIds[idx] = *typeId;
                }(Is), ...);

                return [nativeFn = std::move(nativeFn), typeIds = std::move(typeIds)](Pulsar::ExecutionContext& eContext) {
                    return nativeFn(eContext, typeIds[Is]...);
                };
            };
        }

    protected:
        // Dependencies are IBindings which are bound before this
        template<typename T>
        void AddDependency(T&& dep)
            requires(std::is_base_of_v<IBinding, T>)
        {
            T* depPtr = new T(std::forward<T>(dep));
            PULSAR_ASSERT(depPtr, "Failed to allocate memory.");
            m_Dependencies.push_back(depPtr);
        }

        void BindCustomType(Pulsar::StringView name, Pulsar::CustomType::GlobalDataFactoryFn globalDataFactory=nullptr)
        {
            m_CustomTypesPool.emplace_back(name.ToString(), globalDataFactory);
        }

        void BindNativeFunction(const Pulsar::FunctionSignature& sig, NativeFunctionFactoryFn factory)
        {
            m_NativeFunctionsPool.emplace_back(std::forward<Pulsar::FunctionDefinition>(sig.ToNativeDefinition()), factory);
        }

        void BindNativeFunction(const Pulsar::FunctionSignature& sig, NativeFunction func)
        {
            BindNativeFunction(sig, [func = std::move(func)](const CustomTypeResolver&) { return func; });
        }

    private:
        std::vector<IBinding*> m_Dependencies;
        std::vector<Pulsar::CustomType> m_CustomTypesPool;
        std::vector<NativeFunctionBinding> m_NativeFunctionsPool;
    };
}

#endif // _PULSARTOOLS_BINDING_H
