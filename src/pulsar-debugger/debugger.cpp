#include "pulsar-debugger/debugger.h"

#include <filesystem>

#include <pulsar/parser.h>

namespace PulsarDebugger
{

Debugger::Debugger()
    : ILockable<std::recursive_mutex>()
    , m_DebuggableModule(nullptr)
    , m_ThreadsWaitingToContinue(0)
    , m_MainThread{ .Continue = ContinuePolicy{}, .Thread = nullptr }
{}

std::optional<Debugger::LaunchError> Debugger::Launch(const char* scriptPath, Pulsar::ValueList&& args, const char* entryPoint)
{
    DebuggerScopeLock _lock(*this);

    m_ThreadsWaitingToContinue = 0;
    m_ThreadsWaitingToContinue.notify_all();

    m_DebuggableModule = nullptr;
    m_Breakpoints.Clear();
    m_MainThread = { .Continue = ContinuePolicy{}, .Thread = nullptr };

    Pulsar::ParseResult parseResult;
    Pulsar::Parser parser;

    // FIXME: AddSourceFile doesn't like when the cwd of the debugger is in a different drive on Windows
    parseResult = parser.AddSourceFile(scriptPath);
    if (parseResult != Pulsar::ParseResult::OK) {
        return "Parser Error: " + parser.GetErrorMessage();
    }

    auto debuggableModule = std::make_shared<DebuggableModule>();
    auto parseSettings    = Pulsar::ParseSettings_Default;
    parseSettings.Notifications = debuggableModule->GetParserNotificationsListener();

    parseResult = parser.ParseIntoModule(debuggableModule->GetModule(), parseSettings);
    if (parseResult != Pulsar::ParseResult::OK) {
        return "Parser Error: " + parser.GetErrorMessage();
    }

    m_DebuggableModule = debuggableModule;
    m_Breakpoints.Resize(m_DebuggableModule->GetModule().SourceDebugSymbols.Size());

    m_MainThread.Thread = std::make_shared<Thread>(m_DebuggableModule);
    ThreadScopeLock _threadLock(*m_MainThread.Thread);    

    args.Prepend()->Value().SetString(scriptPath);
    m_MainThread.Thread->GetContext().GetStack().EmplaceBack().SetList(std::move(args));
    Pulsar::RuntimeState runtimeState = m_MainThread.Thread->GetContext().CallFunction(entryPoint);
    if (runtimeState != Pulsar::RuntimeState::OK) {
        return LaunchError("Runtime Error: ") + Pulsar::RuntimeStateToString(runtimeState);
    }

    return std::nullopt;
}

void Debugger::Continue(ThreadId threadId)
{
    DebuggerScopeLock _lock(*this);
    TrackedThread* trackedThread = GetTrackedThread(threadId);
    if (!trackedThread) return;

    if (trackedThread->IsPaused()) {
        trackedThread->Continue.Kind = ContinuePolicy::EKind::Continue;

        m_ThreadsWaitingToContinue.fetch_add(1);
        m_ThreadsWaitingToContinue.notify_all();
    }

    DispatchEvent(threadId, EEventKind::Continue);
}

void Debugger::Pause(ThreadId threadId)
{
    DebuggerScopeLock _lock(*this);
    TrackedThread* trackedThread = GetTrackedThread(threadId);
    if (!trackedThread) return;

    if (!trackedThread->IsPaused()) {
        trackedThread->Continue.Kind = ContinuePolicy::EKind::Paused;
        m_ThreadsWaitingToContinue.fetch_sub(1);
        m_ThreadsWaitingToContinue.notify_all();
    }

    DispatchEvent(threadId, EEventKind::Pause);
}

std::optional<Debugger::BreakpointError> Debugger::SetBreakpoint(SourceReference sourceReference, size_t line)
{
    DebuggerScopeLock _lock(*this);
    if (sourceReference < 0 || static_cast<size_t>(sourceReference) >= m_Breakpoints.Size())
        return "Got invalid sourceReference.";
    auto& localBreakpoints = m_Breakpoints[sourceReference];
    localBreakpoints.Insert(line, Breakpoint{ .Enabled = true });
    // FIXME: Breakpoints won't break on Global Producers since they're evaluated at parse-time.
    if (!m_DebuggableModule->IsLineReachable(sourceReference, line))
        return "Breakpoint is unreachable.";
    return std::nullopt;
}

void Debugger::ClearBreakpoints(SourceReference sourceReference)
{
    DebuggerScopeLock _lock(*this);
    if (sourceReference < 0 || static_cast<size_t>(sourceReference) >= m_Breakpoints.Size()) return;
    auto& localBreakpoints = m_Breakpoints[sourceReference];
    localBreakpoints.Clear();
}

void Debugger::StepInstruction(ThreadId threadId)
{
    DebuggerScopeLock _lock(*this);
    auto thread = GetThread(threadId);
    if (!thread) return;

    DispatchEvent(threadId, StepThread(*thread));
}

void Debugger::StepOver(ThreadId threadId)
{
    DebuggerScopeLock _lock(*this);
    TrackedThread* trackedThread = GetTrackedThread(threadId);
    if (!trackedThread) return;

    if (trackedThread->IsPaused()) {
        ThreadScopeLock _threadLock(*trackedThread->Thread);
        auto initLine   = trackedThread->Thread->GetOrComputeCurrentLine();
        auto initSource = trackedThread->Thread->GetOrComputeCurrentSourceIndex();
        if (!initLine || !initSource) return StepInstruction(threadId);

        trackedThread->Continue.Kind = ContinuePolicy::EKind::StepOver;
        trackedThread->Continue.InitLine          = *initLine;
        trackedThread->Continue.InitSource        = *initSource;
        trackedThread->Continue.InitCallStackSize = trackedThread->Thread->GetContext().GetCallStack().Size();

        m_ThreadsWaitingToContinue.fetch_add(1);
        m_ThreadsWaitingToContinue.notify_all();
    }
}

void Debugger::StepInto(ThreadId threadId)
{
    DebuggerScopeLock _lock(*this);
    TrackedThread* trackedThread = GetTrackedThread(threadId);
    if (!trackedThread) return;

    if (trackedThread->IsPaused()) {
        ThreadScopeLock _threadLock(*trackedThread->Thread);
        auto initLine   = trackedThread->Thread->GetOrComputeCurrentLine();
        auto initSource = trackedThread->Thread->GetOrComputeCurrentSourceIndex();
        if (!initLine || !initSource) return StepInstruction(threadId);

        trackedThread->Continue.Kind = ContinuePolicy::EKind::StepInto;
        trackedThread->Continue.InitLine   = *initLine;
        trackedThread->Continue.InitSource = *initSource;

        m_ThreadsWaitingToContinue.fetch_add(1);
        m_ThreadsWaitingToContinue.notify_all();
    }
}

void Debugger::StepOut(ThreadId threadId)
{
    DebuggerScopeLock _lock(*this);
    TrackedThread* trackedThread = GetTrackedThread(threadId);
    if (!trackedThread) return;

    if (trackedThread->IsPaused()) {
        ThreadScopeLock _threadLock(*trackedThread->Thread);

        trackedThread->Continue.Kind = ContinuePolicy::EKind::StepOut;
        trackedThread->Continue.InitCallStackSize = trackedThread->Thread->GetContext().GetCallStack().Size();

        m_ThreadsWaitingToContinue.fetch_add(1);
        m_ThreadsWaitingToContinue.notify_all();
    }
}

void Debugger::WaitForEvent()
{
    while (m_ThreadsWaitingToContinue.load() <= 0)
        m_ThreadsWaitingToContinue.wait(0);
}

void Debugger::ProcessEvent()
{
    DebuggerScopeLock _lock(*this);
    ForEachTrackedThread([this](TrackedThread& trackedThread)
    {
        ThreadScopeLock _threadLock(*trackedThread.Thread);
        ProcessTrackedThread(trackedThread);
    });
    m_ThreadsWaitingToContinue.notify_all();
}

ThreadId Debugger::GetMainThreadId()
{
    DebuggerScopeLock _lock(*this);
    if (!m_MainThread.Thread) return 0;
    return m_MainThread.Thread->GetId();
}

void Debugger::SetEventHandler(EventHandler handler)
{
    DebuggerScopeLock _lock(*this);
    m_EventHandler = handler;
}

SharedDebuggableModule Debugger::GetModule()
{
    DebuggerScopeLock _lock(*this);
    return m_DebuggableModule;
}

std::shared_ptr<Thread> Debugger::GetThread(ThreadId threadId)
{
    DebuggerScopeLock _lock(*this);
    TrackedThread* trackedThread = GetTrackedThread(threadId);
    return trackedThread ? trackedThread->Thread : nullptr;
}

void Debugger::ForEachThread(std::function<void(std::shared_ptr<Thread>)> fn)
{
    DebuggerScopeLock _lock(*this);
    ForEachTrackedThread([fn = std::move(fn)](TrackedThread& trackedThread)
    {
        fn(trackedThread.Thread);
    });
}

Debugger::TrackedThread* Debugger::GetTrackedThread(ThreadId threadId)
{
    return threadId ==  GetMainThreadId() && m_MainThread.Thread
        ? &m_MainThread : nullptr;
}

void Debugger::ForEachTrackedThread(std::function<void(TrackedThread&)> fn)
{
    if (m_MainThread.Thread)
        fn(m_MainThread);
}

Debugger::EEventKind Debugger::StepThread(Thread& thread)
{
    // These are required to not hit the same breakpoint multiple times
    auto initLine   = thread.GetOrComputeCurrentLine();
    auto initSource = thread.GetOrComputeCurrentSourceIndex();

    auto stepResult = thread.Step();
    switch (stepResult) {
    case Thread::EStatus::Done:
        return EEventKind::Done;
    case Thread::EStatus::Error:
        return EEventKind::Error;
    case Thread::EStatus::Step:
        break;
    }

    auto currentSource = thread.GetOrComputeCurrentSourceIndex();
    if (currentSource && *currentSource < m_Breakpoints.Size()) {
        auto currentLine = thread.GetOrComputeCurrentLine();
        if (currentLine && (currentLine != initLine || currentSource != initSource)) {
            const auto& localBreakpoints = m_Breakpoints[*currentSource];
            const auto* breakpoint = localBreakpoints.Find(*currentLine);
            if (breakpoint && breakpoint->Value().Enabled) return EEventKind::Breakpoint;
        }
    }

    return EEventKind::Step;
}

void Debugger::ProcessTrackedThread(TrackedThread& trackedThread)
{
    if (trackedThread.IsPaused() || !trackedThread.Thread) return;

    switch (trackedThread.Continue.Kind) {
    case ContinuePolicy::EKind::Continue: {
        EEventKind ev = StepThread(*trackedThread.Thread);
        if (ev != EEventKind::Step) {
            trackedThread.Continue.Kind = ContinuePolicy::EKind::Paused;
            m_ThreadsWaitingToContinue.fetch_sub(1);
            DispatchEvent(trackedThread.Thread->GetId(), ev);
        }
    } break;
    case ContinuePolicy::EKind::StepOver: {
        EEventKind ev = StepThread(*trackedThread.Thread);
        if (ev != EEventKind::Step) {
            trackedThread.Continue.Kind = ContinuePolicy::EKind::Paused;
            m_ThreadsWaitingToContinue.fetch_sub(1);
            DispatchEvent(trackedThread.Thread->GetId(), ev);
            break;
        }

        auto currentLine = trackedThread.Thread->GetOrComputeCurrentLine();
        auto currentSourceIndex = trackedThread.Thread->GetOrComputeCurrentSourceIndex();

        bool shouldContinue = trackedThread.Thread->GetContext().GetCallStack().Size() > trackedThread.Continue.InitCallStackSize || (
            currentLine && currentSourceIndex &&
            *currentLine == trackedThread.Continue.InitLine &&
            *currentSourceIndex == trackedThread.Continue.InitSource
        );

        if (!shouldContinue) {
            trackedThread.Continue.Kind = ContinuePolicy::EKind::Paused;
            m_ThreadsWaitingToContinue.fetch_sub(1);
            DispatchEvent(trackedThread.Thread->GetId(), EEventKind::Step);
        }
    } break;
    case ContinuePolicy::EKind::StepInto: {
        EEventKind ev = StepThread(*trackedThread.Thread);
        if (ev != EEventKind::Step) {
            trackedThread.Continue.Kind = ContinuePolicy::EKind::Paused;
            m_ThreadsWaitingToContinue.fetch_sub(1);
            DispatchEvent(trackedThread.Thread->GetId(), ev);
            break;
        }

        auto currentLine = trackedThread.Thread->GetOrComputeCurrentLine();
        auto currentSourceIndex = trackedThread.Thread->GetOrComputeCurrentSourceIndex();

        bool shouldContinue = (
            currentLine && currentSourceIndex &&
            *currentLine == trackedThread.Continue.InitLine &&
            *currentSourceIndex == trackedThread.Continue.InitSource
        );

        if (!shouldContinue) {
            trackedThread.Continue.Kind = ContinuePolicy::EKind::Paused;
            m_ThreadsWaitingToContinue.fetch_sub(1);
            DispatchEvent(trackedThread.Thread->GetId(), EEventKind::Step);
        }
    } break;
    case ContinuePolicy::EKind::StepOut: {
        EEventKind ev = StepThread(*trackedThread.Thread);
        if (ev != EEventKind::Step) {
            trackedThread.Continue.Kind = ContinuePolicy::EKind::Paused;
            m_ThreadsWaitingToContinue.fetch_sub(1);
            DispatchEvent(trackedThread.Thread->GetId(), ev);
            break;
        }

        bool shouldContinue = trackedThread.Thread->GetContext().GetCallStack().Size() >= trackedThread.Continue.InitCallStackSize;

        if (!shouldContinue) {
            trackedThread.Continue.Kind = ContinuePolicy::EKind::Paused;
            m_ThreadsWaitingToContinue.fetch_sub(1);
            DispatchEvent(trackedThread.Thread->GetId(), EEventKind::Step);
        }
    } break;
    default:
        PULSAR_ASSERT(false, "Unhandled trackedThread.Continue.Kind");
    }
}

void Debugger::DispatchEvent(ThreadId threadId, EEventKind kind)
{
    if (m_EventHandler)
        m_EventHandler(threadId, kind, *this);
}

}
