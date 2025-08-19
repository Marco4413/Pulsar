#ifndef _PULSARDEBUGGER_CONTEXT_H
#define _PULSARDEBUGGER_CONTEXT_H

#include <memory>
#include <mutex>
#include <optional>
#include <variant>

#include <pulsar/runtime.h>
#include <pulsar/structures/hashmap.h>
#include <pulsar/structures/list.h>

#include "pulsar-debugger/debugger.h"
#include "pulsar-debugger/lock.h"
#include "pulsar-debugger/types.h"

namespace PulsarDebugger
{
    using DebuggerContextScopeLock = ScopeLock<std::recursive_mutex>;
    class DebuggerContext : public ILockable<std::recursive_mutex>
    {
    public:
        struct Thread
        {
            Pulsar::String Name;
            Pulsar::List<FrameId> StackFrames;
        };

        struct StackFrame
        {
            PulsarDebugger::ThreadId ThreadId;
            size_t CallIndex;

            FrameId Id;
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
            enum class EVisibility { Visible, Shadowed, Unbound };

            EVisibility Visibility;
            Pulsar::String    Name;
            Pulsar::ValueType Type;
            Pulsar::String    Value;
            // Used for Lists
            PulsarDebugger::VariablesReference VariablesReference = NULL_REFERENCE;
        };

    public:
        DebuggerContext(Debugger& debugger);
        ~DebuggerContext() = default;

        DebuggerContext(const DebuggerContext&) = delete;
        DebuggerContext(DebuggerContext&&)      = delete;
        DebuggerContext& operator=(const DebuggerContext&) = delete;
        DebuggerContext& operator=(DebuggerContext&&)      = delete;

        bool RegisterThread(ThreadId id, std::optional<Pulsar::String> name=std::nullopt);

        std::optional<StackFrame> GetOrLoadStackFrame(FrameId frameId);
        std::optional<Scope> GetOrLoadScope(ScopeId scopeId);

        std::optional<Pulsar::List<StackFrame>> GetOrLoadStackFrames(ThreadId threadId, size_t framesStart=0, size_t framesCount=0, size_t* totalFrames=nullptr);
        std::optional<Pulsar::List<Scope>> GetOrLoadScopes(FrameId frameId, size_t scopesStart=0, size_t scopesCount=0, size_t* totalScopes=nullptr);
        std::optional<Pulsar::List<Variable>> GetVariables(VariablesReference variablesReference, size_t variablesStart=0, size_t variablesCount=0, size_t* totalVariables=nullptr);

        std::optional<Thread> GetThread(ThreadId threadId);
        // The returned value, if not nullptr, is valid while this instance exists.
        const Pulsar::SourceDebugSymbol* GetSource(SourceReference sourceReference) const;

    private:
        struct LazyStackFrame
        {
            PulsarDebugger::ThreadId ThreadId;
            size_t CallIndex;
            int64_t InstructionOffset;
            ScopeId GlobalScopeId;
        };

        using StackFrameOrLazy = std::variant<StackFrame, LazyStackFrame>;

        struct LazyScope
        {
            enum class EKind { Globals, Locals, Stack };

            // If Kind == ::Globals, it's a ThreadId, otherwise it's a FrameId
            Id ThreadOrFrameId;
            EKind Kind;
        };

        using ScopeOrLazy = std::variant<Scope, LazyScope>;

    private:
        FrameId CreateLazyStackFrame(ThreadId threadId, size_t callIndex, ScopeId globalScopeId, bool isCaller, bool hasError);
        std::optional<StackFrame> LoadStackFrame(FrameId frameId, const LazyStackFrame& lazyFrame);

        // If scopeKind == ::Globals, threadOrFrameId is a ThreadId, otherwise it's a FrameId
        ScopeId CreateLazyScope(Id threadOrFrameId, LazyScope::EKind scopeKind);
        std::optional<Scope> LoadScope(const LazyScope& lazyScope);

        // Calling this function may invalidate any reference to data within m_Variables
        Variable CreateVariable(const Pulsar::Value& value, Pulsar::String&& name, Variable::EVisibility visibility);

    private:
        Debugger& m_Debugger;
        SharedDebuggableModule m_DebuggableModule;

        Pulsar::HashMap<ThreadId, Thread> m_Threads;
        Pulsar::List<StackFrameOrLazy> m_StackFrames;
        Pulsar::List<ScopeOrLazy> m_Scopes;
        Pulsar::List<Pulsar::List<Variable>> m_Variables;
    };
}

#endif // _PULSARDEBUGGER_CONTEXT_H
