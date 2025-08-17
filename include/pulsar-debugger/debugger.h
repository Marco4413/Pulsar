#ifndef _PULSARDEBUGGER_DEBUGGER_H
#define _PULSARDEBUGGER_DEBUGGER_H

#include <functional>
#include <mutex>
#include <optional>

#include <pulsar/runtime.h>

#include "pulsar-debugger/lock.h"
#include "pulsar-debugger/thread.h"
#include "pulsar-debugger/types.h"

namespace PulsarDebugger
{
    using DebuggerScopeLock = ScopeLock<std::recursive_mutex>;
    class Debugger : public ILockable<std::recursive_mutex>
    {
    public:
        friend class DebuggerContext;

        using LaunchError = Pulsar::String;
        using BreakpointError = Pulsar::String;

        enum class EEventKind { Step, Breakpoint, Continue, Pause, Done, Error };
        using EventHandler = std::function<void(ThreadId, EEventKind, Debugger&)>;

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

        // The Debugger is paused by default on Launch
        std::optional<LaunchError> Launch(const char* scriptPath, Pulsar::ValueList&& args, const char* entryPoint="main");

        void Continue(ThreadId threadId);
        void Pause(ThreadId threadId);

        std::optional<BreakpointError> SetBreakpoint(SourceReference sourceReference, size_t line);
        void ClearBreakpoints(SourceReference sourceReference);

        void StepInstruction(ThreadId threadId);
        void StepOver(ThreadId threadId);
        void StepInto(ThreadId threadId);
        void StepOut(ThreadId threadId);

        void WaitForEvent();
        void ProcessEvent();

        ThreadId GetMainThreadId();

        void SetEventHandler(EventHandler handler);

        SharedDebuggableModule GetModule();
        // If the returned pointer is nullptr the Thread does not exist
        std::shared_ptr<Thread> GetThread(ThreadId threadId);

    private:
        EEventKind StepThread(Thread& thread);
        void DispatchEvent(ThreadId threadId, EEventKind kind);

    private:
        EventHandler m_EventHandler;
        std::atomic_bool m_Continue;

        SharedDebuggableModule m_DebuggableModule;
        // TODO: Support multiple reads at the same time. Will be required when multiple threads are supported.
        Pulsar::List<Pulsar::HashMap<size_t, Breakpoint>> m_Breakpoints;

        std::shared_ptr<Thread> m_MainThread;
    };
}

#endif // _PULSARDEBUGGER_DEBUGGER_H
