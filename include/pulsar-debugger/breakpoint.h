#ifndef _PULSARDEBUGGER_BREAKPOINT_H
#define _PULSARDEBUGGER_BREAKPOINT_H

#include <functional>
#include <mutex>
#include <optional>

#include <pulsar/structures/hashmap.h>
#include <pulsar/structures/list.h>

#include "pulsar-debugger/lock.h"
#include "pulsar-debugger/types.h"

namespace PulsarDebugger
{
    using BreakpointError = Pulsar::String;
    struct Breakpoint
    {
        bool Enabled = true;
    };

    class BreakpointRegister : public ILockable<std::recursive_mutex>
    {
    public:
        BreakpointRegister(DebuggableModule::CRef module);
        ~BreakpointRegister() = default;

        DebuggableModule::CRef GetModule();

        std::optional<BreakpointError> Set(SourceReference sourceReference, size_t line);
        void Clear(SourceReference sourceReference);
        void Clear();

        bool Hits(SourceReference sourceReference, size_t line);

    private:
        DebuggableModule::CRef m_Module;
        // TODO: Support multiple reads at the same time. May help with threads.
        Pulsar::List<Pulsar::HashMap<size_t, Breakpoint>> m_Breakpoints;
    };
}

#endif // _PULSARDEBUGGER_BREAKPOINT_H
