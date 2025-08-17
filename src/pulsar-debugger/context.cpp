#include "pulsar-debugger/context.h"

#include <format>

#include <pulsar/structures/stringview.h>

#include "pulsar-debugger/helpers.h"

namespace PulsarDebugger
{

std::optional<Pulsar::SourceDebugSymbol> DebuggerContext::GetSourceFromModule(const DebuggableModule& mod, SourceReference sourceReference)
{
    if (sourceReference < 0 || static_cast<size_t>(sourceReference) >= mod.GetModule().SourceDebugSymbols.Size()) return std::nullopt;
    return mod.GetModule().SourceDebugSymbols[sourceReference];
}

DebuggerContext::DebuggerContext(Debugger& debugger)
    : m_Debugger(debugger)
    , m_DebuggableModule(nullptr)
{
    // Null Variables
    m_Variables.EmplaceBack();

    DebuggerScopeLock _lock(m_Debugger);
    m_DebuggableModule = debugger.GetModule();
    RegisterThread(m_Debugger.GetMainThreadId(), "MainThread");
}

bool DebuggerContext::RegisterThread(ThreadId threadId, std::optional<Pulsar::String> name)
{
    auto debuggerThread = m_Debugger.GetThread(threadId);
    if (!debuggerThread) return false;

    DebuggerContextScopeLock _lock(*this);
    ThreadScopeLock _threadLock(*debuggerThread);

    Thread& thread = m_Threads.Emplace(threadId).Value();
    const auto& threadContext = debuggerThread->GetContext();
    ScopeId globalScopeId = CreateLazyScope(threadId, LazyScope::EKind::Globals);
    // ScopeId globalScopeId = CreateScope(threadContext.GetGlobals(), "Globals", Variable::EVisibility::Visible);

    thread.Name = name ? std::move(*name) : Pulsar::String(std::format("Thread {}", threadId).c_str());
    for (size_t i = 1; i <= threadContext.GetCallStack().Size(); ++i) {
        size_t callIndex = threadContext.GetCallStack().Size() - i;
        FrameId frameId = CreateLazyStackFrame(
                threadId,
                callIndex,
                globalScopeId,
                callIndex < threadContext.GetCallStack().Size()-1,
                threadContext.GetState() != Pulsar::RuntimeState::OK);
        thread.StackFrames.PushBack(frameId);
    }

    return true;
}

std::optional<DebuggerContext::StackFrame> DebuggerContext::GetOrLoadStackFrame(FrameId frameId)
{
    DebuggerContextScopeLock _lock(*this);
    if (frameId < 0 || static_cast<size_t>(frameId) >= m_StackFrames.Size()) return std::nullopt;
    auto& stackFrameOrLazy = m_StackFrames[frameId];
    if (std::holds_alternative<LazyStackFrame>(stackFrameOrLazy)) {
        auto stackFrame = LoadStackFrame(frameId, std::get<LazyStackFrame>(stackFrameOrLazy));
        if (!stackFrame) return std::nullopt;
        stackFrameOrLazy = *stackFrame;
    }
    return std::get<StackFrame>(stackFrameOrLazy);
}

std::optional<DebuggerContext::Scope> DebuggerContext::GetOrLoadScope(ScopeId scopeId)
{
    DebuggerContextScopeLock _lock(*this);
    if (scopeId < 0 || static_cast<size_t>(scopeId) >= m_Scopes.Size()) return std::nullopt;
    auto& scopeOrLazy = m_Scopes[scopeId];
    if (std::holds_alternative<LazyScope>(scopeOrLazy)) {
        auto scope = LoadScope(std::get<LazyScope>(scopeOrLazy));
        if (!scope) return std::nullopt;
        scopeOrLazy = *scope;
    }
    return std::get<Scope>(scopeOrLazy);
}

std::optional<DebuggerContext::Thread> DebuggerContext::GetThread(ThreadId threadId)
{
    DebuggerContextScopeLock _lock(*this);
    const auto* thread = m_Threads.Find(threadId);
    if (!thread) return std::nullopt;
    return thread->Value();
}

std::optional<Pulsar::List<DebuggerContext::Variable>> DebuggerContext::GetVariables(VariablesReference variablesReference)
{
    DebuggerContextScopeLock _lock(*this);
    if (variablesReference < 0 || static_cast<size_t>(variablesReference) >= m_Variables.Size()) return std::nullopt;
    return m_Variables[variablesReference];
}

std::optional<Pulsar::SourceDebugSymbol> DebuggerContext::GetSource(SourceReference sourceReference) const
{
    // No need to lock since module is constant
    if (!m_DebuggableModule) return std::nullopt;
    return GetSourceFromModule(*m_DebuggableModule, sourceReference);
}

FrameId DebuggerContext::CreateLazyStackFrame(ThreadId threadId, size_t callIndex, ScopeId globalScopeId, bool isCaller, bool hasError)
{
    FrameId stackFrameId = m_StackFrames.Size();
    m_StackFrames.EmplaceBack(LazyStackFrame{
        .ThreadId  = threadId,
        .CallIndex = callIndex,
        .InstructionOffset = isCaller || hasError ? -1 : 0,
        .GlobalScopeId     = globalScopeId,
    });

    return stackFrameId;
}

ScopeId DebuggerContext::CreateLazyScope(Id threadOrFrameId, LazyScope::EKind scopeKind)
{
    ScopeId scopeId = m_Scopes.Size();
    m_Scopes.EmplaceBack(LazyScope{
        .ThreadOrFrameId = threadOrFrameId,
        .Kind            = scopeKind,
    });

    return scopeId;
}

std::optional<DebuggerContext::StackFrame> DebuggerContext::LoadStackFrame(FrameId frameId, const LazyStackFrame& lazyFrame)
{
    auto debuggerThread = m_Debugger.GetThread(lazyFrame.ThreadId);
    if (!debuggerThread) return std::nullopt;

    ThreadScopeLock _threadLock(*debuggerThread);
    const Pulsar::CallStack& callStack = debuggerThread->GetContext().GetCallStack();
    if (lazyFrame.CallIndex >= callStack.Size()) return std::nullopt;
    const Pulsar::Frame& frame = callStack[lazyFrame.CallIndex];

    StackFrame stackFrame;
    stackFrame.ThreadId  = lazyFrame.ThreadId;
    stackFrame.CallIndex = lazyFrame.CallIndex;

    stackFrame.Name  = "(";
    if (frame.IsNative)
        stackFrame.Name += "*";
    stackFrame.Name += frame.Function->Name;
    stackFrame.Name += ")";

    stackFrame.Scopes.PushBack(lazyFrame.GlobalScopeId);
    ScopeId localScopeId = CreateLazyScope(frameId, LazyScope::EKind::Locals);
    // ScopeId localScopeId = CreateScope(frame.Locals, "Locals", Variable::EVisibility::Unbound);
    stackFrame.Scopes.PushBack(localScopeId);
    stackFrame.Scopes.PushBack(CreateLazyScope(frameId, LazyScope::EKind::Stack));
    // stackFrame.Scopes.PushBack(CreateLazyScope(frame.Stack, "Stack", Variable::EVisibility::Visible));

    size_t instrIdx = frame.InstructionIndex;
    if (lazyFrame.InstructionOffset < 0) {
        if (static_cast<size_t>(-lazyFrame.InstructionOffset) >= frame.InstructionIndex) {
            instrIdx  = 0;
        } else {
            instrIdx += lazyFrame.InstructionOffset;
        }
    } else if (lazyFrame.InstructionOffset > 0) {
        instrIdx += lazyFrame.InstructionOffset;
    }

    size_t dbgSymIdx;
    if (frame.Function->FindCodeDebugSymbolFor(instrIdx, dbgSymIdx)) {
        stackFrame.SourcePos = frame.Function->CodeDebugSymbols[dbgSymIdx].Token.SourcePos;
    } else {
        stackFrame.SourcePos = frame.Function->DebugSymbol.Token.SourcePos;
    }

    stackFrame.SourceReference = frame.Function->DebugSymbol.SourceIdx;
    stackFrame.SourcePath      = std::nullopt;
    if (frame.Function->DebugSymbol.SourceIdx < m_DebuggableModule->GetModule().SourceDebugSymbols.Size())
        stackFrame.SourcePath  = m_DebuggableModule->GetModule().SourceDebugSymbols[frame.Function->DebugSymbol.SourceIdx].Path;

    return stackFrame;
}

std::optional<DebuggerContext::Scope> DebuggerContext::LoadScope(const LazyScope& lazyScope)
{
    switch (lazyScope.Kind) {
    case LazyScope::EKind::Globals: {
        auto thread = m_Debugger.GetThread(lazyScope.ThreadOrFrameId);
        ThreadScopeLock _threadLock(*thread);

        const auto& globals = thread->GetContext().GetGlobals();

        Scope scope;
        scope.Name = "Globals";

        scope.VariablesReference = m_Variables.Size();
        m_Variables.EmplaceBack();

        for (size_t i = 0; i < globals.Size(); ++i) {
            Pulsar::String varName;
            if (globals[i].IsConstant)
                varName += "const ";
            varName += m_DebuggableModule->GetModule().Globals[i].Name;
            Variable var = CreateVariable(globals[i].Value, std::move(varName), Variable::EVisibility::Visible);
            m_Variables[scope.VariablesReference].EmplaceBack(std::move(var));
        }

        return scope;
    }
    case LazyScope::EKind::Locals:
    case LazyScope::EKind::Stack: {
        FrameId frameId = lazyScope.ThreadOrFrameId;
        if (frameId < 0 || static_cast<size_t>(frameId) >= m_StackFrames.Size()) return std::nullopt;
        if (!std::holds_alternative<StackFrame>(m_StackFrames[frameId])) return std::nullopt;
        const auto& stackFrame = std::get<StackFrame>(m_StackFrames[frameId]);

        auto thread = m_Debugger.GetThread(stackFrame.ThreadId);
        ThreadScopeLock _threadLock(*thread);

        if (stackFrame.CallIndex > thread->GetContext().GetCallStack().Size()) return std::nullopt;
        const auto& callFrame = thread->GetContext().GetCallStack()[stackFrame.CallIndex];

        Scope scope;

        Variable::EVisibility defaultVisibility;
        const Pulsar::List<Pulsar::Value>* values;

        if (lazyScope.Kind == LazyScope::EKind::Locals) {
            scope.Name = "Locals";
            defaultVisibility = Variable::EVisibility::Unbound;
            values = &callFrame.Locals;
        } else {
            scope.Name = "Stack";
            defaultVisibility = Variable::EVisibility::Visible;
            values = &callFrame.Stack;
        }

        scope.VariablesReference = m_Variables.Size();
        m_Variables.EmplaceBack();

        for (size_t i = 0; i < values->Size(); ++i) {
            Variable var = CreateVariable((*values)[i], Pulsar::UIntToString(i), defaultVisibility);
            m_Variables[scope.VariablesReference].EmplaceBack(std::move(var));
        }

        if (lazyScope.Kind == LazyScope::EKind::Locals) {
            auto scopeInfo = m_DebuggableModule->GetLocalScopeInfo(stackFrame.SourceReference, stackFrame.SourcePos.Line);
            if (scopeInfo) {
                auto& variables = m_Variables[scope.VariablesReference];

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
        }

        return scope;
    }
    default:
        return std::nullopt;
    }
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

}
