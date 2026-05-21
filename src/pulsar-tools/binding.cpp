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

PulsarTools::IBinding::~IBinding()
{
    for (IBinding* dep : m_Dependencies)
        delete dep;
}

void PulsarTools::IBinding::BindTypes(Pulsar::Module& module) const
{
    for (IBinding* dep : m_Dependencies) {
        dep->BindTypes(module);
    }
    for (const auto& customType : m_CustomTypesPool) {
        module.BindCustomType(customType.Name, customType.GlobalDataFactory);
    }
}

void PulsarTools::IBinding::BindFunctions(Pulsar::Module& module, bool declareAndBind) const
{
    for (IBinding* dep : m_Dependencies) {
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
