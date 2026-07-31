#include "pulsar/runtime/module.h"

size_t Pulsar::Module::DeclareNativeFunction(FunctionSignature signature)
{
    return DeclareNativeFunction(signature.ToNativeDefinition());
}

size_t Pulsar::Module::DeclareNativeFunction(const FunctionDefinition& definition)
{
    return DeclareNativeFunction(std::forward<FunctionDefinition>(FunctionDefinition(definition)));
}

size_t Pulsar::Module::DeclareNativeFunction(FunctionDefinition&& definition)
{
    NativeFunctions.Resize(NativeBindings.Size(), nullptr);

    size_t boundIndex = INVALID_INDEX;
    for (size_t nativeIdx = 0; nativeIdx < NativeBindings.Size(); ++nativeIdx) {
        FunctionDefinition& binding = NativeBindings[nativeIdx];
        if (!definition.DeclarationMatches(binding))
            continue;
        if (definition.HasDebugSymbol())
            binding.DebugSymbol = std::move(definition.DebugSymbol);
        boundIndex = nativeIdx;
    }

    if (boundIndex == INVALID_INDEX) {
        NativeBindings.EmplaceBack(std::move(definition));
        NativeFunctions.Resize(NativeBindings.Size());
        boundIndex = NativeBindings.Size()-1;
    }

    return boundIndex;
}

size_t Pulsar::Module::BindNativeFunction(FunctionSignature signature, NativeFunction function)
{
    return BindNativeFunction(signature.ToNativeDefinition(), function);
}

size_t Pulsar::Module::BindNativeFunction(const FunctionDefinition& definition, NativeFunction function)
{
    return BindNativeFunction(std::forward<FunctionDefinition>(FunctionDefinition(definition)), function);
}

size_t Pulsar::Module::BindNativeFunction(FunctionDefinition&& definition, NativeFunction function)
{
    NativeFunctions.Resize(NativeBindings.Size(), nullptr);

    size_t lastNativeIdx = DeclareNativeFunction(std::forward<FunctionDefinition>(definition));
    const FunctionDefinition& definitionToMatch = NativeBindings[lastNativeIdx];

    for (size_t nativeIdx = 0; nativeIdx <= lastNativeIdx; ++nativeIdx) {
        const FunctionDefinition& binding = NativeBindings[nativeIdx];
        if (!definitionToMatch.DeclarationMatches(binding))
            continue;
        NativeFunctions[nativeIdx] = function;
    }

    return lastNativeIdx;
}

uint64_t Pulsar::Module::BindCustomType(const String& name, CustomType::GlobalDataFactoryFn globalDataFactory)
{
    while (CustomTypes.Find(++m_LastTypeId));
    CustomTypes.Emplace(m_LastTypeId, name, globalDataFactory);
    return m_LastTypeId;
}

size_t Pulsar::Module::FindFunctionDefinitionBySignature(const List<FunctionDefinition>& definitions, FunctionSignature signature) const
{
    for (size_t i = definitions.Size(); i > 0; --i) {
        const auto& definition = definitions[i-1];
        if (signature.Matches(definition))
            continue;
        return i-1;
    }
    return INVALID_INDEX;
}
