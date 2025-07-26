#ifndef _PULSARDEBUGGER_CONTEXT_H
#define _PULSARDEBUGGER_CONTEXT_H

#include <memory>
#include <optional>

#include <pulsar/runtime.h>
#include <pulsar/structures/hashmap.h>
#include <pulsar/structures/list.h>

#include "pulsar-debugger/types.h"

namespace PulsarDebugger
{
    // TODO: Lazy Loading
    // - One way to do it might be making DebuggerContext depend on Debugger,
    //   which is not currently the case. That would be needed because of multi-threading.
    //   We must be able to lock the Debugger while we access Pulsar::ExecutionContext.
    class DebuggerContext
    {
    public:
        struct Thread
        {
            Pulsar::String Name;
            Pulsar::List<FrameId> StackFrames;
        };

        struct StackFrame
        {
            Pulsar::String Name;
            Pulsar::List<ScopeId> Scopes;

            PulsarDebugger::SourceReference SourceReference = NULL_REFERENCE;
            std::optional<Pulsar::String>   SourcePath;
            Pulsar::SourcePosition          SourcePos;
        };

        struct Scope
        {
            Pulsar::String Name;
            PulsarDebugger::VariablesReference VariablesReference = NULL_REFERENCE;
        };

        struct Variable
        {
            Pulsar::String Name;
            Pulsar::String Type;
            Pulsar::String Value;
            // Used for Lists
            PulsarDebugger::VariablesReference VariablesReference = NULL_REFERENCE;
        };

    public:
        DebuggerContext(std::shared_ptr<const DebuggableModule> mod);
        ~DebuggerContext() = default;

        DebuggerContext(const DebuggerContext&) = default;
        DebuggerContext(DebuggerContext&&) = default;

        ThreadId CreateThread(const Pulsar::ExecutionContext& execContext, std::optional<Pulsar::String> name=std::nullopt);

        std::optional<Thread> GetThread(ThreadId threadId) const;
        std::optional<StackFrame> GetStackFrame(FrameId frameId) const;
        std::optional<Scope> GetScope(ScopeId scopeId) const;
        std::optional<Pulsar::List<Variable>> GetVariables(VariablesReference variablesReference) const;

        std::optional<Pulsar::SourceDebugSymbol> GetSource(SourceReference sourceReference) const;

    private:
        FrameId CreateStackFrame(const Pulsar::Frame& frame, ScopeId globalScopeId, bool isCaller, bool hasError);
        ScopeId CreateScope(const Pulsar::List<Pulsar::GlobalInstance>& globals, const char* name);
        ScopeId CreateScope(const Pulsar::List<Pulsar::Value>& locals, const char* name);
        // Calling this function may invalidate any reference to data within m_Variables
        Variable CreateVariable(const Pulsar::Value& value, Pulsar::String&& name);

    private:
        std::shared_ptr<const DebuggableModule> m_DebuggableModule;

        Pulsar::HashMap<ThreadId, Thread> m_Threads;
        Pulsar::List<StackFrame> m_StackFrames;
        Pulsar::List<Scope> m_Scopes;
        Pulsar::List<Pulsar::List<Variable>> m_Variables;
    };
}

#endif // _PULSARDEBUGGER_CONTEXT_H
