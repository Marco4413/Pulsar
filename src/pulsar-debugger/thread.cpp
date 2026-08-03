#include "pulsar-debugger/thread.h"

#include "pulsar-debugger/debugger.h"

namespace PulsarDebugger
{

Thread::Thread(Pulsar::StringView name, Ref<BreakpointRegister> breakpoints, Pulsar::ExecutionContext&& context)
    : m_Breakpoints(breakpoints)
    , m_ExecutionContext(std::make_unique<Pulsar::ExecutionContext>(std::forward<Pulsar::ExecutionContext>(context)))
    , m_Id(ComputeThreadId(*m_ExecutionContext))
    , m_Name(name.Data(), name.Length())
    , m_EventHandler(nullptr)
    , m_Alive(false)
    , m_Paused(true)
    , m_RequestHandled(false)
    , m_StepRequest(StepRequest{StepRequest::EKind::Pause})
    , m_CurrentP(std::nullopt)
{
    PULSAR_ASSERT(
        &breakpoints->GetModule()->Get() == &m_ExecutionContext->GetModule(),
        "Given Module does not match the one in the ExecutionContext.");
}

Thread::Thread(Pulsar::StringView name, Ref<BreakpointRegister> breakpoints)
    : Thread(name, breakpoints, Pulsar::ExecutionContext(breakpoints->GetModule()->Get()))
{}

Thread::Thread(Pulsar::StringView name, Ref<BreakpointRegister> breakpoints, const Pulsar::ExecutionContext& parentContext)
    : Thread(name, breakpoints, parentContext.Fork())
{}

Thread::~Thread()
{
}

void Thread::SetEventHandler(EventHandler handler)
{
    ScopeLock _lock(*this);
    m_EventHandler = handler;
}

const Pulsar::String& Thread::GetName() const
{
    return m_Name;
}

void Thread::Start()
{
    m_Thread = std::thread([this]() { this->Runner(); });
}

bool Thread::IsAlive() const
{
    return m_Alive;
}

bool Thread::IsPaused() const
{
    return !IsAlive() || m_Paused;
}

ThreadId Thread::GetId() const
{
    return m_Id;
}

Pulsar::ExecutionContext& Thread::GetContext()
{
    return *m_ExecutionContext;
}

const Pulsar::ExecutionContext& Thread::GetContext() const
{
    return *m_ExecutionContext;
}

bool Thread::Join(std::chrono::milliseconds timeout)
{
    std::chrono::milliseconds totalTime(0);
    std::chrono::milliseconds pollInterval = timeout / 4;

    while (m_Alive) {
        if (totalTime >= timeout) return false;
        std::this_thread::sleep_for(pollInterval);
        totalTime += pollInterval;
    }

    m_Thread.join();
    return true;
}

void Thread::Continue()
{
    if (!IsPaused()) return;
    StepRequest request{};
    request.Kind = StepRequest::EKind::Continue;
    m_StepRequest.Store(request);
    m_RequestHandled = false;
    m_RequestHandled.notify_all();
}

void Thread::Pause()
{
    StepRequest request{};
    request.Kind = StepRequest::EKind::Pause;
    m_StepRequest.Store(request);
    m_RequestHandled = false;
    m_RequestHandled.notify_all();
}

void Thread::StepInstruction()
{
    if (!IsPaused()) return;
    StepRequest request{};
    request.Kind = StepRequest::EKind::StepInstruction;
    m_StepRequest.Store(request);
    m_RequestHandled = false;
    m_RequestHandled.notify_all();
}

void Thread::StepOver()
{
    if (!IsPaused()) return;
    ScopeLock _lock(*this);

    auto initPosition = GetCurrentPosition();
    if (!initPosition || !initPosition->Line) return StepInstruction();

    StepRequest request{};
    request.Kind = StepRequest::EKind::StepOver;
    request.InitSource        = initPosition->SourceIndex;
    request.InitLine          = *initPosition->Line;
    request.InitCallStackSize = GetContext().GetCallStack().Size();
    m_StepRequest.Store(request);
    m_RequestHandled = false;
    m_RequestHandled.notify_all();
}

void Thread::StepInto()
{
    if (!IsPaused()) return;
    ScopeLock _lock(*this);

    auto initPosition = GetCurrentPosition();
    if (!initPosition || !initPosition->Line) return StepInstruction();

    StepRequest request{};
    request.Kind = StepRequest::EKind::StepInto;
    request.InitSource = initPosition->SourceIndex;
    request.InitLine   = *initPosition->Line;
    m_StepRequest.Store(request);
    m_RequestHandled = false;
    m_RequestHandled.notify_all();
}

void Thread::StepOut()
{
    if (!IsPaused()) return;
    ScopeLock _lock(*this);

    StepRequest request{};
    request.Kind = StepRequest::EKind::StepOut;
    request.InitCallStackSize = GetContext().GetCallStack().Size();
    m_StepRequest.Store(request);
    m_RequestHandled = false;
    m_RequestHandled.notify_all();
}

std::optional<size_t> Thread::ComputeCurrentSourceIndex()
{
    if (m_ExecutionContext->IsDone()) return std::nullopt;

    const Pulsar::Frame& frame = m_ExecutionContext->CurrentFrame();
    if (!frame.Function || !frame.Function->HasDebugSymbol()) return std::nullopt;

    return frame.Function->DebugSymbol.SourceIdx;
}

std::optional<size_t> Thread::ComputeCurrentLine()
{
    if (m_ExecutionContext->IsDone()) return std::nullopt;

    const Pulsar::Frame& frame = m_ExecutionContext->CurrentFrame();
    if (!frame.Function || !frame.Function->HasDebugSymbol())
        return std::nullopt;

    if (frame.IsNative) {
        return frame.Function->DebugSymbol.Token.SourcePos.Line;
    }

    if (!frame.Function->HasCodeDebugSymbols())
        return std::nullopt;

    size_t instrIdx = frame.InstructionIndex;
    if (m_ExecutionContext->GetState() != Pulsar::RuntimeState::OK) { /* Callee error */
        if (instrIdx > 0) --instrIdx;
    }

    size_t dbgSymbolIdx;
    if (!frame.Function->FindCodeDebugSymbolFor(instrIdx, dbgSymbolIdx))
        return std::nullopt;

    return frame.Function->CodeDebugSymbols[dbgSymbolIdx].Token.SourcePos.Line;
}

std::optional<Thread::Position> Thread::ComputeCurrentPosition()
{
    auto sourceIndex = ComputeCurrentSourceIndex();
    if (!sourceIndex) return std::nullopt;
    auto line = ComputeCurrentLine();
    return Position{ .SourceIndex = *sourceIndex, .Line = line };
}

void Thread::RecomputeCurrentPosition()
{
    ScopeLock _lock(*this);
    m_CurrentP.Store(ComputeCurrentPosition());
}

std::optional<Thread::Position> Thread::GetCurrentPosition()
{
    return m_CurrentP.Load();
}

void Thread::DispatchEvent(EEventKind event)
{
    if (m_EventHandler) m_EventHandler(GetId(), event);
}

void Thread::HandleRequest()
{
    if (m_RequestHandled) return;
    ScopeLock _lock(*this);
    StepRequest request = m_StepRequest.Load();

    bool shouldContinue = true;

    switch (request.Kind) {
    case StepRequest::EKind::Pause:
        m_RequestHandled = true;
        m_RequestHandled.notify_all();
        m_Paused = true;
        DispatchEvent(EEventKind::Pause);
        return;
    case StepRequest::EKind::Continue:
        m_RequestHandled = true;
        m_RequestHandled.notify_all();
        DispatchEvent(EEventKind::Continue);
        m_Paused = false;
        return;
    case StepRequest::EKind::StepInstruction: {
        static bool step1 = false;
        static bool step2 = false;
        if (step1 == step2) {
            shouldContinue = true;
            step2 = true;
        } else {
            shouldContinue = false;
            step2 = step1;
        }
    } break;
    case StepRequest::EKind::StepOver: {
        auto position = GetCurrentPosition();
        shouldContinue = GetContext().GetCallStack().Size() > request.InitCallStackSize || (
               position && position->Line
            && position->SourceIndex == request.InitSource
            && *position->Line == request.InitLine
        );
    } break;
    case StepRequest::EKind::StepInto: {
        auto position = GetCurrentPosition();
        shouldContinue = (
               position && position->Line
            && position->SourceIndex == request.InitSource
            && *position->Line == request.InitLine
        );
    } break;
    case StepRequest::EKind::StepOut: {
        shouldContinue = GetContext().GetCallStack().Size() >= request.InitCallStackSize;
    } break;
    default:
        PULSAR_ASSERT(false, "Unhandled trackedThread.Continue.Kind");
    }

    m_Paused = !shouldContinue;
    if (!shouldContinue) {
        m_RequestHandled = true;
        m_RequestHandled.notify_all();
        DispatchEvent(EEventKind::Step);
    }
}

void Thread::HandleBreakpoints(std::optional<Position> prevPausePosition)
{
    ScopeLock _lock(*this);
    auto position = GetCurrentPosition();
    if (!position || !position->Line) return;
    if (position == prevPausePosition) return;
    if (!m_Breakpoints->Hits(position->SourceIndex, *position->Line)) return;

    // Mark any request as handled
    m_RequestHandled = true;
    m_RequestHandled.notify_all();
    m_Paused = true;
    DispatchEvent(EEventKind::Breakpoint);
}

void Thread::Runner()
{
    m_Alive = true;
    std::optional<Position> prevPausePosition;
    RecomputeCurrentPosition();
    while (true) {
        HandleRequest();
        if (m_Paused) {
            prevPausePosition = GetCurrentPosition();
            m_RequestHandled.wait(true);
            continue;
        }

        HandleBreakpoints(prevPausePosition);
        if (m_Paused) {
            continue;
        }

        m_Paused = false;

        ScopeLock _lock(*this);
        auto state = this->m_ExecutionContext->GetState();
        if (state != Pulsar::RuntimeState::OK || this->m_ExecutionContext->IsDone()) {
            m_CurrentP.Store(std::nullopt);
            DispatchEvent(EEventKind::Done);
            break;
        }

        state = m_ExecutionContext->Step();
        RecomputeCurrentPosition();

        if (state != Pulsar::RuntimeState::OK) {
            DispatchEvent(EEventKind::Error);
            break;
        } else if (m_ExecutionContext->IsDone()) {
            DispatchEvent(EEventKind::Done);
            break;
        }
    }
    m_Alive = false;
}

}
