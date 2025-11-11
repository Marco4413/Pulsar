#include "pulsar/runtime/module.h"

size_t Pulsar::Module::BindNativeFunction(FunctionSignature sig, NativeFunction func)
{
    if (NativeFunctions.Size() != NativeBindings.Size())
        return 0;
    size_t bound = 0;
    for (size_t i = 0; i < NativeBindings.Size(); i++) {
        const FunctionDefinition& binding = NativeBindings[i];
        if (!sig.MatchesNative(binding))
            continue;
        bound++;
        NativeFunctions[i] = func;
    }
    return bound;
}

size_t Pulsar::Module::BindNativeFunction(const FunctionDefinition& def, NativeFunction func)
{
    if (def.Arity != def.LocalsCount) return 0;
    FunctionSignature sig{ def.Name, def.Arity, def.Returns, def.StackArity };
    return BindNativeFunction(sig, func);
}

size_t Pulsar::Module::DeclareAndBindNativeFunction(FunctionDefinition&& def, NativeFunction func)
{
    NativeBindings.EmplaceBack(std::move(def));
    NativeFunctions.Resize(NativeBindings.Size());
    NativeFunctions.Back() = func;
    return NativeBindings.Size()-1;
}

size_t Pulsar::Module::DeclareAndBindNativeFunction(const FunctionDefinition& def, NativeFunction func)
{
    return DeclareAndBindNativeFunction(std::forward<FunctionDefinition>(FunctionDefinition(def)), func);
}

size_t Pulsar::Module::DeclareAndBindNativeFunction(FunctionSignature sig, NativeFunction func)
{
    return DeclareAndBindNativeFunction(std::forward<FunctionDefinition>(sig.ToNativeDefinition()), func);
}

uint64_t Pulsar::Module::BindCustomType(const String& name, CustomType::GlobalDataFactoryFn globalDataFactory)
{
    while (CustomTypes.Find(++m_LastTypeId));
    CustomTypes.Emplace(m_LastTypeId, name, globalDataFactory);
    return m_LastTypeId;
}

size_t Pulsar::Module::FindFunctionBySignature(FunctionSignature sig) const
{
    for (size_t i = Functions.Size(); i > 0; --i) {
        const auto& definition = Functions[i-1];
        if (sig.Matches(definition))
            continue;
        return i-1;
    }
    return INVALID_INDEX;
}
