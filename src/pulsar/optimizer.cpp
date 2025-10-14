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
    m_ReachableFunctions.Clear();
    m_ReachableFunctions.Resize(module.Functions.Size(), false);

    m_ReachableNatives.Clear();
    m_ReachableNatives.Resize(module.NativeBindings.Size(), false);

    m_ReachableGlobals.Clear();
    m_ReachableGlobals.Resize(module.Globals.Size(), false);

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
                    if (!m_ReachableFunctions[instrFnIdx])
                        functionIndicesToCheck.PushBack(instrFnIdx);
                    m_ReachableFunctions[instrFnIdx] = true;
                } else if (OptimizerUtils::InstructionReferencesNative(instruction.Code)) {
                    size_t instrNativeIdx = static_cast<size_t>(instruction.Arg0);
                    m_ReachableNatives[instrNativeIdx] = true;
                } else if (OptimizerUtils::InstructionReferencesGlobal(instruction.Code)) {
                    size_t instrGlobalIdx = static_cast<size_t>(instruction.Arg0);
                    m_ReachableGlobals[instrGlobalIdx] = true;
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
}

void Pulsar::UnusedOptimizer::RemoveUnreachable(Module& module)
{
    m_RemappedFunctions.Clear();
    m_RemappedFunctions.Resize(module.Functions.Size(), INVALID_INDEX);

    m_RemappedNatives.Clear();
    m_RemappedNatives.Resize(module.NativeBindings.Size(), INVALID_INDEX);

    m_RemappedGlobals.Clear();
    m_RemappedGlobals.Resize(module.Globals.Size(), INVALID_INDEX);

    RemoveUnreachableFor(m_ReachableFunctions, m_RemappedFunctions, module.Functions);
    RemoveUnreachableFor(m_ReachableNatives,   m_RemappedNatives,   module.NativeBindings, module.NativeFunctions);
    RemoveUnreachableFor(m_ReachableGlobals,   m_RemappedGlobals,   module.Globals);
}

void Pulsar::UnusedOptimizer::RemapIndices(Module& module)
{
    for (size_t fnIdx = 0; fnIdx < module.Functions.Size(); ++fnIdx) {
        auto& function = module.Functions[fnIdx];
        for (size_t j = 0; j < function.Code.Size(); ++j) {
            auto& instruction = function.Code[j];
            if (OptimizerUtils::InstructionReferencesFunction(instruction.Code)) {
                size_t instrFnIdx = static_cast<size_t>(instruction.Arg0);
                size_t remappedIndex = m_RemappedFunctions[instrFnIdx];
                PULSAR_ASSERT(remappedIndex != INVALID_INDEX, "Did not find remap for function index.");
                instruction.Arg0 = static_cast<int64_t>(remappedIndex);
            } else if (OptimizerUtils::InstructionReferencesNative(instruction.Code)) {
                size_t instrNativeIdx = static_cast<size_t>(instruction.Arg0);
                size_t remappedIndex = m_RemappedNatives[instrNativeIdx];
                PULSAR_ASSERT(remappedIndex != INVALID_INDEX, "Did not find remap for native index.");
                instruction.Arg0 = static_cast<int64_t>(remappedIndex);
            } else if (OptimizerUtils::InstructionReferencesGlobal(instruction.Code)) {
                size_t instrGlobalIdx = static_cast<size_t>(instruction.Arg0);
                size_t remappedIndex = m_RemappedGlobals[instrGlobalIdx];
                PULSAR_ASSERT(remappedIndex != INVALID_INDEX, "Did not find remap for global index.");
                instruction.Arg0 = static_cast<int64_t>(remappedIndex);
            }
        }
    }
}
