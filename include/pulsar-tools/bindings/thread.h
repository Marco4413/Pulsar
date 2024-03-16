#ifndef _PULSARTOOLS_BINDINGS_THREAD_H
#define _PULSARTOOLS_BINDINGS_THREAD_H

#include <atomic>
#include <mutex>
#include <thread>

#include "pulsar-tools/core.h"

#include "pulsar/runtime.h"
#include "pulsar/structures/hashmap.h"

namespace PulsarTools
{
    namespace ThreadNativeBindings
    {
        struct PulsarThreadContext
        {
            Pulsar::ExecutionContext Context;
            Pulsar::ValueStack Stack;
            Pulsar::RuntimeState State;
            std::atomic_bool IsRunning = false;
        };

        struct PulsarThread
        {
            std::thread Thread;
            std::shared_ptr<PulsarThreadContext> ThreadContext;
        };

        class ThreadTypeData : public Pulsar::CustomTypeData
        {
        public:
            int64_t NextHandle = 1;
            Pulsar::HashMap<int64_t, std::shared_ptr<PulsarThread>> Threads;
        };

        void BindToModule(Pulsar::Module& module);

        Pulsar::RuntimeState Thread_Run(Pulsar::ExecutionContext& eContext, uint64_t type);
        Pulsar::RuntimeState Thread_Join(Pulsar::ExecutionContext& eContext, uint64_t type);
        Pulsar::RuntimeState Thread_JoinAll(Pulsar::ExecutionContext& eContext, uint64_t type);

        Pulsar::RuntimeState Thread_IsAlive(Pulsar::ExecutionContext& eContext, uint64_t type);
        Pulsar::RuntimeState Thread_IsValid(Pulsar::ExecutionContext& eContext, uint64_t type);

        Pulsar::ValueList ThreadJoin(std::shared_ptr<PulsarThread> thread);
    }
}

#endif // _PULSARTOOLS_BINDINGS_THREAD_H
