#ifndef _PULSARDEBUGGER_DEBUGGER_H
#define _PULSARDEBUGGER_DEBUGGER_H

#include <functional>
#include <mutex>
#include <optional>

#include <pulsar/runtime.h>

#include "pulsar-debugger/breakpoint.h"
#include "pulsar-debugger/lock.h"
#include "pulsar-debugger/thread.h"
#include "pulsar-debugger/types.h"

namespace PulsarDebugger
{
    class Debugger : public ILockable<std::recursive_mutex>
    {
    public:
        using LaunchError = Pulsar::String;

        using EEventKind = Thread::EEventKind;
        using EventHandler = std::function<void(ThreadId, EEventKind, Debugger&)>;

    public:
        Debugger();
        ~Debugger() = default;

        Debugger(const Debugger&) = delete;
        Debugger(Debugger&&)      = delete;
        Debugger& operator=(const Debugger&) = delete;
        Debugger& operator=(Debugger&&)      = delete;

        // The Debugger is paused by default on Launch
        std::optional<LaunchError> Launch(
                Pulsar::StringView scriptPath,
                Pulsar::Value::List&& args,
                Pulsar::StringView entryPoint="main");

        Ref<BreakpointRegister> GetBreakpoints();

        void Continue(ThreadId threadId);
        void Pause(ThreadId threadId);

        void StepInstruction(ThreadId threadId);
        void StepOver(ThreadId threadId);
        void StepInto(ThreadId threadId);
        void StepOut(ThreadId threadId);

        void Terminate();

        ThreadId GetMainThreadId();

        void SetEventHandler(EventHandler handler);

        DebuggableModule::CRef GetModule();
        Ref<Thread> SpawnThread(Pulsar::StringView name, Pulsar::StringView entryPoint, Pulsar::Stack&& initStack);
        Ref<Thread> SpawnThread(Pulsar::StringView name, const Pulsar::ExecutionContext& parentContext);
        // If the returned pointer is nullptr the Thread does not exist
        Ref<Thread> GetThread(ThreadId threadId);
        void ForEachThread(std::function<void(Ref<Thread>)> fn);

    private:
        Ref<Thread> SpawnThread(Pulsar::StringView name, Pulsar::ExecutionContext&& context);
        void RemoveThread(ThreadId threadId);
        void DispatchEvent(ThreadId threadId, EEventKind kind);

    private:
        EventHandler m_EventHandler;

        DebuggableModule::CRef m_Module;
        Ref<BreakpointRegister> m_Breakpoints;

        ThreadId m_MainThreadId;
        Pulsar::HashMap<ThreadId, Ref<Thread>> m_Threads;

        std::mutex m_LaunchMutex;
    };
}

#endif // _PULSARDEBUGGER_DEBUGGER_H
