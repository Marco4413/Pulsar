#ifndef _PULSARBINDINGS_STD_THREAD_H
#define _PULSARBINDINGS_STD_THREAD_H

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <thread>

#include <iostream>

#include "pulsar-bindings/binding.h"

namespace PulsarBindings::Std
{
    class Channel : public Binding
    {
    public:
        struct ChannelData
        {
            // FIFO List
            Pulsar::Value::List Pipe;
            bool IsClosed = false;
            std::mutex Mutex;
            std::condition_variable CV;
        };

        // TODO: Rework to not require ChannelData, like Thread::IThreadType.
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
        static Pulsar::RuntimeState FNew(Pulsar::ExecutionContext& eContext, uint64_t channelTypeId);
        static Pulsar::RuntimeState FSend(Pulsar::ExecutionContext& eContext, uint64_t channelTypeId);
        static Pulsar::RuntimeState FReceive(Pulsar::ExecutionContext& eContext, uint64_t channelTypeId);
        static Pulsar::RuntimeState FClose(Pulsar::ExecutionContext& eContext, uint64_t channelTypeId);
        static Pulsar::RuntimeState FIsEmpty(Pulsar::ExecutionContext& eContext, uint64_t channelTypeId);
        static Pulsar::RuntimeState FIsClosed(Pulsar::ExecutionContext& eContext, uint64_t channelTypeId);
        static Pulsar::RuntimeState FIsValid(Pulsar::ExecutionContext& eContext, uint64_t channelTypeId);
    };

    class Thread : public Binding
    {
    public:
        class IThreadType :
            public Pulsar::CustomDataHolder
        {
        public:
            using Ref = Pulsar::SharedRef<IThreadType>;
            virtual ~IThreadType() = default;

            // If false after Join() is called, the thread is not and will no longer be running.
            virtual bool IsRunning() const = 0;
            virtual void Join() = 0;

            // Safely accessible only if IsRunning() returns false
            // virtual Pulsar::ExecutionContext& GetContext() = 0;

            // These methods may only be called after Join().
            virtual Pulsar::RuntimeState GetState() const = 0;
            // Gets the stack from the context and clears it.
            virtual void PullStack(Pulsar::Stack& out) = 0;
        };

    public:
        Thread();
        virtual ~Thread() = default;

    public:
        Pulsar::RuntimeState FRun(Pulsar::ExecutionContext& eContext, uint64_t threadTypeId);

        static Pulsar::RuntimeState FThisSleep(Pulsar::ExecutionContext& eContext);
        static Pulsar::RuntimeState FJoin(Pulsar::ExecutionContext& eContext, uint64_t threadTypeId);
        static Pulsar::RuntimeState FJoinAll(Pulsar::ExecutionContext& eContext, uint64_t threadTypeId);
        static Pulsar::RuntimeState FIsAlive(Pulsar::ExecutionContext& eContext, uint64_t threadTypeId);
        static Pulsar::RuntimeState FIsValid(Pulsar::ExecutionContext& eContext, uint64_t threadTypeId);

        static void Join(IThreadType::Ref thread, Pulsar::Stack& stack);

        virtual IThreadType::Ref CreateThread(const Pulsar::ExecutionContext& parentContext, const Pulsar::FunctionDefinition& function, Pulsar::Stack&& initStack) const;
    };
}

#endif // _PULSARBINDINGS_STD_THREAD_H
