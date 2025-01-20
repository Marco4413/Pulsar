#include "pulsar-tools/binding.h"

std::optional<uint64_t> PulsarTools::CustomTypeResolver::ResolveType(const Pulsar::String& typeName) const
{
    std::optional<uint64_t> result = std::nullopt;
    m_Module.CustomTypes.ForEach([&result, &typeName](const auto& idTypePair) {
        if (!result && idTypePair.Value().Name == typeName) {
            result = idTypePair.Key();
        }
    });
    return result;
}

void PulsarTools::IBinding::BindTypes(Pulsar::Module& module) const
{
    for (const auto& customType : m_CustomTypesPool) {
        module.BindCustomType(customType.Name, customType.DataFactory);
    }
}

void PulsarTools::IBinding::BindFunctions(Pulsar::Module& module, bool declareAndBind) const
{
    CustomTypeResolver typeResolver(module);
    if (declareAndBind) {
        for (const auto& nativeFnBinding : m_NativeFunctionsPool) {
            module.DeclareAndBindNativeFunction(
                    nativeFnBinding.Definition,
                    nativeFnBinding.CreateFunction(typeResolver));
        }
    } else {
        for (const auto& nativeFnBinding : m_NativeFunctionsPool) {
            module.BindNativeFunction(
                    nativeFnBinding.Definition,
                    nativeFnBinding.CreateFunction(typeResolver));
        }
    }
}

PulsarTools::IBinding::NativeFunctionFactoryFn PulsarTools::IBinding::CreateMonoTypeBoundFactory(
        ExpandedNativeFunction<uint64_t> nativeFn,
        Pulsar::String type1)
{
    return [nativeFn, type1 = std::move(type1)](const auto& resolver)
        -> PulsarTools::IBinding::NativeFunction
    {
        auto typeId1Opt = resolver.ResolveType(type1);
        PULSAR_ASSERT(typeId1Opt, "Call to CreateMonoTypeBoundFactory tried to access invalid type1.");
        return [nativeFn, typeId1 = *typeId1Opt](Pulsar::ExecutionContext& context) {
            return nativeFn(context, typeId1);
        };
    };
}

PulsarTools::IBinding::NativeFunctionFactoryFn PulsarTools::IBinding::CreateBiTypeBoundFactory(
        ExpandedNativeFunction<uint64_t, uint64_t> nativeFn,
        Pulsar::String type1, Pulsar::String type2)
{
    return [nativeFn, type1 = std::move(type1), type2 = std::move(type2)](const auto& resolver)
        -> PulsarTools::IBinding::NativeFunction
    {
        auto typeId1Opt = resolver.ResolveType(type1);
        PULSAR_ASSERT(typeId1Opt, "Call to CreateBiTypeBoundFactory tried to access invalid type1.");
        auto typeId2Opt = resolver.ResolveType(type2);
        PULSAR_ASSERT(typeId2Opt, "Call to CreateBiTypeBoundFactory tried to access invalid type2.");
        return [nativeFn, typeId1 = *typeId1Opt, typeId2 = *typeId2Opt](Pulsar::ExecutionContext& context) {
            return nativeFn(context, typeId1, typeId2);
        };
    };
}
