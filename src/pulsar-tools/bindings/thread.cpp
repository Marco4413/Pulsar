#include "pulsar-tools/bindings/thread.h"

#include <chrono>

void PulsarTools::ThreadNativeBindings::BindToModule(Pulsar::Module& module)
{
    uint64_t threadType = module.BindCustomType("Pulsar-Tools/Thread");
    uint64_t channelType = module.BindCustomType("Pulsar-Tools/Channel");

    module.BindNativeFunction({ "this-thread/sleep!", 1, 0 }, ThisThread_Sleep);

    module.BindNativeFunction({ "thread/run", 2, 1 },
        [threadType, channelType](auto& ctx) { return Thread_Run(ctx, threadType, channelType); });
    module.BindNativeFunction({ "thread/join", 1, 2 },
        [threadType](auto& ctx) { return Thread_Join(ctx, threadType); });
    module.BindNativeFunction({ "thread/join-all", 1, 1 },
        [threadType](auto& ctx) { return Thread_JoinAll(ctx, threadType); });
    module.BindNativeFunction({ "thread/alive?", 1, 1 },
        [threadType](auto& ctx) { return Thread_IsAlive(ctx, threadType); });
    module.BindNativeFunction({ "thread/valid?", 1, 1 },
        [threadType](auto& ctx) { return Thread_IsValid(ctx, threadType); });
    
    module.BindNativeFunction({ "channel/new", 0, 1 },
        [channelType](auto& ctx) { return Channel_New(ctx, channelType); });
    module.BindNativeFunction({ "channel/send!", 2, 0 },
        [channelType](auto& ctx) { return Channel_Send(ctx, channelType); });
    module.BindNativeFunction({ "channel/receive", 1, 1 },
        [channelType](auto& ctx) { return Channel_Receive(ctx, channelType); });
    module.BindNativeFunction({ "channel/close!", 1, 0 },
        [channelType](auto& ctx) { return Channel_Close(ctx, channelType); });
    module.BindNativeFunction({ "channel/empty?", 1, 1 },
        [channelType](auto& ctx) { return Channel_IsEmpty(ctx, channelType); });
    module.BindNativeFunction({ "channel/closed?", 1, 1 },
        [channelType](auto& ctx) { return Channel_IsClosed(ctx, channelType); });
    module.BindNativeFunction({ "channel/valid?", 1, 1 },
        [channelType](auto& ctx) { return Channel_IsValid(ctx, channelType); });
}

Pulsar::RuntimeState PulsarTools::ThreadNativeBindings::ThisThread_Sleep(Pulsar::ExecutionContext& eContext)
{
    Pulsar::Frame& frame = eContext.CallStack.CurrentFrame();
    Pulsar::Value& delay = frame.Locals[0];
    if (!Pulsar::IsNumericValueType(delay.Type()))
        return Pulsar::RuntimeState::TypeError;
    int64_t delayMs = delay.Type() == Pulsar::ValueType::Double ?
        (int64_t)delay.AsDouble() : (int64_t)delay.AsInteger();
    if (delayMs > 0)
        std::this_thread::sleep_for(std::chrono::duration<int64_t, std::milli>(delayMs));
    return Pulsar::RuntimeState::OK;
}

Pulsar::RuntimeState PulsarTools::ThreadNativeBindings::Thread_Run(Pulsar::ExecutionContext& eContext, uint64_t threadType, uint64_t channelType)
{
    Pulsar::Frame& frame = eContext.CallStack.CurrentFrame();
    Pulsar::Value& threadFn = frame.Locals[1];
    if (threadFn.Type() != Pulsar::ValueType::FunctionReference)
        return Pulsar::RuntimeState::TypeError;
    int64_t funcIdx = threadFn.AsInteger();
    if (funcIdx < 0 || (size_t)funcIdx >= eContext.OwnerModule->Functions.Size())
        return Pulsar::RuntimeState::OutOfBoundsFunctionIndex;
    Pulsar::Value& threadArgs = frame.Locals[0];
    if (threadArgs.Type() != Pulsar::ValueType::List)
        return Pulsar::RuntimeState::TypeError;
    
    const Pulsar::FunctionDefinition& func = eContext.OwnerModule->Functions[(size_t)funcIdx];
    Pulsar::SharedRef<PulsarThreadContext> threadContext = Pulsar::SharedRef<PulsarThreadContext>::New(
        eContext.OwnerModule->CreateExecutionContext(false, true),
        Pulsar::ValueStack(std::move(threadArgs.AsList())),
        Pulsar::RuntimeState::OK
    );

    eContext.CustomTypeData.ForEach([&threadContext](const Pulsar::HashMapBucket<uint64_t, Pulsar::CustomTypeData::Ref_T>& b) mutable {
        PULSAR_ASSERT(b.Value(), "Reference to CustomTypeData is nullptr.");
        uint64_t customType = b.Key();
        Pulsar::CustomTypeData::Ref_T customData = b.Value()->Copy();
        if (customData)
            threadContext->Context.CustomTypeData.Insert(customType, customData);
    });

    threadContext->Context.Globals = eContext.Globals;
    threadContext->IsRunning.store(true);
    std::thread nativeThread = std::thread([func, threadContext]() mutable {
        threadContext->State = threadContext->Context.OwnerModule->ExecuteFunction(
            func, threadContext->Stack, threadContext->Context);
        threadContext->IsRunning.store(false);
    });

    threadContext->Context.CustomTypeData.Insert(channelType, eContext.GetCustomTypeData<Pulsar::CustomTypeData>(channelType));

    frame.Stack.EmplaceBack()
        .SetCustom({ .Type=threadType, .Data=ThreadType::Ref_T::New(std::move(nativeThread), std::move(threadContext)) });
    return Pulsar::RuntimeState::OK;
}

Pulsar::RuntimeState PulsarTools::ThreadNativeBindings::Thread_Join(Pulsar::ExecutionContext& eContext, uint64_t type)
{
    Pulsar::Frame& frame = eContext.CallStack.CurrentFrame();
    Pulsar::Value& threadRef = frame.Locals[0];
    if (threadRef.Type() != Pulsar::ValueType::Custom
        || threadRef.AsCustom().Type != type)
        return Pulsar::RuntimeState::TypeError;
    
    ThreadType::Ref_T thread = threadRef.AsCustom().As<ThreadType>();
    if (!thread)
        return Pulsar::RuntimeState::InvalidCustomTypeReference;

    ThreadJoin(thread, frame.Stack);
    return Pulsar::RuntimeState::OK;
}

Pulsar::RuntimeState PulsarTools::ThreadNativeBindings::Thread_JoinAll(Pulsar::ExecutionContext& eContext, uint64_t type)
{
    Pulsar::Frame& frame = eContext.CallStack.CurrentFrame();
    Pulsar::Value& threadRefListValue = frame.Locals[0];
    if (threadRefListValue.Type() != Pulsar::ValueType::List)
        return Pulsar::RuntimeState::TypeError;
    
    Pulsar::ValueList threadResults;
    Pulsar::ValueList& threadRefList = threadRefListValue.AsList();
    Pulsar::ValueList::NodeType* threadRefNode = threadRefList.Front();
    while (threadRefNode) {
        Pulsar::Value& threadRef = threadRefNode->Value();
        if (threadRef.Type() != Pulsar::ValueType::Custom
            || threadRef.AsCustom().Type != type)
            return Pulsar::RuntimeState::TypeError;


        ThreadType::Ref_T thread = threadRef.AsCustom().As<ThreadType>();
        if (!thread)
            return Pulsar::RuntimeState::InvalidCustomTypeReference;
        
        ThreadJoin(thread, frame.Stack);
        Pulsar::ValueList threadResult;
        threadResult.Append(std::move(frame.Stack.Back()));
        frame.Stack.PopBack();
        threadResult.Append(std::move(frame.Stack.Back()));
        frame.Stack.PopBack();

        threadResults.Append()->Value().SetList(std::move(threadResult));

        threadRefNode = threadRefNode->Next();
    }

    frame.Stack.EmplaceBack()
        .SetList(std::move(threadResults));
    return Pulsar::RuntimeState::OK;
}

Pulsar::RuntimeState PulsarTools::ThreadNativeBindings::Thread_IsAlive(Pulsar::ExecutionContext& eContext, uint64_t type)
{
    Pulsar::Frame& frame = eContext.CallStack.CurrentFrame();
    Pulsar::Value& threadRef = frame.Locals[0];
    if (threadRef.Type() != Pulsar::ValueType::Custom
        || threadRef.AsCustom().Type != type)
        return Pulsar::RuntimeState::TypeError;
    
    ThreadType::Ref_T thread = threadRef.AsCustom().As<ThreadType>();
    if (!thread)
        return Pulsar::RuntimeState::InvalidCustomTypeReference;

    frame.Stack.EmplaceBack()
        .SetInteger(thread && thread->ThreadContext->IsRunning.load() ? 1 : 0);
    return Pulsar::RuntimeState::OK;
}

Pulsar::RuntimeState PulsarTools::ThreadNativeBindings::Thread_IsValid(Pulsar::ExecutionContext& eContext, uint64_t type)
{
    Pulsar::Frame& frame = eContext.CallStack.CurrentFrame();
    Pulsar::Value& threadRef = frame.Locals[0];
    if (threadRef.Type() != Pulsar::ValueType::Custom
        || threadRef.AsCustom().Type != type)
        return Pulsar::RuntimeState::TypeError;
    
    ThreadType::Ref_T thread = threadRef.AsCustom().As<ThreadType>();
    if (!thread)
        return Pulsar::RuntimeState::InvalidCustomTypeReference;

    frame.Stack.EmplaceBack()
        .SetInteger(thread ? 1 : 0);
    return Pulsar::RuntimeState::OK;
}

void PulsarTools::ThreadNativeBindings::ThreadJoin(Pulsar::SharedRef<PulsarThread> thread, Pulsar::ValueStack& stack)
{
    thread->Thread.join();
    Pulsar::ValueList threadResult;
    if (thread->ThreadContext->State != Pulsar::RuntimeState::OK) {
        // An error occurred
        stack.EmplaceBack().SetList(Pulsar::ValueList());
        stack.EmplaceBack().SetInteger((int64_t)thread->ThreadContext->State);
        return;
    }

    Pulsar::ValueList returnValues(std::move(thread->ThreadContext->Stack));
    stack.EmplaceBack().SetList(std::move(returnValues));
    stack.EmplaceBack().SetInteger(0);
}

Pulsar::RuntimeState PulsarTools::ThreadNativeBindings::Channel_New(Pulsar::ExecutionContext& eContext, uint64_t type)
{
    Pulsar::Frame& frame = eContext.CallStack.CurrentFrame();
    
    ChannelType::Ref_T channel = ChannelType::Ref_T::New();
    frame.Stack.EmplaceBack()
        .SetCustom({ .Type=type, .Data=channel });
    return Pulsar::RuntimeState::OK;
}

Pulsar::RuntimeState PulsarTools::ThreadNativeBindings::Channel_Send(Pulsar::ExecutionContext& eContext, uint64_t type)
{
    Pulsar::Frame& frame = eContext.CallStack.CurrentFrame();
    Pulsar::Value& channelRef = frame.Locals[1];
    if (channelRef.Type() != Pulsar::ValueType::Custom
        || channelRef.AsCustom().Type != type)
        return Pulsar::RuntimeState::TypeError;

    ChannelType::Ref_T channel = channelRef.AsCustom().As<ChannelType>();
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

Pulsar::RuntimeState PulsarTools::ThreadNativeBindings::Channel_Receive(Pulsar::ExecutionContext& eContext, uint64_t type)
{
    Pulsar::Frame& frame = eContext.CallStack.CurrentFrame();
    Pulsar::Value& channelRef = frame.Locals[0];
    if (channelRef.Type() != Pulsar::ValueType::Custom
        || channelRef.AsCustom().Type != type)
        return Pulsar::RuntimeState::TypeError;

    ChannelType::Ref_T channel = channelRef.AsCustom().As<ChannelType>();
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

Pulsar::RuntimeState PulsarTools::ThreadNativeBindings::Channel_Close(Pulsar::ExecutionContext& eContext, uint64_t type)
{
    Pulsar::Frame& frame = eContext.CallStack.CurrentFrame();
    Pulsar::Value& channelRef = frame.Locals[0];
    if (channelRef.Type() != Pulsar::ValueType::Custom
        || channelRef.AsCustom().Type != type)
        return Pulsar::RuntimeState::TypeError;

    ChannelType::Ref_T channel = channelRef.AsCustom().As<ChannelType>();
    if (!channel)
        return Pulsar::RuntimeState::InvalidCustomTypeReference;
    
    std::unique_lock channelLock(channel->Mutex);
    channel->IsClosed = true;
    channel->CV.notify_all();
    return Pulsar::RuntimeState::OK;
}

Pulsar::RuntimeState PulsarTools::ThreadNativeBindings::Channel_IsEmpty(Pulsar::ExecutionContext& eContext, uint64_t type)
{
    Pulsar::Frame& frame = eContext.CallStack.CurrentFrame();
    Pulsar::Value& channelRef = frame.Locals[0];
    if (channelRef.Type() != Pulsar::ValueType::Custom
        || channelRef.AsCustom().Type != type)
        return Pulsar::RuntimeState::TypeError;

    ChannelType::Ref_T channel = channelRef.AsCustom().As<ChannelType>();
    if (!channel)
        return Pulsar::RuntimeState::InvalidCustomTypeReference;
    
    std::unique_lock channelLock(channel->Mutex);
    frame.Stack.EmplaceBack()
        .SetInteger(channel->Pipe.Front() ? 0 : 1);
    return Pulsar::RuntimeState::OK;
}

Pulsar::RuntimeState PulsarTools::ThreadNativeBindings::Channel_IsClosed(Pulsar::ExecutionContext& eContext, uint64_t type)
{
    Pulsar::Frame& frame = eContext.CallStack.CurrentFrame();
    Pulsar::Value& channelRef = frame.Locals[0];
    if (channelRef.Type() != Pulsar::ValueType::Custom
        || channelRef.AsCustom().Type != type)
        return Pulsar::RuntimeState::TypeError;

    ChannelType::Ref_T channel = channelRef.AsCustom().As<ChannelType>();
    if (!channel)
        return Pulsar::RuntimeState::InvalidCustomTypeReference;
    
    std::unique_lock channelLock(channel->Mutex);
    frame.Stack.EmplaceBack()
        .SetInteger(channel->IsClosed ? 1 : 0);
    return Pulsar::RuntimeState::OK;
}

Pulsar::RuntimeState PulsarTools::ThreadNativeBindings::Channel_IsValid(Pulsar::ExecutionContext& eContext, uint64_t type)
{
    Pulsar::Frame& frame = eContext.CallStack.CurrentFrame();
    Pulsar::Value& channelRef = frame.Locals[0];
    if (channelRef.Type() != Pulsar::ValueType::Custom
        || channelRef.AsCustom().Type != type)
        return Pulsar::RuntimeState::TypeError;

    ChannelType::Ref_T channel = channelRef.AsCustom().As<ChannelType>();
    if (!channel)
        return Pulsar::RuntimeState::InvalidCustomTypeReference;
    
    frame.Stack.EmplaceBack()
        .SetInteger(channel ? 1 : 0);
    return Pulsar::RuntimeState::OK;
}
