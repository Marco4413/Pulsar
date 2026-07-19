#include "pulsar-bindings/binding.h"

std::optional<uint64_t> PulsarBindings::CustomTypeResolver::ResolveType(const Pulsar::String& typeName) const
{
    std::optional<uint64_t> result = std::nullopt;
    m_Module.CustomTypes.ForEach([&result, &typeName](const auto& idTypePair) {
        if (!result && idTypePair.Value().Name == typeName) {
            result = idTypePair.Key();
        }
    });
    return result;
}

void PulsarBindings::IBinding::BindTypes(Pulsar::Module& module) const
{
    for (const auto& dep : m_Dependencies) {
        dep->BindTypes(module);
    }
    for (const auto& customType : m_CustomTypesPool) {
        module.BindCustomType(customType.Name, customType.GlobalDataFactory);
    }
}

void PulsarBindings::IBinding::BindFunctions(Pulsar::Module& module, bool declareAndBind) const
{
    for (const auto& dep : m_Dependencies) {
        dep->BindFunctions(module, declareAndBind);
    }

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
