#ifndef _PULSARTOOLS_BINDINGS_THREAD_H
#define _PULSARTOOLS_BINDINGS_THREAD_H

#include <atomic>
#include <condition_variable>
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

        struct Channel
        {
            // FIFO List
            Pulsar::ValueList Pipe;
            bool IsClosed = false;
            std::mutex Mutex;
            std::condition_variable CV;
        };

        class ChannelTypeData : public Pulsar::CustomTypeData
        {
        public:
            int64_t NextHandle = 1;
            Pulsar::HashMap<int64_t, std::shared_ptr<Channel>> Channels;
            std::mutex Mutex;
        };

        void BindToModule(Pulsar::Module& module);

        Pulsar::RuntimeState Thread_Run(Pulsar::ExecutionContext& eContext, uint64_t threadType, uint64_t channelType);
        Pulsar::RuntimeState Thread_Join(Pulsar::ExecutionContext& eContext, uint64_t type);
        Pulsar::RuntimeState Thread_JoinAll(Pulsar::ExecutionContext& eContext, uint64_t type);
        Pulsar::RuntimeState Thread_IsAlive(Pulsar::ExecutionContext& eContext, uint64_t type);
        Pulsar::RuntimeState Thread_IsValid(Pulsar::ExecutionContext& eContext, uint64_t type);

        Pulsar::RuntimeState Channel_New(Pulsar::ExecutionContext& eContext, uint64_t type);
        Pulsar::RuntimeState Channel_Send(Pulsar::ExecutionContext& eContext, uint64_t type);
        Pulsar::RuntimeState Channel_Receive(Pulsar::ExecutionContext& eContext, uint64_t type);
        Pulsar::RuntimeState Channel_Close(Pulsar::ExecutionContext& eContext, uint64_t type);
        Pulsar::RuntimeState Channel_Free(Pulsar::ExecutionContext& eContext, uint64_t type);
        Pulsar::RuntimeState Channel_IsEmpty(Pulsar::ExecutionContext& eContext, uint64_t type);
        Pulsar::RuntimeState Channel_IsClosed(Pulsar::ExecutionContext& eContext, uint64_t type);
        Pulsar::RuntimeState Channel_IsValid(Pulsar::ExecutionContext& eContext, uint64_t type);

        void ThreadJoin(std::shared_ptr<PulsarThread> thread, Pulsar::ValueStack& stack);
    }
}

#endif // _PULSARTOOLS_BINDINGS_THREAD_H
