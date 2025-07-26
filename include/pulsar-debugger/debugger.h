#ifndef _PULSARDEBUGGER_DEBUGGER_H
#define _PULSARDEBUGGER_DEBUGGER_H

#include <functional>
#include <mutex>
#include <optional>

#include <pulsar/runtime.h>

#include "pulsar-debugger/context.h"
#include "pulsar-debugger/types.h"

namespace PulsarDebugger
{
    class DebuggerScopeLock
    {
    public:
        DebuggerScopeLock(class Debugger& debugger);
        ~DebuggerScopeLock() = default;

        DebuggerScopeLock(const DebuggerScopeLock&) = delete;
        DebuggerScopeLock(DebuggerScopeLock&&)      = delete;
        DebuggerScopeLock& operator=(const DebuggerScopeLock&) = delete;
        DebuggerScopeLock& operator=(DebuggerScopeLock&&)      = delete;

        void Lock();
        void Unlock();

    private:
        std::unique_lock<std::recursive_mutex> m_Lock;
    };

    class Debugger
    {
    public:
        friend DebuggerScopeLock;

        using LaunchError = Pulsar::String;
        using BreakpointError = Pulsar::String;

        enum class EventKind { Step, Breakpoint, Continue, Pause, Done, Error };
        using EventHandler = std::function<void(ThreadId, EventKind, Debugger&)>;

        struct Breakpoint
        {
            bool Enabled = true;
        };

    public:
        Debugger();
        ~Debugger() = default;

        Debugger(const Debugger&) = delete;
        Debugger(Debugger&&)      = delete;
        Debugger& operator=(const Debugger&) = delete;
        Debugger& operator=(Debugger&&)      = delete;

        std::optional<LaunchError> Launch(const char* scriptPath, Pulsar::ValueList&& args, const char* entryPoint="main");

        void Continue();
        void Pause();

        std::optional<BreakpointError> SetBreakpoint(SourceReference sourceReference, size_t line);
        void ClearBreakpoints(SourceReference sourceReference);

        void StepInstruction();
        void StepOver();
        void StepInto();
        void StepOut();

        void WaitForEvent();
        void ProcessEvent();

        ThreadId GetMainThreadId();

        void SetEventHandler(EventHandler handler);

        std::optional<Pulsar::RuntimeState> GetCurrentState(ThreadId threadId);
        std::optional<size_t> GetOrComputeCurrentLine(ThreadId threadId);
        std::optional<size_t> GetOrComputeCurrentSourceIndex(ThreadId threadId);

        std::shared_ptr<const DebuggerContext> GetOrComputeContext();
        std::optional<Pulsar::SourceDebugSymbol> GetSource(SourceReference sourceReference);

    private:
        std::optional<size_t> ComputeCurrentLine(ThreadId threadId);
        std::optional<size_t> ComputeCurrentSourceIndex(ThreadId threadId);

        EventKind InternalStep();
        void DispatchEvent(ThreadId threadId, EventKind kind);

    private:
        EventHandler m_EventHandler;
        std::atomic_bool m_Continue;

        std::recursive_mutex m_Mutex;

        std::shared_ptr<DebuggableModule> m_DebuggableModule;
        Pulsar::List<Pulsar::HashMap<size_t, Breakpoint>> m_Breakpoints;

        std::optional<size_t> m_CachedCurrentLine;
        std::optional<size_t> m_CachedCurrentSource;

        ThreadId m_ThreadId;
        std::unique_ptr<Pulsar::ExecutionContext> m_Thread;
        std::shared_ptr<const DebuggerContext> m_Context;
    };
}

#endif // _PULSARDEBUGGER_DEBUGGER_H
