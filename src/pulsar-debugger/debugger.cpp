#include "pulsar-debugger/debugger.h"

#include <filesystem>

#include <pulsar/parser.h>

namespace PulsarDebugger
{

DebuggerScopeLock::DebuggerScopeLock(Debugger &debugger)
    : m_Lock(debugger.m_Mutex) {}

void DebuggerScopeLock::Lock()
{
    m_Lock.lock();
}

void DebuggerScopeLock::Unlock()
{
    m_Lock.unlock();
}

Debugger::Debugger()
    : m_Module(nullptr)
    , m_ThreadId(-1)
    , m_Thread(nullptr) {}

std::optional<Debugger::LaunchError> Debugger::Launch(const char* scriptPath, Pulsar::ValueList&& args, const char* entryPoint)
{
    DebuggerScopeLock _lock(*this);

    m_Module   = nullptr;
    m_Breakpoints.Clear();
    m_ThreadId = -1;
    m_Thread   = nullptr;

    Pulsar::ParseResult parseResult;
    Pulsar::Parser parser;

    // FIXME: AddSourceFile doesn't like when the cwd of the debugger is in a different drive on Windows
    parseResult = parser.AddSourceFile(scriptPath);
    if (parseResult != Pulsar::ParseResult::OK) {
        return "Parser Error: " + parser.GetErrorMessage();
    }

    auto pulsarModule = std::make_shared<Pulsar::Module>();
    // Null Source
    pulsarModule->SourceDebugSymbols.EmplaceBack();

    parseResult = parser.ParseIntoModule(*pulsarModule);
    if (parseResult != Pulsar::ParseResult::OK) {
        return "Parser Error: " + parser.GetErrorMessage();
    }

    m_Module   = pulsarModule;
    m_Breakpoints.Resize(m_Module->SourceDebugSymbols.Size());
    m_Thread   = std::make_unique<Pulsar::ExecutionContext>(*m_Module);
    m_ThreadId = DebuggerContext::ComputeThreadId(*m_Thread);

    args.Prepend()->Value().SetString(scriptPath);
    m_Thread->GetStack().EmplaceBack().SetList(std::move(args));
    Pulsar::RuntimeState runtimeState = m_Thread->CallFunction(entryPoint);
    if (runtimeState != Pulsar::RuntimeState::OK) {
        return LaunchError("Runtime Error: ") + Pulsar::RuntimeStateToString(runtimeState);
    }

    return std::nullopt;
}

void Debugger::Continue()
{
    DebuggerScopeLock _lock(*this);
    m_Continue = true;
    m_Continue.notify_all();
    DispatchEvent(m_ThreadId, EventKind::Continue);
}

void Debugger::Pause()
{
    DebuggerScopeLock _lock(*this);
    m_Continue = false;
    m_Continue.notify_all();
    DispatchEvent(m_ThreadId, EventKind::Pause);
}

std::optional<Debugger::BreakpointError> Debugger::SetBreakpoint(DebuggerContext::SourceReferenceId sourceReference, size_t line)
{
    DebuggerScopeLock _lock(*this);
    if (sourceReference < 0 || static_cast<size_t>(sourceReference) >= m_Breakpoints.Size())
        return "Got invalid sourceReference.";
    auto& localBreakpoints = m_Breakpoints[sourceReference];
    localBreakpoints.Insert(line, Breakpoint{ .Enabled = true });
    // TODO: Check if breakpoint is in a valid line
    return std::nullopt;
}

void Debugger::ClearBreakpoints(DebuggerContext::SourceReferenceId sourceReference)
{
    DebuggerScopeLock _lock(*this);
    if (sourceReference < 0 || static_cast<size_t>(sourceReference) >= m_Breakpoints.Size()) return;
    auto& localBreakpoints = m_Breakpoints[sourceReference];
    localBreakpoints.Clear();
}

void Debugger::StepInstruction()
{
    DebuggerScopeLock _lock(*this);
    DispatchEvent(m_ThreadId, InternalStep());
}

void Debugger::StepOver()
{
    DebuggerScopeLock _lock(*this);
    auto initLine   = GetOrComputeCurrentLine(m_ThreadId);
    auto initSource = GetOrComputeCurrentSourceIndex(m_ThreadId);
    if (!initLine || !initSource) return StepInstruction();

    size_t initStackSize = m_Thread->GetCallStack().Size();

    do {
        EventKind ev = InternalStep();
        if (ev != EventKind::Step)
            return DispatchEvent(m_ThreadId, ev);
    } while (m_Thread->GetCallStack().Size() > initStackSize || (GetOrComputeCurrentLine(m_ThreadId) == initLine && GetOrComputeCurrentSourceIndex(m_ThreadId) == initSource));

    // Send a single step event once the line was completed
    DispatchEvent(m_ThreadId, EventKind::Step);
}

void Debugger::StepInto()
{
    DebuggerScopeLock _lock(*this);
    auto initLine   = GetOrComputeCurrentLine(m_ThreadId);
    auto initSource = GetOrComputeCurrentSourceIndex(m_ThreadId);
    if (!initLine || !initSource) return StepInstruction();

    do {
        EventKind ev = InternalStep();
        if (ev != EventKind::Step)
            return DispatchEvent(m_ThreadId, ev);
    } while (GetOrComputeCurrentLine(m_ThreadId) == initLine && GetOrComputeCurrentSourceIndex(m_ThreadId) == initSource);
    
    // Send a single step event once the line was completed
    DispatchEvent(m_ThreadId, EventKind::Step);
}

void Debugger::StepOut()
{
    DebuggerScopeLock _lock(*this);
    size_t initStackSize = m_Thread->GetCallStack().Size();

    do {
        EventKind ev = InternalStep();
        if (ev != EventKind::Step)
            return DispatchEvent(m_ThreadId, ev);
    } while (m_Thread->GetCallStack().Size() >= initStackSize);
    
    // Send a single step event once the line was completed
    DispatchEvent(m_ThreadId, EventKind::Step);
}

void Debugger::WaitForEvent()
{
    while (!m_Continue) m_Continue.wait(false);
}

void Debugger::ProcessEvent()
{
    DebuggerScopeLock _lock(*this);
    if (m_Continue) {
        EventKind ev = InternalStep();
        if (ev != EventKind::Step) {
            m_Continue = false;
            m_Continue.notify_all();
            DispatchEvent(m_ThreadId, ev);
        }
    }
}

DebuggerContext::ThreadId Debugger::GetMainThreadId()
{
    DebuggerScopeLock _lock(*this);
    return m_ThreadId;
}

void Debugger::SetEventHandler(EventHandler handler)
{
    m_EventHandler = handler;
}

std::optional<Pulsar::RuntimeState> Debugger::GetCurrentState(DebuggerContext::ThreadId threadId)
{
    DebuggerScopeLock _lock(*this);
    if (!m_Module || !m_Thread) return std::nullopt;
    if (threadId != m_ThreadId) return std::nullopt;
    return m_Thread->GetState();
}

std::optional<size_t> Debugger::GetOrComputeCurrentLine(DebuggerContext::ThreadId threadId)
{
    DebuggerScopeLock _lock(*this);
    if (threadId != m_ThreadId) return ComputeCurrentLine(threadId);
    if (!m_CachedCurrentLine) m_CachedCurrentLine = ComputeCurrentLine(threadId);
    return m_CachedCurrentLine;
}

std::optional<size_t> Debugger::GetOrComputeCurrentSourceIndex(DebuggerContext::ThreadId threadId)
{
    DebuggerScopeLock _lock(*this);
    if (threadId != m_ThreadId) return ComputeCurrentSourceIndex(threadId);
    if (!m_CachedCurrentSource) m_CachedCurrentSource = ComputeCurrentSourceIndex(threadId);
    return m_CachedCurrentSource;
}

std::optional<size_t> Debugger::ComputeCurrentLine(DebuggerContext::ThreadId threadId)
{
    if (!m_Module || !m_Thread) return std::nullopt;
    if (threadId != m_ThreadId || m_Thread->IsDone()) return std::nullopt;

    const Pulsar::Frame& frame = m_Thread->CurrentFrame();
    if (!frame.Function || !frame.Function->HasDebugSymbol())
        return std::nullopt;

    if (frame.IsNative) {
        return frame.Function->DebugSymbol.Token.SourcePos.Line;
    }

    if (!frame.Function->HasCodeDebugSymbols())
        return std::nullopt;

    size_t instrIdx = frame.InstructionIndex;
    if (m_Thread->GetState() != Pulsar::RuntimeState::OK) { /* Callee error */
        if (instrIdx > 0) --instrIdx;
    }

    size_t dbgSymbolIdx;
    if (!frame.Function->FindCodeDebugSymbolFor(instrIdx, dbgSymbolIdx))
        return std::nullopt;

    return frame.Function->CodeDebugSymbols[dbgSymbolIdx].Token.SourcePos.Line;
}

std::optional<size_t> Debugger::ComputeCurrentSourceIndex(DebuggerContext::ThreadId threadId)
{
    if (!m_Module || !m_Thread) return std::nullopt;
    if (threadId != m_ThreadId || m_Thread->IsDone()) return std::nullopt;

    const Pulsar::Frame& frame = m_Thread->CurrentFrame();
    if (!frame.Function || !frame.Function->HasDebugSymbol()) return std::nullopt;

    return frame.Function->DebugSymbol.SourceIdx;
}

std::shared_ptr<const DebuggerContext> Debugger::GetOrComputeContext()
{
    DebuggerScopeLock _lock(*this);
    if (!m_Module) return nullptr;
    if (m_Context) return m_Context;

    DebuggerContext context(m_Module);
    context.CreateThread(*m_Thread);

    m_Context = std::make_shared<const DebuggerContext>(std::move(context));
    return m_Context;
}

std::optional<Pulsar::SourceDebugSymbol> Debugger::GetSource(DebuggerContext::SourceReferenceId sourceReference)
{
    DebuggerScopeLock _lock(*this);
    if (!m_Module) return std::nullopt;
    DebuggerContext shallowContext(m_Module);
    return shallowContext.GetSource(sourceReference);
}

Debugger::EventKind Debugger::InternalStep()
{
    // Invalidate context
    m_Context = nullptr;
    m_CachedCurrentLine = std::nullopt;
    m_CachedCurrentSource = std::nullopt;

    auto state = m_Thread->GetState();
    if (state != Pulsar::RuntimeState::OK || m_Thread->IsDone())
        return EventKind::Done;

    // These are required to not hit the same breakpoint multiple times
    auto initLine   = ComputeCurrentLine(m_ThreadId);
    auto initSource = ComputeCurrentSourceIndex(m_ThreadId);

    state = m_Thread->Step();
    if (state != Pulsar::RuntimeState::OK)
        return EventKind::Error;
    else if (m_Thread->IsDone())
        return EventKind::Done;

    auto currentSource = GetOrComputeCurrentSourceIndex(m_ThreadId);
    if (currentSource && *currentSource < m_Breakpoints.Size()) {
        auto currentLine = GetOrComputeCurrentLine(m_ThreadId);
        if (currentLine && (currentLine != initLine || currentSource != initSource)) {
            const auto& localBreakpoints = m_Breakpoints[*currentSource];
            const auto* breakpoint = localBreakpoints.Find(*currentLine);
            if (breakpoint && breakpoint->Value().Enabled) return EventKind::Breakpoint;
        }
    }

    return EventKind::Step;
}

void Debugger::DispatchEvent(DebuggerContext::ThreadId threadId, EventKind kind)
{
    if (m_EventHandler)
        m_EventHandler(threadId, kind, *this);
}

}
