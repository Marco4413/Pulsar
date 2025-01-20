#include "pulsar-tools/bindings/thread.h"

#include <chrono>
#include <iostream>

PulsarTools::Bindings::Thread::Thread() :
    IBinding()
{
    IBinding::BindCustomType("Pulsar-Tools/Thread");

    IBinding::BindNativeFunction({ "this-thread/sleep!", 1, 0 }, FThisSleep);

    IBinding::BindNativeFunction({ "thread/run", 2, 1 },
        CreateBiTypeBoundFactory(FRun, "Pulsar-Tools/Thread", "Pulsar-Tools/Channel"));
    IBinding::BindNativeFunction({ "thread/join", 1, 2 },
        CreateMonoTypeBoundFactory(FJoin, "Pulsar-Tools/Thread"));
    IBinding::BindNativeFunction({ "thread/join-all", 1, 1 },
        CreateMonoTypeBoundFactory(FJoinAll, "Pulsar-Tools/Thread"));
    IBinding::BindNativeFunction({ "thread/alive?", 1, 1 },
        CreateMonoTypeBoundFactory(FIsAlive, "Pulsar-Tools/Thread"));
    IBinding::BindNativeFunction({ "thread/valid?", 1, 1 },
        CreateMonoTypeBoundFactory(FIsValid, "Pulsar-Tools/Thread"));
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

Pulsar::RuntimeState PulsarTools::Bindings::Thread::FRun(Pulsar::ExecutionContext& eContext, uint64_t threadType, uint64_t channelType)
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
    Pulsar::SharedRef<ThreadContext> threadContext = Pulsar::SharedRef<ThreadContext>::New(
        Pulsar::ExecutionContext(module, false)
    );

    threadContext->Context.InitCustomTypeData();
    eContext.GetAllCustomTypeData().ForEach([&threadContext](const Pulsar::ExecutionContext::CustomTypeDataMap::Bucket& b) mutable {
        PULSAR_ASSERT(b.Value(), "Reference to CustomTypeData is nullptr.");
        uint64_t typeId = b.Key();
        Pulsar::CustomTypeData::Ref typeData = b.Value()->Copy();
        if (typeData) threadContext->Context.SetCustomTypeData(typeId, typeData);
    });

    threadContext->Context.SetCustomTypeData(channelType, eContext.GetCustomTypeData<Pulsar::CustomTypeData>(channelType));
    threadContext->Context.GetGlobals() = eContext.GetGlobals();
    threadContext->Context.GetStack() = Pulsar::ValueStack(std::move(threadFnArgs.AsList()));

    threadContext->IsRunning.store(true);
    std::thread nativeThread = std::thread([threadFn, threadContext]() {
        threadContext->Context.CallFunction(threadFn);
        threadContext->Context.Run();
        threadContext->IsRunning.store(false);
    });

    frame.Stack.EmplaceBack()
        .SetCustom({
            .Type=threadType,
            .Data=ThreadType::Ref::New(std::move(nativeThread), std::move(threadContext))
        });

    return Pulsar::RuntimeState::OK;
}

Pulsar::RuntimeState PulsarTools::Bindings::Thread::FJoin(Pulsar::ExecutionContext& eContext, uint64_t type)
{
    Pulsar::Frame& frame = eContext.CurrentFrame();
    Pulsar::Value& threadReference = frame.Locals[0];
    if (threadReference.Type() != Pulsar::ValueType::Custom
        || threadReference.AsCustom().Type != type)
        return Pulsar::RuntimeState::TypeError;
    
    ThreadType::Ref thread = threadReference.AsCustom().As<ThreadType>();
    if (!thread)
        return Pulsar::RuntimeState::InvalidCustomTypeReference;

    Join(thread, frame.Stack);
    return Pulsar::RuntimeState::OK;
}

Pulsar::RuntimeState PulsarTools::Bindings::Thread::FJoinAll(Pulsar::ExecutionContext& eContext, uint64_t type)
{
    Pulsar::Frame& frame = eContext.CurrentFrame();
    Pulsar::Value& threadReferencesList = frame.Locals[0];
    if (threadReferencesList.Type() != Pulsar::ValueType::List)
        return Pulsar::RuntimeState::TypeError;
    
    Pulsar::ValueList threadResults;
    Pulsar::ValueList& threadReferences = threadReferencesList.AsList();
    Pulsar::ValueList::Node* threadReferenceNode = threadReferences.Front();
    while (threadReferenceNode) {
        Pulsar::Value& threadReference = threadReferenceNode->Value();
        if (threadReference.Type() != Pulsar::ValueType::Custom
            || threadReference.AsCustom().Type != type)
            return Pulsar::RuntimeState::TypeError;


        ThreadType::Ref thread = threadReference.AsCustom().As<ThreadType>();
        if (!thread)
            return Pulsar::RuntimeState::InvalidCustomTypeReference;
        
        Join(thread, frame.Stack);
        Pulsar::ValueList threadResult;
        threadResult.Append(std::move(frame.Stack.Back()));
        frame.Stack.PopBack();
        threadResult.Append(std::move(frame.Stack.Back()));
        frame.Stack.PopBack();

        threadResults.Append()->Value().SetList(std::move(threadResult));

        threadReferenceNode = threadReferenceNode->Next();
    }

    frame.Stack.EmplaceBack()
        .SetList(std::move(threadResults));
    return Pulsar::RuntimeState::OK;
}

Pulsar::RuntimeState PulsarTools::Bindings::Thread::FIsAlive(Pulsar::ExecutionContext& eContext, uint64_t type)
{
    Pulsar::Frame& frame = eContext.CurrentFrame();
    Pulsar::Value& threadReference = frame.Locals[0];
    if (threadReference.Type() != Pulsar::ValueType::Custom
        || threadReference.AsCustom().Type != type)
        return Pulsar::RuntimeState::TypeError;
    
    ThreadType::Ref thread = threadReference.AsCustom().As<ThreadType>();
    if (!thread)
        return Pulsar::RuntimeState::InvalidCustomTypeReference;

    frame.Stack.EmplaceBack()
        .SetInteger(thread && thread->ThreadContext->IsRunning.load() ? 1 : 0);
    return Pulsar::RuntimeState::OK;
}

Pulsar::RuntimeState PulsarTools::Bindings::Thread::FIsValid(Pulsar::ExecutionContext& eContext, uint64_t type)
{
    Pulsar::Frame& frame = eContext.CurrentFrame();
    Pulsar::Value& threadReference = frame.Locals[0];
    if (threadReference.Type() != Pulsar::ValueType::Custom
        || threadReference.AsCustom().Type != type)
        return Pulsar::RuntimeState::TypeError;
    
    ThreadType::Ref thread = threadReference.AsCustom().As<ThreadType>();
    if (!thread)
        return Pulsar::RuntimeState::InvalidCustomTypeReference;

    frame.Stack.EmplaceBack()
        .SetInteger(thread ? 1 : 0);
    return Pulsar::RuntimeState::OK;
}

void PulsarTools::Bindings::Thread::Join(Pulsar::SharedRef<ThreadData> thread, Pulsar::ValueStack& stack)
{
    thread->Thread.join();
    Pulsar::ValueList threadResult;
    Pulsar::RuntimeState threadState = thread->ThreadContext->Context.GetState();
    if (threadState != Pulsar::RuntimeState::OK) {
        // An error occurred
        stack.EmplaceBack().SetList(Pulsar::ValueList());
        stack.EmplaceBack().SetInteger((int64_t)threadState);
        return;
    }

    Pulsar::ValueList returnValues(std::move(thread->ThreadContext->Context.GetStack()));
    stack.EmplaceBack().SetList(std::move(returnValues));
    stack.EmplaceBack().SetInteger(0);
}

PulsarTools::Bindings::Channel::Channel() :
    IBinding()
{
    BindCustomType("Pulsar-Tools/Channel");

    BindNativeFunction({ "channel/new", 0, 1 },
        CreateMonoTypeBoundFactory(FNew, "Pulsar-Tools/Channel"));
    BindNativeFunction({ "channel/send!", 2, 0 },
        CreateMonoTypeBoundFactory(FSend, "Pulsar-Tools/Channel"));
    BindNativeFunction({ "channel/receive", 1, 1 },
        CreateMonoTypeBoundFactory(FReceive, "Pulsar-Tools/Channel"));
    BindNativeFunction({ "channel/close!", 1, 0 },
        CreateMonoTypeBoundFactory(FClose, "Pulsar-Tools/Channel"));
    BindNativeFunction({ "channel/empty?", 1, 1 },
        CreateMonoTypeBoundFactory(FIsEmpty, "Pulsar-Tools/Channel"));
    BindNativeFunction({ "channel/closed?", 1, 1 },
        CreateMonoTypeBoundFactory(FIsClosed, "Pulsar-Tools/Channel"));
    BindNativeFunction({ "channel/valid?", 1, 1 },
        CreateMonoTypeBoundFactory(FIsValid, "Pulsar-Tools/Channel"));
}

Pulsar::RuntimeState PulsarTools::Bindings::Channel::FNew(Pulsar::ExecutionContext& eContext, uint64_t type)
{
    Pulsar::Frame& frame = eContext.CurrentFrame();
    
    ChannelType::Ref channel = ChannelType::Ref::New();
    frame.Stack.EmplaceBack()
        .SetCustom({ .Type=type, .Data=channel });
    return Pulsar::RuntimeState::OK;
}

Pulsar::RuntimeState PulsarTools::Bindings::Channel::FSend(Pulsar::ExecutionContext& eContext, uint64_t type)
{
    Pulsar::Frame& frame = eContext.CurrentFrame();
    Pulsar::Value& channelReference = frame.Locals[1];
    if (channelReference.Type() != Pulsar::ValueType::Custom
        || channelReference.AsCustom().Type != type)
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

Pulsar::RuntimeState PulsarTools::Bindings::Channel::FReceive(Pulsar::ExecutionContext& eContext, uint64_t type)
{
    Pulsar::Frame& frame = eContext.CurrentFrame();
    Pulsar::Value& channelReference = frame.Locals[0];
    if (channelReference.Type() != Pulsar::ValueType::Custom
        || channelReference.AsCustom().Type != type)
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
        frame.Stack.EmplaceBack(); // Push Void
        return Pulsar::RuntimeState::OK;
    }
    
    frame.Stack.EmplaceBack(std::move(channel->Pipe.Front()->Value()));
    channel->Pipe.RemoveFront(1);
    return Pulsar::RuntimeState::OK;
}

Pulsar::RuntimeState PulsarTools::Bindings::Channel::FClose(Pulsar::ExecutionContext& eContext, uint64_t type)
{
    Pulsar::Frame& frame = eContext.CurrentFrame();
    Pulsar::Value& channelReference = frame.Locals[0];
    if (channelReference.Type() != Pulsar::ValueType::Custom
        || channelReference.AsCustom().Type != type)
        return Pulsar::RuntimeState::TypeError;

    ChannelType::Ref channel = channelReference.AsCustom().As<ChannelType>();
    if (!channel)
        return Pulsar::RuntimeState::InvalidCustomTypeReference;
    
    std::unique_lock channelLock(channel->Mutex);
    channel->IsClosed = true;
    channel->CV.notify_all();
    return Pulsar::RuntimeState::OK;
}

Pulsar::RuntimeState PulsarTools::Bindings::Channel::FIsEmpty(Pulsar::ExecutionContext& eContext, uint64_t type)
{
    Pulsar::Frame& frame = eContext.CurrentFrame();
    Pulsar::Value& channelReference = frame.Locals[0];
    if (channelReference.Type() != Pulsar::ValueType::Custom
        || channelReference.AsCustom().Type != type)
        return Pulsar::RuntimeState::TypeError;

    ChannelType::Ref channel = channelReference.AsCustom().As<ChannelType>();
    if (!channel)
        return Pulsar::RuntimeState::InvalidCustomTypeReference;
    
    std::unique_lock channelLock(channel->Mutex);
    frame.Stack.EmplaceBack()
        .SetInteger(channel->Pipe.Front() ? 0 : 1);
    return Pulsar::RuntimeState::OK;
}

Pulsar::RuntimeState PulsarTools::Bindings::Channel::FIsClosed(Pulsar::ExecutionContext& eContext, uint64_t type)
{
    Pulsar::Frame& frame = eContext.CurrentFrame();
    Pulsar::Value& channelReference = frame.Locals[0];
    if (channelReference.Type() != Pulsar::ValueType::Custom
        || channelReference.AsCustom().Type != type)
        return Pulsar::RuntimeState::TypeError;

    ChannelType::Ref channel = channelReference.AsCustom().As<ChannelType>();
    if (!channel)
        return Pulsar::RuntimeState::InvalidCustomTypeReference;
    
    std::unique_lock channelLock(channel->Mutex);
    frame.Stack.EmplaceBack()
        .SetInteger(channel->IsClosed ? 1 : 0);
    return Pulsar::RuntimeState::OK;
}

Pulsar::RuntimeState PulsarTools::Bindings::Channel::FIsValid(Pulsar::ExecutionContext& eContext, uint64_t type)
{
    Pulsar::Frame& frame = eContext.CurrentFrame();
    Pulsar::Value& channelReference = frame.Locals[0];
    if (channelReference.Type() != Pulsar::ValueType::Custom
        || channelReference.AsCustom().Type != type)
        return Pulsar::RuntimeState::TypeError;

    ChannelType::Ref channel = channelReference.AsCustom().As<ChannelType>();
    if (!channel)
        return Pulsar::RuntimeState::InvalidCustomTypeReference;
    
    frame.Stack.EmplaceBack()
        .SetInteger(channel ? 1 : 0);
    return Pulsar::RuntimeState::OK;
}
