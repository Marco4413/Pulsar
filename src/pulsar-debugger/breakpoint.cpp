#include "pulsar-debugger/breakpoint.h"

namespace PulsarDebugger
{

BreakpointRegister::BreakpointRegister(DebuggableModule::CRef module)
    : m_Module(module)
{
    m_Breakpoints.Reserve(m_Module->Get().SourceDebugSymbols.Size());
}

DebuggableModule::CRef BreakpointRegister::GetModule()
{
    return m_Module;
}

std::optional<BreakpointError> BreakpointRegister::Set(SourceReference sourceReference, size_t line)
{
    ScopeLock _lock(*this);
    m_Breakpoints.Resize(m_Module->Get().SourceDebugSymbols.Size());

    if (sourceReference < 0 || static_cast<size_t>(sourceReference) >= m_Breakpoints.Size())
        return "Got invalid sourceReference.";

    size_t sourceIndex = static_cast<size_t>(sourceReference);
    auto& localBreakpoints = m_Breakpoints[sourceIndex];

    localBreakpoints.Insert(line, Breakpoint{ .Enabled = true });
    // FIXME: Breakpoints won't break on Global Producers since they're evaluated at parse-time.
    if (!m_Module->IsLineReachable(sourceReference, line))
        return "Breakpoint is unreachable.";
    return std::nullopt;
}

void BreakpointRegister::Clear(SourceReference sourceReference)
{
    ScopeLock _lock(*this);
    if (sourceReference < 0 || static_cast<size_t>(sourceReference) >= m_Breakpoints.Size()) return;

    size_t sourceIndex = static_cast<size_t>(sourceReference);
    auto& localBreakpoints = m_Breakpoints[sourceIndex];
    localBreakpoints.Clear();
}

void BreakpointRegister::Clear()
{
    ScopeLock _lock(*this);
    m_Breakpoints.Clear();
}

bool BreakpointRegister::Hits(SourceReference sourceReference, size_t line)
{
    ScopeLock _lock(*this);
    if (sourceReference < 0 || static_cast<size_t>(sourceReference) >= m_Breakpoints.Size()) return false;

    size_t sourceIndex = static_cast<size_t>(sourceReference);
    const auto& localBreakpoints = m_Breakpoints[sourceIndex];

    const auto* breakpoint = localBreakpoints.Find(line);
    if (breakpoint && breakpoint->Value().Enabled) return true;

    return false;
}

}
