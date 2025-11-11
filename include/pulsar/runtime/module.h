#ifndef _PULSAR_RUNTIME_MODULE_H
#define _PULSAR_RUNTIME_MODULE_H

#include "pulsar/core.h"

#include "pulsar/runtime/customtype.h"
#include "pulsar/runtime/debug.h"
#include "pulsar/runtime/function.h"
#include "pulsar/runtime/global.h"
#include "pulsar/runtime/state.h"
#include "pulsar/runtime/value.h"
#include "pulsar/structures/hashmap.h"
#include "pulsar/structures/list.h"

namespace Pulsar
{
    // Forward declaration for NativeFunction
    class ExecutionContext;

    /**
     * This class represents an executable Pulsar program.
     * See ExecutionContext for program execution.
     */
    class Module
    {
    public:
        // Returned by Find* functions to indicate that the item was not found.
        static constexpr size_t INVALID_INDEX = size_t(-1);

    public:
        Module() = default;
        ~Module() = default;

        Module(const Module&) = default;
        Module(Module&&) = default;

        Module& operator=(const Module&) = default;
        Module& operator=(Module&&) = default;

        using NativeFunction = std::function<RuntimeState(ExecutionContext&)>;
        // Returns how many definitions were bound.
        size_t BindNativeFunction(const FunctionDefinition& def, NativeFunction func);
        size_t BindNativeFunction(FunctionSignature sig, NativeFunction func);
        // Returns the index of the newly declared function.
        size_t DeclareAndBindNativeFunction(const FunctionDefinition& def, NativeFunction func);
        size_t DeclareAndBindNativeFunction(FunctionDefinition&& def, NativeFunction func);
        size_t DeclareAndBindNativeFunction(FunctionSignature sig, NativeFunction func);

        uint64_t BindCustomType(const String& name, CustomType::GlobalDataFactoryFn globalDataFactory=nullptr);

        // Be sure to check if the type exists first (unless you know for sure it exists)
        CustomType& GetCustomType(uint64_t typeId)             { return CustomTypes.Find(typeId)->Value(); }
        const CustomType& GetCustomType(uint64_t typeId) const { return CustomTypes.Find(typeId)->Value(); }
        bool HasCustomType(uint64_t typeId) const              { return CustomTypes.Find(typeId); }

        bool HasSourceDebugSymbols() const { return !SourceDebugSymbols.IsEmpty(); }

        template<typename T>
        size_t FindDefinitionByName(const List<T>& definitions, const String& name) const;

        size_t FindFunctionByName(const String& name) const { return FindDefinitionByName(Functions, name); }
        size_t FindNativeByName(const String& name)   const { return FindDefinitionByName(NativeBindings, name); }
        size_t FindGlobalByName(const String& name)   const { return FindDefinitionByName(Globals, name); }

        size_t FindFunctionBySignature(FunctionSignature sig) const;

    public:
        // Access these member variables only for:
        // - Inspecting the Module.
        // - Creating your own language which runs on the Pulsar VM.
        List<FunctionDefinition> Functions;
        List<FunctionDefinition> NativeBindings;
        List<GlobalDefinition> Globals;
        List<Value> Constants;

        List<SourceDebugSymbol> SourceDebugSymbols;

        // These are managed by the Bind* methods.
        List<NativeFunction> NativeFunctions;
        HashMap<uint64_t, CustomType> CustomTypes;

    private:
        uint64_t m_LastTypeId = 0;
    };
}

template<typename T>
size_t Pulsar::Module::FindDefinitionByName(const List<T>& definitions, const String& name) const
{
    for (size_t i = definitions.Size(); i > 0; --i) {
        const T& definition = definitions[i-1];
        if (definition.Name != name)
            continue;
        return i-1;
    }
    return INVALID_INDEX;
}

#endif // _PULSAR_RUNTIME_MODULE_H
