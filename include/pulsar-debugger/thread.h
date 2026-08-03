#ifndef _PULSARDEBUGGER_THREAD_H
#define _PULSARDEBUGGER_THREAD_H

#include <atomic>
#include <memory>
#include <thread>

#include <pulsar/runtime.h>

#include "pulsar-debugger/breakpoint.h"
#include "pulsar-debugger/lock.h"
#include "pulsar-debugger/types.h"

namespace PulsarDebugger
{
    class Thread : public ILockable<std::recursive_mutex>
    {
    public:
        enum class EEventKind { Step, Breakpoint, Continue, Pause, Done, Error };
        using EventHandler = std::function<void(ThreadId, EEventKind)>;

    public:
        Thread(Pulsar::StringView name, Ref<BreakpointRegister> breakpoints);
        Thread(Pulsar::StringView name, Ref<BreakpointRegister> breakpoints, const Pulsar::ExecutionContext& parentContext);
        Thread(Pulsar::StringView name, Ref<BreakpointRegister> breakpoints, Pulsar::ExecutionContext&& context);
        ~Thread();

        void SetEventHandler(EventHandler handler);
        const Pulsar::String& GetName() const;

        void Start();
        bool IsAlive() const;
        bool IsPaused() const;

        ThreadId GetId() const;
        // You should lock this Thread before any read/write to the context
        Pulsar::ExecutionContext& GetContext();
        const Pulsar::ExecutionContext& GetContext() const;

        bool Join(std::chrono::milliseconds timeout);

        void Continue();
        void Pause();

        void StepInstruction();
        void StepOver();
        void StepInto();
        void StepOut();

    private:
        struct Position
        {
            size_t SourceIndex;
            std::optional<size_t> Line;

            // FIXME: why isn't this already implemented?
            bool operator==(const Position& other) const = default;
        };

        struct StepRequest
        {
            enum class EKind { Pause, Continue, StepInstruction, StepOver, StepInto, StepOut };

            EKind Kind = EKind::Pause;
            // Required by StepOver, StepInto
            size_t InitSource = 0;
            // Required by StepOver, StepInto
            size_t InitLine = 0;
            // Required by StepOver, StepOut
            size_t InitCallStackSize = 0;

            // FIXME: why isn't this already implemented?
            bool operator==(const StepRequest& other) const = default;
        };

    private:
        std::optional<size_t> ComputeCurrentSourceIndex();
        std::optional<size_t> ComputeCurrentLine();
        std::optional<Position> ComputeCurrentPosition();

        void RecomputeCurrentPosition();
        std::optional<Position> GetCurrentPosition();

        void DispatchEvent(EEventKind event);
        void HandleRequest();
        void HandleBreakpoints(std::optional<Position> prevPausePosition);
        void Runner();

    private:
        // Also stores the Module used.
        Ref<BreakpointRegister> m_Breakpoints;

        std::unique_ptr<Pulsar::ExecutionContext> m_ExecutionContext;

        const ThreadId m_Id;
        const Pulsar::String m_Name;

        EventHandler m_EventHandler;
        std::atomic_bool m_Alive;
        std::atomic_bool m_Paused;
        std::atomic_bool m_RequestHandled;
        LockedValue<StepRequest> m_StepRequest;
        std::thread m_Thread;

        LockedValue<std::optional<Position>> m_CurrentP;
    };
}

#endif // _PULSARDEBUGGER_THREAD_H
