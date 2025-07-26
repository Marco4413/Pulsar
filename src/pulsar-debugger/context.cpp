#include "pulsar-debugger/context.h"

#include <format>

#include <pulsar/lexer.h>

inline const char* ValueTypeToString(Pulsar::ValueType type)
{
    switch (type) {
    case Pulsar::ValueType::Void:
        return "Void";
    case Pulsar::ValueType::Integer:
        return "Integer";
    case Pulsar::ValueType::Double:
        return "Double";
    case Pulsar::ValueType::FunctionReference:
        return "FunctionReference";
    case Pulsar::ValueType::NativeFunctionReference:
        return "NativeFunctionReference";
    case Pulsar::ValueType::List:
        return "List";
    case Pulsar::ValueType::String:
        return "String";
    case Pulsar::ValueType::Custom:
        return "Custom";
    default:
        return "Unknown";
    }
}

inline Pulsar::String ValueToString(Pulsar::Value val, bool recursive)
{
    switch (val.Type()) {
    case Pulsar::ValueType::Void:
        return "Void";
    case Pulsar::ValueType::Integer:
        return std::format("{}", val.AsInteger()).c_str();
    case Pulsar::ValueType::Double:
        return std::format("{}", val.AsDouble()).c_str();
    case Pulsar::ValueType::FunctionReference:
        return std::format("<&(@{})", val.AsInteger()).c_str();
    case Pulsar::ValueType::NativeFunctionReference:
        return std::format("<&(*@{})", val.AsInteger()).c_str();
    case Pulsar::ValueType::List: {
        if (!recursive)
            return "[...]";

        if (!val.AsList().Front())
            return "[]";

        Pulsar::String asString;
        asString  = "[ ";

        const auto* node = val.AsList().Front();
        asString += ValueToString(node->Value(), recursive);
        node = node->Next();

        for (; node; node = node->Next()) {
            asString += ", ";
            asString += ValueToString(node->Value(), recursive);
        }

        asString += " ]";
        return asString;
    }
    case Pulsar::ValueType::String:
        return Pulsar::ToStringLiteral(val.AsString());
    case Pulsar::ValueType::Custom:
        return std::format("Custom(.Type={},.Data={})", val.AsCustom().Type, (void*)val.AsCustom().Data.Get()).c_str();
    default:
        return "Unknown";
    }
}

namespace PulsarDebugger
{

DebuggerContext::ThreadId DebuggerContext::ComputeThreadId(const Pulsar::ExecutionContext& thread)
{
    return reinterpret_cast<ThreadId>(&thread);
}

DebuggerContext::DebuggerContext(std::shared_ptr<const Pulsar::Module> mod)
    : m_Module(mod)
{
    // Null Variables
    m_Variables.EmplaceBack();
}

DebuggerContext::ThreadId DebuggerContext::CreateThread(const Pulsar::ExecutionContext& execContext, std::optional<Pulsar::String> name)
{
    ThreadId threadId = ComputeThreadId(execContext);
    Thread& thread = m_Threads.Emplace(threadId).Value();

    ScopeId globalScopeId = CreateScope(execContext.GetGlobals(), "Globals");

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

DebuggerContext::FrameId DebuggerContext::CreateStackFrame(const Pulsar::Frame& frame, ScopeId globalScope, bool isCaller, bool hasError)
{
    FrameId stackFrameId = m_StackFrames.Size();
    StackFrame& stackFrame = m_StackFrames.EmplaceBack();

    stackFrame.Name  = "(";
    if (frame.IsNative)
        stackFrame.Name += "*";
    stackFrame.Name += frame.Function->Name;
    stackFrame.Name += ")";

    stackFrame.Scopes.PushBack(globalScope);
    stackFrame.Scopes.PushBack(CreateScope(frame.Locals, "Locals"));
    stackFrame.Scopes.PushBack(CreateScope(frame.Stack, "Stack"));

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

    stackFrame.SourceReference = static_cast<SourceReferenceId>(frame.Function->DebugSymbol.SourceIdx);
    stackFrame.SourcePath      = std::nullopt;
    if (frame.Function->DebugSymbol.SourceIdx < m_Module->SourceDebugSymbols.Size())
        stackFrame.SourcePath  = m_Module->SourceDebugSymbols[frame.Function->DebugSymbol.SourceIdx].Path;

    return stackFrameId;
}

DebuggerContext::ScopeId DebuggerContext::CreateScope(const Pulsar::List<Pulsar::GlobalInstance>& globals, const char* name)
{
    ScopeId scopeId = m_Scopes.Size();
    Scope& scope = m_Scopes.EmplaceBack();
    scope.Name = name;

    VariablesReferenceId variablesReference = m_Variables.Size();
    m_Variables.EmplaceBack();

    scope.Variables = variablesReference;

    for (size_t i = 0; i < globals.Size(); ++i) {
        Pulsar::String varName;
        if (globals[i].IsConstant)
            varName += "const ";
        varName += m_Module->Globals[i].Name;
        Variable var = CreateVariable(globals[i].Value, std::move(varName));
        m_Variables[variablesReference].EmplaceBack(std::move(var));
    }

    return scopeId;
}

DebuggerContext::ScopeId DebuggerContext::CreateScope(const Pulsar::List<Pulsar::Value>& locals, const char* name)
{
    ScopeId scopeId = m_Scopes.Size();
    Scope& scope = m_Scopes.EmplaceBack();
    scope.Name = name;

    VariablesReferenceId variablesReference = m_Variables.Size();
    m_Variables.EmplaceBack();

    scope.Variables = variablesReference;

    for (size_t i = 0; i < locals.Size(); ++i) {
        Variable var = CreateVariable(locals[i], Pulsar::UIntToString(i));
        m_Variables[variablesReference].EmplaceBack(std::move(var));
    }

    return scopeId;
}

DebuggerContext::Variable DebuggerContext::CreateVariable(const Pulsar::Value& value, Pulsar::String&& name)
{
    Variable variable;
    variable.Name  = std::move(name);
    variable.Type  = ValueTypeToString(value.Type());
    variable.Value = ValueToString(value, false);
    variable.VariablesReference = 0;

    switch (value.Type()) {
    case Pulsar::ValueType::List: {
        VariablesReferenceId variablesReference = m_Variables.Size();
        m_Variables.EmplaceBack();

        variable.VariablesReference = variablesReference;

        uint64_t idx = 0;
        for (const auto* node = value.AsList().Front(); node; node = node->Next()) {
            Variable var = CreateVariable(node->Value(), Pulsar::UIntToString(idx++));
            m_Variables[variablesReference].EmplaceBack(std::move(var));
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

std::optional<Pulsar::List<DebuggerContext::Variable>> DebuggerContext::GetVariables(VariablesReferenceId variablesReference) const
{
    if (variablesReference < 0 || static_cast<size_t>(variablesReference) >= m_Variables.Size()) return std::nullopt;
    return m_Variables[variablesReference];
}

std::optional<Pulsar::SourceDebugSymbol> DebuggerContext::GetSource(SourceReferenceId sourceReference) const
{
    if (!m_Module) return std::nullopt;
    if (sourceReference < 0 || static_cast<size_t>(sourceReference) >= m_Module->SourceDebugSymbols.Size()) return std::nullopt;
    return m_Module->SourceDebugSymbols[sourceReference];
}

}
