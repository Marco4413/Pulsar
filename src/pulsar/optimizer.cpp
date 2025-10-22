#include "pulsar/optimizer.h"

Pulsar::BaseOptimizerSettings::IsExportedFunctionFn Pulsar::BaseOptimizerSettings::CreateReachableFunctionsFilter(const Module& module, const List<StringView>& exportedNames) { return CreateReachableDefinitionFilterFor(module.Functions, exportedNames); }
Pulsar::BaseOptimizerSettings::IsExportedFunctionFn Pulsar::BaseOptimizerSettings::CreateReachableFunctionsFilter(const Module& module, const List<String>& exportedNames)     { return CreateReachableDefinitionFilterFor(module.Functions, exportedNames); }
Pulsar::BaseOptimizerSettings::IsExportedNativeFn   Pulsar::BaseOptimizerSettings::CreateReachableNativesFilter(const Module& module, const List<StringView>& exportedNames)   { return CreateReachableDefinitionFilterFor(module.NativeBindings, exportedNames); }
Pulsar::BaseOptimizerSettings::IsExportedNativeFn   Pulsar::BaseOptimizerSettings::CreateReachableNativesFilter(const Module& module, const List<String>& exportedNames)       { return CreateReachableDefinitionFilterFor(module.NativeBindings, exportedNames); }
Pulsar::BaseOptimizerSettings::IsExportedGlobalFn   Pulsar::BaseOptimizerSettings::CreateReachableGlobalsFilter(const Module& module, const List<StringView>& exportedNames)   { return CreateReachableDefinitionFilterFor(module.Globals, exportedNames); }
Pulsar::BaseOptimizerSettings::IsExportedGlobalFn   Pulsar::BaseOptimizerSettings::CreateReachableGlobalsFilter(const Module& module, const List<String>& exportedNames)       { return CreateReachableDefinitionFilterFor(module.Globals, exportedNames); }

void Pulsar::UnusedOptimizer::Optimize(Module& module, const Settings& settings)
{
    MarkReachables(module, settings);
    RemoveUnreachable(module);
    RemapIndices(module);
}

void Pulsar::UnusedOptimizer::MarkReachables(const Module& module, const Settings& settings)
{
    // TODO: Graceful bounds checking
#define MARK_REACHABLE(reachable, name)                                                 \
    do {                                                                                \
        size_t index = static_cast<size_t>(instruction.Arg0);                           \
        PULSAR_ASSERT(index < (reachable).Size(), "Index out of bounds for " name "."); \
        (reachable)[index] = true;                                                      \
    } while (0)

    m_ReachableFunctions.Clear();
    m_ReachableFunctions.Resize(module.Functions.Size(), false);

    m_ReachableNatives.Clear();
    m_ReachableNatives.Resize(module.NativeBindings.Size(), false);

    m_ReachableGlobals.Clear();
    m_ReachableGlobals.Resize(module.Globals.Size(), false);

    m_ReachableConstants.Clear();
    m_ReachableConstants.Resize(module.Constants.Size(), false);

    if (settings.IsExportedFunction) {
        List<size_t> functionIndicesToCheck(module.Functions.Size());
        for (size_t fnIdx = 0; fnIdx < module.Functions.Size(); ++fnIdx) {
            if (settings.IsExportedFunction(fnIdx, module.Functions[fnIdx])) {
                functionIndicesToCheck.PushBack(fnIdx);
                m_ReachableFunctions[fnIdx] = true;
            }
        }

        while (functionIndicesToCheck.Size() > 0) {
            size_t fnIdx = functionIndicesToCheck.Back();
            functionIndicesToCheck.PopBack();

            const auto& function = module.Functions[fnIdx];
            for (size_t j = 0; j < function.Code.Size(); ++j) {
                auto instruction = function.Code[j];
                if (OptimizerUtils::InstructionReferencesFunction(instruction.Code)) {
                    size_t instrFnIdx = static_cast<size_t>(instruction.Arg0);
                    PULSAR_ASSERT(instrFnIdx < m_ReachableFunctions.Size(), "Index out of bounds for function.");
                    if (!m_ReachableFunctions[instrFnIdx])
                        functionIndicesToCheck.PushBack(instrFnIdx);
                    m_ReachableFunctions[instrFnIdx] = true;
                } else if (OptimizerUtils::InstructionReferencesNative(instruction.Code)) {
                    MARK_REACHABLE(m_ReachableNatives, "native");
                } else if (OptimizerUtils::InstructionReferencesGlobal(instruction.Code)) {
                    MARK_REACHABLE(m_ReachableGlobals, "global");
                } else if (OptimizerUtils::InstructionReferencesConstant(instruction.Code)) {
                    MARK_REACHABLE(m_ReachableConstants, "constant");
                }
            }
        }
    }

    if (settings.IsExportedNative) {
        for (size_t nativeIdx = 0; nativeIdx < module.NativeBindings.Size(); ++nativeIdx) {
            // Already reachable by a function, no need to check if it's exported
            if (m_ReachableNatives[nativeIdx]) continue;
            if (settings.IsExportedNative(nativeIdx, module.NativeBindings[nativeIdx])) {
                m_ReachableNatives[nativeIdx] = true;
            }
        }
    }

    if (settings.IsExportedGlobal) {
        for (size_t globalIdx = 0; globalIdx < module.Globals.Size(); ++globalIdx) {
            // Already reachable by a function, no need to check if it's exported
            if (m_ReachableGlobals[globalIdx]) continue;
            if (settings.IsExportedGlobal(globalIdx, module.Globals[globalIdx])) {
                m_ReachableGlobals[globalIdx] = true;
            }
        }
    }

#undef MARK_REACHABLE
}

void Pulsar::UnusedOptimizer::RemoveUnreachable(Module& module)
{
    m_RemappedFunctions.Clear();
    m_RemappedFunctions.Resize(module.Functions.Size(), INVALID_INDEX);

    m_RemappedNatives.Clear();
    m_RemappedNatives.Resize(module.NativeBindings.Size(), INVALID_INDEX);

    m_RemappedGlobals.Clear();
    m_RemappedGlobals.Resize(module.Globals.Size(), INVALID_INDEX);

    m_RemappedConstants.Clear();
    m_RemappedConstants.Resize(module.Constants.Size(), INVALID_INDEX);

    RemoveUnreachableFor(m_ReachableFunctions, m_RemappedFunctions, module.Functions);
    RemoveUnreachableFor(m_ReachableNatives,   m_RemappedNatives,   module.NativeBindings, module.NativeFunctions);
    RemoveUnreachableFor(m_ReachableGlobals,   m_RemappedGlobals,   module.Globals);
    RemoveUnreachableFor(m_ReachableConstants, m_RemappedConstants, module.Constants);
}

void Pulsar::UnusedOptimizer::RemapIndices(Module& module)
{
#define REMAP_INDEX(map, name)                                                                   \
    do {                                                                                         \
        size_t index = static_cast<size_t>(instruction.Arg0);                                    \
        size_t remappedIndex = (map)[index];                                                     \
        PULSAR_ASSERT(remappedIndex != INVALID_INDEX, "Did not find remap for " name " index."); \
        instruction.Arg0 = static_cast<int64_t>(remappedIndex);                                  \
    } while (0)

    for (size_t fnIdx = 0; fnIdx < module.Functions.Size(); ++fnIdx) {
        auto& function = module.Functions[fnIdx];
        for (size_t j = 0; j < function.Code.Size(); ++j) {
            auto& instruction = function.Code[j];
            if (OptimizerUtils::InstructionReferencesFunction(instruction.Code)) {
                REMAP_INDEX(m_RemappedFunctions, "function");
            } else if (OptimizerUtils::InstructionReferencesNative(instruction.Code)) {
                REMAP_INDEX(m_RemappedNatives, "native");
            } else if (OptimizerUtils::InstructionReferencesGlobal(instruction.Code)) {
                REMAP_INDEX(m_RemappedGlobals, "global");
            } else if (OptimizerUtils::InstructionReferencesConstant(instruction.Code)) {
                REMAP_INDEX(m_RemappedConstants, "constant");
            }
        }
    }

#undef REMAP_INDEX
}
