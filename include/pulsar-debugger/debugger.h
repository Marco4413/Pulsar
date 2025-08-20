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
        void ForEachThread(std::function<void(std::shared_ptr<Thread>)> fn);

    private:
        struct ContinuePolicy
        {
            enum class EKind { Paused, Continue, StepOver, StepInto, StepOut };

            EKind Kind = EKind::Paused;
            // Required by StepOver, StepInto
            size_t InitSource = 0;
            // Required by StepOver, StepInto
            size_t InitLine = 0;
            // Required by StepOver, StepOut
            size_t InitCallStackSize = 0;
        };

        // A TrackedThread wraps a Thread with additional
        //  information required by the debugger (like execution flags).
        // Anything that attempts to read/write data stored in this struct
        //  MUST lock the Debugger that owns it first.
        struct TrackedThread
        {
            bool IsPaused() const
            {
                return Continue.Kind == ContinuePolicy::EKind::Paused;
            }

            ContinuePolicy Continue;
            std::shared_ptr<PulsarDebugger::Thread> Thread;
        };

    private:
        TrackedThread* GetTrackedThread(ThreadId threadId);
        void ForEachTrackedThread(std::function<void(TrackedThread&)> fn);

        EEventKind StepThread(Thread& thread);
        // Does not call m_ThreadsWaitingToContinue.notify_all(); because
        //  it should only be called after all threads have been processed.
        void ProcessTrackedThread(TrackedThread& trackedThread);
        void DispatchEvent(ThreadId threadId, EEventKind kind);

    private:
        EventHandler m_EventHandler;

        SharedDebuggableModule m_DebuggableModule;
        // TODO: Support multiple reads at the same time. Will be required when multiple threads are supported.
        Pulsar::List<Pulsar::HashMap<size_t, Breakpoint>> m_Breakpoints;

        std::atomic_size_t m_ThreadsWaitingToContinue;
        TrackedThread m_MainThread;
    };
}

#endif // _PULSARDEBUGGER_DEBUGGER_H
