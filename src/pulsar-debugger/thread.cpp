#include "pulsar-debugger/thread.h"

namespace PulsarDebugger
{

Thread::Thread(SharedDebuggableModule mod)
    : m_DebuggableModule(mod)
    , m_ExecutionContext(std::make_unique<Pulsar::ExecutionContext>(mod->GetModule()))
    , m_Id(ComputeThreadId(*m_ExecutionContext))
    , m_CachedCurrentLine(std::nullopt)
    , m_CachedCurrentSource(std::nullopt)
{
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

Pulsar::RuntimeState Thread::GetCurrentState()
{
    ThreadScopeLock _lock(*this);
    return m_ExecutionContext->GetState();
}

std::optional<size_t> Thread::GetOrComputeCurrentLine()
{
    ThreadScopeLock _lock(*this);
    if (!m_CachedCurrentLine) m_CachedCurrentLine = ComputeCurrentLine();
    return m_CachedCurrentLine;
}

std::optional<size_t> Thread::GetOrComputeCurrentSourceIndex()
{
    ThreadScopeLock _lock(*this);
    if (!m_CachedCurrentSource) m_CachedCurrentSource = ComputeCurrentSourceIndex();
    return m_CachedCurrentSource;
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

std::optional<size_t> Thread::ComputeCurrentSourceIndex()
{
    if (m_ExecutionContext->IsDone()) return std::nullopt;

    const Pulsar::Frame& frame = m_ExecutionContext->CurrentFrame();
    if (!frame.Function || !frame.Function->HasDebugSymbol()) return std::nullopt;

    return frame.Function->DebugSymbol.SourceIdx;
}

Thread::EStatus Thread::Step()
{
    ThreadScopeLock _lock(*this);

    // Invalidate cache
    m_CachedCurrentLine = std::nullopt;
    m_CachedCurrentSource = std::nullopt;

    auto state = m_ExecutionContext->GetState();
    if (state != Pulsar::RuntimeState::OK || m_ExecutionContext->IsDone())
        return EStatus::Done;

    state = m_ExecutionContext->Step();
    if (state != Pulsar::RuntimeState::OK)
        return EStatus::Error;
    else if (m_ExecutionContext->IsDone())
        return EStatus::Done;

    return EStatus::Step;
}

}
