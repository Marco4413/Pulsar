#ifndef _PULSARDEBUGGER_CONTEXT_H
#define _PULSARDEBUGGER_CONTEXT_H

#include <memory>
#include <optional>

#include <pulsar/runtime.h>
#include <pulsar/structures/hashmap.h>
#include <pulsar/structures/list.h>

namespace Pulsar
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
            String Name;
            List<FrameId> StackFrames;
        };

        struct StackFrame
        {
            String Name;
            List<ScopeId> Scopes;

            SourceReferenceId     SourceReference = 0;
            std::optional<String> SourcePath;
            SourcePosition        SourcePos;
        };

        struct Scope
        {
            String Name;
            VariablesReferenceId Variables = 0;
        };

        struct Variable
        {
            String Name;
            String Type;
            String Value;
            // Used for Lists
            VariablesReferenceId VariablesReference = 0;
        };

    public:
        // Thread Ids are static and can be computed outside of ::CreateThread.
        // However, they're invalidated if the ExecutionContext is moved within memory.
        static ThreadId ComputeThreadId(const ExecutionContext& thread);

    public:
        DebuggerContext(std::shared_ptr<const Module> mod);
        ~DebuggerContext() = default;

        DebuggerContext(const DebuggerContext&) = default;
        DebuggerContext(DebuggerContext&&) = default;

        ThreadId CreateThread(const ExecutionContext& execContext, std::optional<String> name=std::nullopt);

        std::optional<Thread> GetThread(ThreadId threadId) const;
        std::optional<StackFrame> GetStackFrame(FrameId frameId) const;
        std::optional<Scope> GetScope(ScopeId scopeId) const;
        std::optional<List<Variable>> GetVariables(VariablesReferenceId variablesReference) const;

        std::optional<SourceDebugSymbol> GetSource(SourceReferenceId sourceReference) const;

    private:
        FrameId CreateStackFrame(const Frame& frame, ScopeId globalScopeId, bool isCaller, bool hasError);
        ScopeId CreateScope(const List<GlobalInstance>& globals, const char* name);
        ScopeId CreateScope(const List<Value>& locals, const char* name);
        // Calling this function may invalidate any reference to data within m_Variables
        Variable CreateVariable(const Value& value, String&& name);

    private:
        std::shared_ptr<const Module> m_Module;

        HashMap<ThreadId, Thread> m_Threads;
        List<StackFrame> m_StackFrames;
        List<Scope> m_Scopes;
        List<List<Variable>> m_Variables;
    };
}

#endif // _PULSARDEBUGGER_CONTEXT_H
