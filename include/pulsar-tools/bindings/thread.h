#ifndef _PULSARTOOLS_BINDINGS_THREAD_H
#define _PULSARTOOLS_BINDINGS_THREAD_H

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <thread>

#include <iostream>

#include "pulsar-tools/binding.h"

namespace PulsarTools::Bindings
{
    class Channel :
        public IBinding
    {
    public:
        struct ChannelData
        {
            // FIFO List
            Pulsar::ValueList Pipe;
            bool IsClosed = false;
            std::mutex Mutex;
            std::condition_variable CV;
        };

        class ChannelType :
            public Pulsar::CustomDataHolder,
            public ChannelData
        {
        public:
            using Ref = Pulsar::SharedRef<ChannelType>;
            using ChannelData::ChannelData;
        };

    public:
        Channel();

    public:
        static Pulsar::RuntimeState FNew(Pulsar::ExecutionContext& eContext, uint64_t type);
        static Pulsar::RuntimeState FSend(Pulsar::ExecutionContext& eContext, uint64_t type);
        static Pulsar::RuntimeState FReceive(Pulsar::ExecutionContext& eContext, uint64_t type);
        static Pulsar::RuntimeState FClose(Pulsar::ExecutionContext& eContext, uint64_t type);
        static Pulsar::RuntimeState FIsEmpty(Pulsar::ExecutionContext& eContext, uint64_t type);
        static Pulsar::RuntimeState FIsClosed(Pulsar::ExecutionContext& eContext, uint64_t type);
        static Pulsar::RuntimeState FIsValid(Pulsar::ExecutionContext& eContext, uint64_t type);
    };

    class Thread :
        public IBinding
    {
    public:
        struct ThreadContext
        {
            Pulsar::ExecutionContext Context;
            std::atomic_bool IsRunning = false;
        };

        struct ThreadData
        {
            std::thread Thread;
            Pulsar::SharedRef<Thread::ThreadContext> ThreadContext;
        };

        class ThreadType :
            public Pulsar::CustomDataHolder,
            public ThreadData
        {
        public:
            using Ref = Pulsar::SharedRef<ThreadType>;
            ThreadType(std::thread&& thread, Pulsar::SharedRef<Thread::ThreadContext>&& threadContext)
                : ThreadData{ std::move(thread), std::move(threadContext) } { }
        };

    public:
        Thread();

    public:
        static Pulsar::RuntimeState FThisSleep(Pulsar::ExecutionContext& eContext);
        static Pulsar::RuntimeState FRun(Pulsar::ExecutionContext& eContext, uint64_t threadType, uint64_t channelType);
        static Pulsar::RuntimeState FJoin(Pulsar::ExecutionContext& eContext, uint64_t type);
        static Pulsar::RuntimeState FJoinAll(Pulsar::ExecutionContext& eContext, uint64_t type);
        static Pulsar::RuntimeState FIsAlive(Pulsar::ExecutionContext& eContext, uint64_t type);
        static Pulsar::RuntimeState FIsValid(Pulsar::ExecutionContext& eContext, uint64_t type);

        static void Join(Pulsar::SharedRef<ThreadData> thread, Pulsar::ValueStack& stack);
    };
}

#endif // _PULSARTOOLS_BINDINGS_THREAD_H
