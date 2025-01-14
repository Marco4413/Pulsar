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
            std::atomic_bool IsRunning = false;
        };

        struct PulsarThread
        {
            std::thread Thread;
            Pulsar::SharedRef<PulsarThreadContext> ThreadContext;
        };

        class ThreadType : public Pulsar::CustomDataHolder, public PulsarThread
        {
        public:
            using Ref = Pulsar::SharedRef<ThreadType>;
            ThreadType(std::thread&& thread, Pulsar::SharedRef<PulsarThreadContext>&& threadContext)
                : PulsarThread{ std::move(thread), std::move(threadContext) } { }
        };

        struct Channel
        {
            // FIFO List
            Pulsar::ValueList Pipe;
            bool IsClosed = false;
            std::mutex Mutex;
            std::condition_variable CV;
        };

        class ChannelType : public Pulsar::CustomDataHolder, public Channel
        {
        public:
            using Ref = Pulsar::SharedRef<ChannelType>;
            using Channel::Channel;
        };

        void BindToModule(Pulsar::Module& module);

        Pulsar::RuntimeState ThisThread_Sleep(Pulsar::ExecutionContext& eContext);

        Pulsar::RuntimeState Thread_Run(Pulsar::ExecutionContext& eContext, uint64_t threadType, uint64_t channelType);
        Pulsar::RuntimeState Thread_Join(Pulsar::ExecutionContext& eContext, uint64_t type);
        Pulsar::RuntimeState Thread_JoinAll(Pulsar::ExecutionContext& eContext, uint64_t type);
        Pulsar::RuntimeState Thread_IsAlive(Pulsar::ExecutionContext& eContext, uint64_t type);
        Pulsar::RuntimeState Thread_IsValid(Pulsar::ExecutionContext& eContext, uint64_t type);

        Pulsar::RuntimeState Channel_New(Pulsar::ExecutionContext& eContext, uint64_t type);
        Pulsar::RuntimeState Channel_Send(Pulsar::ExecutionContext& eContext, uint64_t type);
        Pulsar::RuntimeState Channel_Receive(Pulsar::ExecutionContext& eContext, uint64_t type);
        Pulsar::RuntimeState Channel_Close(Pulsar::ExecutionContext& eContext, uint64_t type);
        Pulsar::RuntimeState Channel_IsEmpty(Pulsar::ExecutionContext& eContext, uint64_t type);
        Pulsar::RuntimeState Channel_IsClosed(Pulsar::ExecutionContext& eContext, uint64_t type);
        Pulsar::RuntimeState Channel_IsValid(Pulsar::ExecutionContext& eContext, uint64_t type);

        void ThreadJoin(Pulsar::SharedRef<PulsarThread> thread, Pulsar::ValueStack& stack);
    }
}

#endif // _PULSARTOOLS_BINDINGS_THREAD_H
