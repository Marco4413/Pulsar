#include "pulsar-tools/bindings/thread.h"

#include <chrono>
#include <iostream>

PulsarTools::Bindings::Thread::Thread() :
    IBinding()
{
    AddDependency(Channel());

    BindCustomType("Pulsar-Tools/Thread");

    BindNativeFunction({ "this-thread/sleep!", 1, 0 }, FThisSleep);

    BindNativeFunction({ "thread/run",      2, 1 }, CreateTypeBoundFactory(FRun,     "Pulsar-Tools/Thread"));
    BindNativeFunction({ "thread/join",     1, 2 }, CreateTypeBoundFactory(FJoin,    "Pulsar-Tools/Thread"));
    BindNativeFunction({ "thread/join-all", 1, 1 }, CreateTypeBoundFactory(FJoinAll, "Pulsar-Tools/Thread"));
    BindNativeFunction({ "thread/alive?",   1, 1 }, CreateTypeBoundFactory(FIsAlive, "Pulsar-Tools/Thread"));
    BindNativeFunction({ "thread/valid?",   1, 1 }, CreateTypeBoundFactory(FIsValid, "Pulsar-Tools/Thread"));
}

Pulsar::RuntimeState PulsarTools::Bindings::Thread::FThisSleep(Pulsar::ExecutionContext& eContext)
{
    Pulsar::Frame& frame = eContext.CurrentFrame();
    Pulsar::Value& delay = frame.Locals[0];
    if (!Pulsar::IsNumericValueType(delay.Type()))
        return Pulsar::RuntimeState::TypeError;
    int64_t delayMs = delay.Type() == Pulsar::ValueType::Double ?
        (int64_t)delay.AsDouble() : (int64_t)delay.AsInteger();
    if (delayMs > 0)
        std::this_thread::sleep_for(std::chrono::duration<int64_t, std::milli>(delayMs));
    return Pulsar::RuntimeState::OK;
}

Pulsar::RuntimeState PulsarTools::Bindings::Thread::FRun(Pulsar::ExecutionContext& eContext, uint64_t threadTypeId)
{
    const Pulsar::Module& module = eContext.GetModule();
    Pulsar::Frame& frame = eContext.CurrentFrame();

    Pulsar::Value& threadFnReference = frame.Locals[1];
    if (threadFnReference.Type() != Pulsar::ValueType::FunctionReference)
        return Pulsar::RuntimeState::TypeError;
    int64_t threadFnIdx = threadFnReference.AsInteger();
    if (threadFnIdx < 0 || (size_t)threadFnIdx >= module.Functions.Size())
        return Pulsar::RuntimeState::OutOfBoundsFunctionIndex;
    Pulsar::Value& threadFnArgs = frame.Locals[0];
    if (threadFnArgs.Type() != Pulsar::ValueType::List)
        return Pulsar::RuntimeState::TypeError;
    
    const Pulsar::FunctionDefinition& threadFn = module.Functions[(size_t)threadFnIdx];
    Pulsar::SharedRef<ThreadContext> threadContext = Pulsar::SharedRef<ThreadContext>::New(eContext.Fork());

    threadContext->Context.GetStack() = Pulsar::Stack(std::move(threadFnArgs.AsList()));

    threadContext->IsRunning.store(true);
    std::thread nativeThread = std::thread([threadFn, threadContext]() {
        threadContext->Context.CallFunction(threadFn);
        threadContext->Context.Run();
        threadContext->IsRunning.store(false);
    });

    frame.Stack.EmplaceCustom({
            .Type=threadTypeId,
            .Data=ThreadType::Ref::New(std::move(nativeThread), std::move(threadContext))
        });

    return Pulsar::RuntimeState::OK;
}

Pulsar::RuntimeState PulsarTools::Bindings::Thread::FJoin(Pulsar::ExecutionContext& eContext, uint64_t threadTypeId)
{
    Pulsar::Frame& frame = eContext.CurrentFrame();
    Pulsar::Value& threadReference = frame.Locals[0];
    if (threadReference.Type() != Pulsar::ValueType::Custom
        || threadReference.AsCustom().Type != threadTypeId)
        return Pulsar::RuntimeState::TypeError;
    
    ThreadType::Ref thread = threadReference.AsCustom().As<ThreadType>();
    if (!thread)
        return Pulsar::RuntimeState::InvalidCustomTypeReference;

    Join(thread, frame.Stack);
    return Pulsar::RuntimeState::OK;
}

Pulsar::RuntimeState PulsarTools::Bindings::Thread::FJoinAll(Pulsar::ExecutionContext& eContext, uint64_t threadTypeId)
{
    Pulsar::Frame& frame = eContext.CurrentFrame();
    Pulsar::Value& threadReferencesList = frame.Locals[0];
    if (threadReferencesList.Type() != Pulsar::ValueType::List)
        return Pulsar::RuntimeState::TypeError;

    Pulsar::Value::List threadResults;
    for (Pulsar::Value& threadReference : threadReferencesList.AsList()) {
        if (threadReference.Type() != Pulsar::ValueType::Custom
            || threadReference.AsCustom().Type != threadTypeId)
            return Pulsar::RuntimeState::TypeError;

        ThreadType::Ref thread = threadReference.AsCustom().As<ThreadType>();
        if (!thread)
            return Pulsar::RuntimeState::InvalidCustomTypeReference;
        
        Join(thread, frame.Stack);
        Pulsar::Value::List threadResult;
        threadResult.Append(frame.Stack.Pop());
        threadResult.Append(frame.Stack.Pop());

        threadResults.Append()->Value().SetList(std::move(threadResult));
    }

    frame.Stack.EmplaceList(std::move(threadResults));
    return Pulsar::RuntimeState::OK;
}

Pulsar::RuntimeState PulsarTools::Bindings::Thread::FIsAlive(Pulsar::ExecutionContext& eContext, uint64_t threadTypeId)
{
    Pulsar::Frame& frame = eContext.CurrentFrame();
    Pulsar::Value& threadReference = frame.Locals[0];
    if (threadReference.Type() != Pulsar::ValueType::Custom
        || threadReference.AsCustom().Type != threadTypeId)
        return Pulsar::RuntimeState::TypeError;
    
    ThreadType::Ref thread = threadReference.AsCustom().As<ThreadType>();
    if (!thread)
        return Pulsar::RuntimeState::InvalidCustomTypeReference;

    frame.Stack.EmplaceInteger(thread && thread->ThreadContext->IsRunning.load() ? 1 : 0);
    return Pulsar::RuntimeState::OK;
}

Pulsar::RuntimeState PulsarTools::Bindings::Thread::FIsValid(Pulsar::ExecutionContext& eContext, uint64_t threadTypeId)
{
    Pulsar::Frame& frame = eContext.CurrentFrame();
    Pulsar::Value& threadReference = frame.Locals[0];
    if (threadReference.Type() != Pulsar::ValueType::Custom
        || threadReference.AsCustom().Type != threadTypeId)
        return Pulsar::RuntimeState::TypeError;
    
    ThreadType::Ref thread = threadReference.AsCustom().As<ThreadType>();
    if (!thread)
        return Pulsar::RuntimeState::InvalidCustomTypeReference;

    frame.Stack.EmplaceInteger(thread ? 1 : 0);
    return Pulsar::RuntimeState::OK;
}

void PulsarTools::Bindings::Thread::Join(Pulsar::SharedRef<ThreadData> thread, Pulsar::Stack& stack)
{
    thread->Thread.join();
    Pulsar::Value::List threadResult;
    Pulsar::RuntimeState threadState = thread->ThreadContext->Context.GetState();
    if (threadState != Pulsar::RuntimeState::OK) {
        // An error occurred
        stack.EmplaceList();
        stack.EmplaceInteger((int64_t)threadState);
        return;
    }

    Pulsar::Value::List returnValues;
    Pulsar::Stack& threadStack = thread->ThreadContext->Context.GetStack();
    for (Pulsar::Value& value : threadStack) returnValues.Append(std::move(value));

    stack.EmplaceList(std::move(returnValues));
    stack.EmplaceInteger(0);
}

PulsarTools::Bindings::Channel::Channel() :
    IBinding()
{
    BindCustomType("Pulsar-Tools/Channel");

    BindNativeFunction({ "channel/new",     0, 1 }, CreateTypeBoundFactory(FNew,      "Pulsar-Tools/Channel"));
    BindNativeFunction({ "channel/send!",   2, 0 }, CreateTypeBoundFactory(FSend,     "Pulsar-Tools/Channel"));
    BindNativeFunction({ "channel/receive", 1, 1 }, CreateTypeBoundFactory(FReceive,  "Pulsar-Tools/Channel"));
    BindNativeFunction({ "channel/close!",  1, 0 }, CreateTypeBoundFactory(FClose,    "Pulsar-Tools/Channel"));
    BindNativeFunction({ "channel/empty?",  1, 1 }, CreateTypeBoundFactory(FIsEmpty,  "Pulsar-Tools/Channel"));
    BindNativeFunction({ "channel/closed?", 1, 1 }, CreateTypeBoundFactory(FIsClosed, "Pulsar-Tools/Channel"));
    BindNativeFunction({ "channel/valid?",  1, 1 }, CreateTypeBoundFactory(FIsValid,  "Pulsar-Tools/Channel"));
}

Pulsar::RuntimeState PulsarTools::Bindings::Channel::FNew(Pulsar::ExecutionContext& eContext, uint64_t channelTypeId)
{
    Pulsar::Frame& frame = eContext.CurrentFrame();
    
    ChannelType::Ref channel = ChannelType::Ref::New();
    frame.Stack.EmplaceCustom({ channelTypeId, channel });
    return Pulsar::RuntimeState::OK;
}

Pulsar::RuntimeState PulsarTools::Bindings::Channel::FSend(Pulsar::ExecutionContext& eContext, uint64_t channelTypeId)
{
    Pulsar::Frame& frame = eContext.CurrentFrame();
    Pulsar::Value& channelReference = frame.Locals[1];
    if (channelReference.Type() != Pulsar::ValueType::Custom
        || channelReference.AsCustom().Type != channelTypeId)
        return Pulsar::RuntimeState::TypeError;

    ChannelType::Ref channel = channelReference.AsCustom().As<ChannelType>();
    if (!channel)
        return Pulsar::RuntimeState::InvalidCustomTypeReference;
    
    std::unique_lock channelLock(channel->Mutex);
    if (channel->IsClosed)
        return Pulsar::RuntimeState::OK;
    channel->Pipe.Append(std::move(frame.Locals[0]));
    channelLock.unlock();

    channel->CV.notify_one();
    return Pulsar::RuntimeState::OK;
}

Pulsar::RuntimeState PulsarTools::Bindings::Channel::FReceive(Pulsar::ExecutionContext& eContext, uint64_t channelTypeId)
{
    Pulsar::Frame& frame = eContext.CurrentFrame();
    Pulsar::Value& channelReference = frame.Locals[0];
    if (channelReference.Type() != Pulsar::ValueType::Custom
        || channelReference.AsCustom().Type != channelTypeId)
        return Pulsar::RuntimeState::TypeError;

    ChannelType::Ref channel = channelReference.AsCustom().As<ChannelType>();
    if (!channel)
        return Pulsar::RuntimeState::InvalidCustomTypeReference;
    
    std::unique_lock channelLock(channel->Mutex);
    channel->CV.wait(channelLock, [&channel]() {
        // Wait for Channel to have items or to be closed
        return channel->Pipe.Front() || channel->IsClosed;
    });

    if (channel->IsClosed && !channel->Pipe.Front()) {
        frame.Stack.Emplace(); // Push Void
        return Pulsar::RuntimeState::OK;
    }
    
    frame.Stack.Push(std::move(channel->Pipe.Front()->Value()));
    channel->Pipe.RemoveFront(1);
    return Pulsar::RuntimeState::OK;
}

Pulsar::RuntimeState PulsarTools::Bindings::Channel::FClose(Pulsar::ExecutionContext& eContext, uint64_t channelTypeId)
{
    Pulsar::Frame& frame = eContext.CurrentFrame();
    Pulsar::Value& channelReference = frame.Locals[0];
    if (channelReference.Type() != Pulsar::ValueType::Custom
        || channelReference.AsCustom().Type != channelTypeId)
        return Pulsar::RuntimeState::TypeError;

    ChannelType::Ref channel = channelReference.AsCustom().As<ChannelType>();
    if (!channel)
        return Pulsar::RuntimeState::InvalidCustomTypeReference;
    
    std::unique_lock channelLock(channel->Mutex);
    channel->IsClosed = true;
    channel->CV.notify_all();
    return Pulsar::RuntimeState::OK;
}

Pulsar::RuntimeState PulsarTools::Bindings::Channel::FIsEmpty(Pulsar::ExecutionContext& eContext, uint64_t channelTypeId)
{
    Pulsar::Frame& frame = eContext.CurrentFrame();
    Pulsar::Value& channelReference = frame.Locals[0];
    if (channelReference.Type() != Pulsar::ValueType::Custom
        || channelReference.AsCustom().Type != channelTypeId)
        return Pulsar::RuntimeState::TypeError;

    ChannelType::Ref channel = channelReference.AsCustom().As<ChannelType>();
    if (!channel)
        return Pulsar::RuntimeState::InvalidCustomTypeReference;
    
    std::unique_lock channelLock(channel->Mutex);
    frame.Stack.EmplaceInteger(channel->Pipe.Front() ? 0 : 1);
    return Pulsar::RuntimeState::OK;
}

Pulsar::RuntimeState PulsarTools::Bindings::Channel::FIsClosed(Pulsar::ExecutionContext& eContext, uint64_t channelTypeId)
{
    Pulsar::Frame& frame = eContext.CurrentFrame();
    Pulsar::Value& channelReference = frame.Locals[0];
    if (channelReference.Type() != Pulsar::ValueType::Custom
        || channelReference.AsCustom().Type != channelTypeId)
        return Pulsar::RuntimeState::TypeError;

    ChannelType::Ref channel = channelReference.AsCustom().As<ChannelType>();
    if (!channel)
        return Pulsar::RuntimeState::InvalidCustomTypeReference;
    
    std::unique_lock channelLock(channel->Mutex);
    frame.Stack.EmplaceInteger(channel->IsClosed ? 1 : 0);
    return Pulsar::RuntimeState::OK;
}

Pulsar::RuntimeState PulsarTools::Bindings::Channel::FIsValid(Pulsar::ExecutionContext& eContext, uint64_t channelTypeId)
{
    Pulsar::Frame& frame = eContext.CurrentFrame();
    Pulsar::Value& channelReference = frame.Locals[0];
    if (channelReference.Type() != Pulsar::ValueType::Custom
        || channelReference.AsCustom().Type != channelTypeId)
        return Pulsar::RuntimeState::TypeError;

    ChannelType::Ref channel = channelReference.AsCustom().As<ChannelType>();
    if (!channel)
        return Pulsar::RuntimeState::InvalidCustomTypeReference;
    
    frame.Stack.EmplaceInteger(channel ? 1 : 0);
    return Pulsar::RuntimeState::OK;
}
