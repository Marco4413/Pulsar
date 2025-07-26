#include "pulsar-debugger/context.h"

#include <format>

#include <pulsar/structures/stringview.h>

#include "pulsar-debugger/helpers.h"

namespace PulsarDebugger
{

DebuggerContext::DebuggerContext(std::shared_ptr<const DebuggableModule> mod)
    : m_DebuggableModule(mod)
{
    // Null Variables
    m_Variables.EmplaceBack();
}

ThreadId DebuggerContext::CreateThread(const Pulsar::ExecutionContext& execContext, std::optional<Pulsar::String> name)
{
    ThreadId threadId = ComputeThreadId(execContext);
    Thread& thread = m_Threads.Emplace(threadId).Value();

    ScopeId globalScopeId = CreateScope(execContext.GetGlobals(), "Globals", Variable::EVisibility::Visible);

    thread.Name = name ? std::move(*name) : Pulsar::String(std::format("Thread {}", threadId).c_str());
    for (size_t i = 1; i <= execContext.GetCallStack().Size(); ++i) {
        size_t frameIdx = execContext.GetCallStack().Size() - i;
        const Pulsar::Frame& callFrame = execContext.GetCallStack()[frameIdx];

        FrameId frameId = CreateStackFrame(
                callFrame,
                globalScopeId,
                frameIdx < execContext.GetCallStack().Size()-1,
                execContext.GetState() != Pulsar::RuntimeState::OK);
        thread.StackFrames.PushBack(frameId);
    }

    return threadId;
}

FrameId DebuggerContext::CreateStackFrame(const Pulsar::Frame& frame, ScopeId globalScope, bool isCaller, bool hasError)
{
    FrameId stackFrameId = m_StackFrames.Size();
    StackFrame& stackFrame = m_StackFrames.EmplaceBack();

    stackFrame.Name  = "(";
    if (frame.IsNative)
        stackFrame.Name += "*";
    stackFrame.Name += frame.Function->Name;
    stackFrame.Name += ")";

    stackFrame.Scopes.PushBack(globalScope);
    ScopeId localScopeId = CreateScope(frame.Locals, "Locals", Variable::EVisibility::Unbound);
    stackFrame.Scopes.PushBack(localScopeId);
    stackFrame.Scopes.PushBack(CreateScope(frame.Stack, "Stack", Variable::EVisibility::Visible));

    size_t instrIdx = frame.InstructionIndex;
    if (isCaller) { /* Caller */
        if (instrIdx > 0) /* Should always be true */
            --instrIdx;
    } else if (hasError) { /* Callee error */
        if (instrIdx > 0)
            --instrIdx;
    }

    size_t dbgSymIdx;
    if (frame.Function->FindCodeDebugSymbolFor(instrIdx, dbgSymIdx)) {
        stackFrame.SourcePos = frame.Function->CodeDebugSymbols[dbgSymIdx].Token.SourcePos;
    } else {
        stackFrame.SourcePos = frame.Function->DebugSymbol.Token.SourcePos;
    }

    stackFrame.SourceReference = static_cast<SourceReference>(frame.Function->DebugSymbol.SourceIdx);
    stackFrame.SourcePath      = std::nullopt;
    if (frame.Function->DebugSymbol.SourceIdx < m_DebuggableModule->GetModule().SourceDebugSymbols.Size())
        stackFrame.SourcePath  = m_DebuggableModule->GetModule().SourceDebugSymbols[frame.Function->DebugSymbol.SourceIdx].Path;

    auto scopeInfo = m_DebuggableModule->GetLocalScopeInfo(stackFrame.SourceReference, stackFrame.SourcePos.Line);
    if (scopeInfo) {
        const auto& localScope = m_Scopes[localScopeId];
        auto& variables = m_Variables[localScope.VariablesReference];

        Pulsar::HashMap<Pulsar::StringView, size_t> usedLocalNames;

        size_t namedVariablesCount = scopeInfo->Locals.Size() <= variables.Size() ? scopeInfo->Locals.Size() : variables.Size();
        for (size_t it = 1; it <= namedVariablesCount; ++it) {
            size_t localIdx = namedVariablesCount - it;
            const auto& local = scopeInfo->Locals[localIdx];
            variables[localIdx].Name = local.Name;
            if (usedLocalNames.Find(local.Name)) {
                variables[localIdx].Visibility = Variable::EVisibility::Shadowed;
            } else {
                usedLocalNames.Insert(local.Name, localIdx);
                variables[localIdx].Visibility = Variable::EVisibility::Visible;
            }
        }
    }

    return stackFrameId;
}

ScopeId DebuggerContext::CreateScope(const Pulsar::List<Pulsar::GlobalInstance>& globals, const char* name, Variable::EVisibility varVisibility)
{
    ScopeId scopeId = m_Scopes.Size();
    Scope& scope = m_Scopes.EmplaceBack();
    scope.Name = name;

    scope.VariablesReference = m_Variables.Size();
    m_Variables.EmplaceBack();

    for (size_t i = 0; i < globals.Size(); ++i) {
        Pulsar::String varName;
        if (globals[i].IsConstant)
            varName += "const ";
        varName += m_DebuggableModule->GetModule().Globals[i].Name;
        Variable var = CreateVariable(globals[i].Value, std::move(varName), varVisibility);
        m_Variables[scope.VariablesReference].EmplaceBack(std::move(var));
    }

    return scopeId;
}

ScopeId DebuggerContext::CreateScope(const Pulsar::List<Pulsar::Value>& locals, const char* name, Variable::EVisibility varVisibility)
{
    ScopeId scopeId = m_Scopes.Size();
    Scope& scope = m_Scopes.EmplaceBack();
    scope.Name = name;

    scope.VariablesReference = m_Variables.Size();
    m_Variables.EmplaceBack();

    for (size_t i = 0; i < locals.Size(); ++i) {
        Variable var = CreateVariable(locals[i], Pulsar::UIntToString(i), varVisibility);
        m_Variables[scope.VariablesReference].EmplaceBack(std::move(var));
    }

    return scopeId;
}

DebuggerContext::Variable DebuggerContext::CreateVariable(const Pulsar::Value& value, Pulsar::String&& name, Variable::EVisibility visibility)
{
    Variable variable;
    variable.Visibility = visibility;
    variable.Name       = std::move(name);
    variable.Type       = value.Type();
    variable.Value      = ValueToString(value, false);
    variable.VariablesReference = NULL_REFERENCE;

    switch (value.Type()) {
    case Pulsar::ValueType::List: {
        variable.VariablesReference = m_Variables.Size();
        m_Variables.EmplaceBack();

        uint64_t idx = 0;
        for (const auto* node = value.AsList().Front(); node; node = node->Next()) {
            Variable var = CreateVariable(node->Value(), Pulsar::UIntToString(idx++), Variable::EVisibility::Visible);
            m_Variables[variable.VariablesReference].EmplaceBack(std::move(var));
        }
    } break;
    default:
        break;
    }

    return variable;
}

std::optional<DebuggerContext::Thread> DebuggerContext::GetThread(ThreadId threadId) const
{
    const auto* thread = m_Threads.Find(threadId);
    if (!thread) return std::nullopt;
    return thread->Value();
}

std::optional<DebuggerContext::StackFrame> DebuggerContext::GetStackFrame(FrameId frameId) const
{
    if (frameId < 0 || static_cast<size_t>(frameId) >= m_StackFrames.Size()) return std::nullopt;
    return m_StackFrames[frameId];
}

std::optional<DebuggerContext::Scope> DebuggerContext::GetScope(ScopeId scopeId) const
{
    if (scopeId < 0 || static_cast<size_t>(scopeId) >= m_Scopes.Size()) return std::nullopt;
    return m_Scopes[scopeId];
}

std::optional<Pulsar::List<DebuggerContext::Variable>> DebuggerContext::GetVariables(VariablesReference variablesReference) const
{
    if (variablesReference < 0 || static_cast<size_t>(variablesReference) >= m_Variables.Size()) return std::nullopt;
    return m_Variables[variablesReference];
}

std::optional<Pulsar::SourceDebugSymbol> DebuggerContext::GetSource(SourceReference sourceReference) const
{
    if (!m_DebuggableModule) return std::nullopt;
    if (sourceReference < 0 || static_cast<size_t>(sourceReference) >= m_DebuggableModule->GetModule().SourceDebugSymbols.Size()) return std::nullopt;
    return m_DebuggableModule->GetModule().SourceDebugSymbols[sourceReference];
}

}
