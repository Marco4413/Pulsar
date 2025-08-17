#ifndef _PULSARDEBUGGER_THREAD_H
#define _PULSARDEBUGGER_THREAD_H

#include <memory>

#include <pulsar/runtime.h>

#include "pulsar-debugger/lock.h"
#include "pulsar-debugger/types.h"

namespace PulsarDebugger
{
    using ThreadScopeLock = ScopeLock<std::recursive_mutex>;
    class Thread : public ILockable<std::recursive_mutex>
    {
    public:
        enum class EStatus { Step, Done, Error };
    public:
        Thread(SharedDebuggableModule mod);
        ~Thread() = default;

        ThreadId GetId() const;
        // You should lock this Thread before changing anything within the context
        Pulsar::ExecutionContext& GetContext();
        const Pulsar::ExecutionContext& GetContext() const;

        EStatus Step();

        Pulsar::RuntimeState GetCurrentState();
        std::optional<size_t> GetOrComputeCurrentLine();
        std::optional<size_t> GetOrComputeCurrentSourceIndex();

    private:
        std::optional<size_t> ComputeCurrentLine();
        std::optional<size_t> ComputeCurrentSourceIndex();

    private:
        SharedDebuggableModule m_DebuggableModule;
        std::unique_ptr<Pulsar::ExecutionContext> m_ExecutionContext;
        const ThreadId m_Id;

        std::optional<size_t> m_CachedCurrentLine;
        std::optional<size_t> m_CachedCurrentSource;
    };
}

#endif // _PULSARDEBUGGER_THREAD_H
