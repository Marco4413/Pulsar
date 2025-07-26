#ifndef _PULSARDEBUGGER_CONTEXT_H
#define _PULSARDEBUGGER_CONTEXT_H

#include <memory>
#include <optional>

#include <pulsar/runtime.h>
#include <pulsar/structures/hashmap.h>
#include <pulsar/structures/list.h>

namespace PulsarDebugger
{
    // TODO: Lazy Loading
    // - One way to do it might be making DebuggerContext depend on Debugger,
    //   which is not currently the case. That would be needed because of multi-threading.
    //   We must be able to lock the Debugger while we access Pulsar::ExecutionContext.
    class DebuggerContext
    {
    public:
        using Id                = int64_t;
        using ThreadId          = Id;
        using FrameId           = Id;
        using ScopeId           = Id;
        using VariablesReferenceId = Id;
        using SourceReferenceId    = Id;

        struct Thread
        {
            Pulsar::String Name;
            Pulsar::List<FrameId> StackFrames;
        };

        struct StackFrame
        {
            Pulsar::String Name;
            Pulsar::List<ScopeId> Scopes;

            SourceReferenceId             SourceReference = 0;
            std::optional<Pulsar::String> SourcePath;
            Pulsar::SourcePosition        SourcePos;
        };

        struct Scope
        {
            Pulsar::String Name;
            VariablesReferenceId Variables = 0;
        };

        struct Variable
        {
            Pulsar::String Name;
            Pulsar::String Type;
            Pulsar::String Value;
            // Used for Lists
            VariablesReferenceId VariablesReference = 0;
        };

    public:
        // Thread Ids are static and can be computed outside of ::CreateThread.
        // However, they're invalidated if the ExecutionContext is moved within memory.
        static ThreadId ComputeThreadId(const Pulsar::ExecutionContext& thread);

    public:
        DebuggerContext(std::shared_ptr<const Pulsar::Module> mod);
        ~DebuggerContext() = default;

        DebuggerContext(const DebuggerContext&) = default;
        DebuggerContext(DebuggerContext&&) = default;

        ThreadId CreateThread(const Pulsar::ExecutionContext& execContext, std::optional<Pulsar::String> name=std::nullopt);

        std::optional<Thread> GetThread(ThreadId threadId) const;
        std::optional<StackFrame> GetStackFrame(FrameId frameId) const;
        std::optional<Scope> GetScope(ScopeId scopeId) const;
        std::optional<Pulsar::List<Variable>> GetVariables(VariablesReferenceId variablesReference) const;

        std::optional<Pulsar::SourceDebugSymbol> GetSource(SourceReferenceId sourceReference) const;

    private:
        FrameId CreateStackFrame(const Pulsar::Frame& frame, ScopeId globalScopeId, bool isCaller, bool hasError);
        ScopeId CreateScope(const Pulsar::List<Pulsar::GlobalInstance>& globals, const char* name);
        ScopeId CreateScope(const Pulsar::List<Pulsar::Value>& locals, const char* name);
        // Calling this function may invalidate any reference to data within m_Variables
        Variable CreateVariable(const Pulsar::Value& value, Pulsar::String&& name);

    private:
        std::shared_ptr<const Pulsar::Module> m_Module;

        Pulsar::HashMap<ThreadId, Thread> m_Threads;
        Pulsar::List<StackFrame> m_StackFrames;
        Pulsar::List<Scope> m_Scopes;
        Pulsar::List<Pulsar::List<Variable>> m_Variables;
    };
}

#endif // _PULSARDEBUGGER_CONTEXT_H
