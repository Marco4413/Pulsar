#ifndef _PULSAR_OPTIMIZER_H
#define _PULSAR_OPTIMIZER_H

#include "pulsar/core.h"

#include "pulsar/runtime.h"
#include "pulsar/structures/list.h"
#include "pulsar/structures/hashmap.h"
#include "pulsar/structures/string.h"
#include "pulsar/structures/stringview.h"

namespace Pulsar
{
    namespace OptimizerUtils
    {
        constexpr bool InstructionReferencesFunction(InstructionCode code);
        constexpr bool InstructionReferencesNative(InstructionCode code);
        constexpr bool InstructionReferencesGlobal(InstructionCode code);
        constexpr bool InstructionReferencesConstant(InstructionCode code);
    }

    struct BaseOptimizerSettings
    {
        template<typename T>
        using IsExportedDefinitionFn = std::function<bool(size_t, const T&)>;

        using IsExportedFunctionFn = IsExportedDefinitionFn<FunctionDefinition>;
        using IsExportedNativeFn   = IsExportedDefinitionFn<FunctionDefinition>;
        using IsExportedGlobalFn   = IsExportedDefinitionFn<GlobalDefinition>;

        IsExportedFunctionFn IsExportedFunction = nullptr;
        IsExportedNativeFn   IsExportedNative   = nullptr;
        IsExportedGlobalFn   IsExportedGlobal   = nullptr;

        static IsExportedFunctionFn CreateReachableFunctionsFilter(const Module& module, const List<StringView>& exportedNames);
        static IsExportedFunctionFn CreateReachableFunctionsFilter(const Module& module, const List<String>& exportedNames);

        static IsExportedNativeFn CreateReachableNativesFilter(const Module& module, const List<StringView>& exportedNames);
        static IsExportedNativeFn CreateReachableNativesFilter(const Module& module, const List<String>& exportedNames);

        static IsExportedGlobalFn CreateReachableGlobalsFilter(const Module& module, const List<StringView>& exportedNames);
        static IsExportedGlobalFn CreateReachableGlobalsFilter(const Module& module, const List<String>& exportedNames);

    private:
        template<typename T>
        static IsExportedDefinitionFn<T> CreateReachableDefinitionFilterFor(const List<T>& list, const List<StringView>& exportedNames);

        template<typename T>
        static IsExportedDefinitionFn<T> CreateReachableDefinitionFilterFor(const List<T>& list, const List<String>& exportedNames);
    };

    class UnusedOptimizer
    {
    public:
        using Settings = BaseOptimizerSettings;

        UnusedOptimizer() = default;
        ~UnusedOptimizer() = default;

        // Returns false on error. On error `module` is not modified.
        bool Optimize(
                Module& module,
                /* Leaving IsExportedFunction to nullptr may remove everything
                   as nothing is reachable from the outside */
                const Settings& settings);

    private:
        // Marks reachable functions. Returns false on error (index out of bounds).
        bool MarkReachables(const Module& module, const Settings& settings);

        // Removes items marked as unreachable from module and stores remapped indices.
        void RemoveUnreachable(Module& module);

        // Remaps indices that reference items moved within module.
        void RemapIndices(Module& module);

    private:
        static constexpr auto INVALID_INDEX = Module::INVALID_INDEX;
        // ReachableIndex[functionIndex] is true if functionIndex is reachable
        using ReachableIndex = List<bool>;
        // RemappedIndex[original index] is == INVALID_INDEX if functionIndex is unreachable.
        // Otherwise, it's the new index for that function.
        using RemappedIndex  = List<size_t>;

        template<typename ...T>
        void RemoveUnreachableFor(
                const ReachableIndex& reachableIndices,
                RemappedIndex& remappedIndices,
                List<T>& ...dataHolders);

    private:
        ReachableIndex m_ReachableFunctions;
        ReachableIndex m_ReachableNatives;
        ReachableIndex m_ReachableGlobals;
        ReachableIndex m_ReachableConstants;

        RemappedIndex m_RemappedFunctions;
        RemappedIndex m_RemappedNatives;
        RemappedIndex m_RemappedGlobals;
        RemappedIndex m_RemappedConstants;
    };
}

constexpr bool Pulsar::OptimizerUtils::InstructionReferencesFunction(InstructionCode code)
{
    return code == InstructionCode::Call
        || code == InstructionCode::PushFunctionReference;
}

constexpr bool Pulsar::OptimizerUtils::InstructionReferencesNative(InstructionCode code)
{
    return code == InstructionCode::CallNative
        || code == InstructionCode::PushNativeFunctionReference;
}

constexpr bool Pulsar::OptimizerUtils::InstructionReferencesGlobal(InstructionCode code)
{
    return code == InstructionCode::MoveGlobal
        || code == InstructionCode::PushGlobal
        || code == InstructionCode::PopIntoGlobal
        || code == InstructionCode::CopyIntoGlobal;
}

constexpr bool Pulsar::OptimizerUtils::InstructionReferencesConstant(InstructionCode code)
{
    return code == InstructionCode::PushConst;
}

template<typename T>
Pulsar::BaseOptimizerSettings::IsExportedDefinitionFn<T> Pulsar::BaseOptimizerSettings::CreateReachableDefinitionFilterFor(const List<T>& list, const List<StringView>& exportedNames)
{
    constexpr auto INVALID_INDEX = Module::INVALID_INDEX;

    HashMap<StringView, size_t> exportedNamesMap;
    for (size_t i = 0; i < exportedNames.Size(); ++i) {
        exportedNamesMap.Insert(exportedNames[i], INVALID_INDEX);
    }

    for (size_t i = 0; i < list.Size(); ++i) {
        size_t index = list.Size()-i-1;
        const T& definition = list[index];

        auto* exportedIndex = exportedNamesMap.Find(definition.Name);
        if (exportedIndex && exportedIndex->Value() == INVALID_INDEX)
            exportedIndex->Value() = index;
    }

    HashMap<size_t, bool> exportedIndices;
    exportedIndices.Reserve(exportedNames.Size());
    for (const auto& [ name, index ] : exportedNamesMap) {
        PULSAR_UNUSED(name);
        if (index != INVALID_INDEX)
            exportedIndices.Insert(index, true);
    }

    return [exportedIndices = std::move(exportedIndices)](size_t index, const auto&)
    {
        return (bool)exportedIndices.Find(index);
    };
}

template<typename T>
Pulsar::BaseOptimizerSettings::IsExportedDefinitionFn<T> Pulsar::BaseOptimizerSettings::CreateReachableDefinitionFilterFor(const List<T>& list, const List<String>& exportedNames)
{
    List<StringView> exportedNameViews(exportedNames.Size());
    for (size_t i = 0; i < exportedNames.Size(); ++i)
        exportedNameViews.PushBack(exportedNames[i]);
    return CreateReachableDefinitionFilterFor<T>(list, exportedNameViews);
}

template<typename ...T>
void Pulsar::UnusedOptimizer::RemoveUnreachableFor(const ReachableIndex& reachableIndices, RemappedIndex& remappedIndices, List<T>& ...dataHolders)
{
    size_t lastFreeIdx = INVALID_INDEX;
    for (size_t idx = 0; idx < reachableIndices.Size(); ++idx) {
        if (lastFreeIdx == INVALID_INDEX) {
            if (reachableIndices[idx]) {
                remappedIndices[idx] = idx;
            } else {
                lastFreeIdx = idx;
            }
        } else if (reachableIndices[idx]) {
            remappedIndices[idx] = lastFreeIdx;
            ((dataHolders[lastFreeIdx] = std::move(dataHolders[idx])), ...);
            ++lastFreeIdx;
        }
    }

    if (lastFreeIdx != INVALID_INDEX) {
        (dataHolders.Resize(lastFreeIdx), ...);
    }
}

#endif // _PULSAR_OPTIMIZER_H
