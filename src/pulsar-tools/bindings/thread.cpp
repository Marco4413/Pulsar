#include "pulsar-tools/bindings/thread.h"

void PulsarTools::ThreadNativeBindings::BindToModule(Pulsar::Module& module)
{
    uint64_t threadType = module.BindCustomType("Thread", []() {
        return std::make_shared<ThreadTypeData>();
    });
    
    uint64_t channelType = module.BindCustomType("Channel", []() {
        return std::make_shared<ChannelTypeData>();
    });

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
    module.BindNativeFunction({ "channel/free!", 1, 0 },
        [channelType](auto& ctx) { return Channel_Free(ctx, channelType); });
    module.BindNativeFunction({ "channel/empty?", 1, 1 },
        [channelType](auto& ctx) { return Channel_IsEmpty(ctx, channelType); });
    module.BindNativeFunction({ "channel/closed?", 1, 1 },
        [channelType](auto& ctx) { return Channel_IsClosed(ctx, channelType); });
    module.BindNativeFunction({ "channel/valid?", 1, 1 },
        [channelType](auto& ctx) { return Channel_IsValid(ctx, channelType); });
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
    std::shared_ptr<PulsarThreadContext> threadContext = std::make_shared<PulsarThreadContext>(
        eContext.OwnerModule->CreateExecutionContext(false, true),
        Pulsar::ValueStack(),
        Pulsar::RuntimeState::OK
    );

    {
        Pulsar::ValueList& threadArgsList = threadArgs.AsList();
        Pulsar::ValueList::NodeType* threadArgsListNode = threadArgsList.Front();
        while (threadArgsListNode) {
            threadContext->Stack.PushBack(std::move(threadArgsListNode->Value()));
            threadArgsListNode = threadArgsListNode->Next();
        }
    }

    threadContext->Context.Globals = eContext.Globals;
    threadContext->IsRunning.store(true);
    std::shared_ptr<PulsarThread> thread = std::make_shared<PulsarThread>(
        std::thread([func, threadContext]() mutable {
            threadContext->State = threadContext->Context.OwnerModule->ExecuteFunction(
                func, threadContext->Stack, threadContext->Context);
            threadContext->IsRunning.store(false);
        }), threadContext
    );

    threadContext->Context.CustomTypeData.Insert(channelType, eContext.GetCustomTypeData<Pulsar::CustomTypeData>(channelType));
    
    auto threadData = eContext.GetCustomTypeData<ThreadTypeData>(threadType);
    if (!threadData)
        return Pulsar::RuntimeState::NoCustomTypeData;

    int64_t handle = threadData->NextHandle++;
    frame.Stack.EmplaceBack()
        .SetCustom({ .Type=threadType, .Handle=handle });
    threadData->Threads.Insert(handle, std::move(thread));

    return Pulsar::RuntimeState::OK;
}

Pulsar::RuntimeState PulsarTools::ThreadNativeBindings::Thread_Join(Pulsar::ExecutionContext& eContext, uint64_t type)
{
    Pulsar::Frame& frame = eContext.CallStack.CurrentFrame();
    Pulsar::Value& threadHandle = frame.Locals[0];
    if (threadHandle.Type() != Pulsar::ValueType::Custom
        || threadHandle.AsCustom().Type != type)
        return Pulsar::RuntimeState::TypeError;
    
    auto threadData = eContext.GetCustomTypeData<ThreadTypeData>(type);
    if (!threadData)
        return Pulsar::RuntimeState::NoCustomTypeData;

    auto handleThreadPair = threadData->Threads.Find(threadHandle.AsCustom().Handle);
    if (!handleThreadPair)
        return Pulsar::RuntimeState::InvalidCustomTypeHandle;
    std::shared_ptr<PulsarThread> thread = *handleThreadPair.Value;

    ThreadJoin(thread, frame.Stack);
    threadData->Threads.Remove(threadHandle.AsCustom().Handle);

    return Pulsar::RuntimeState::OK;
}

Pulsar::RuntimeState PulsarTools::ThreadNativeBindings::Thread_JoinAll(Pulsar::ExecutionContext& eContext, uint64_t type)
{
    Pulsar::Frame& frame = eContext.CallStack.CurrentFrame();
    Pulsar::Value& threadHandleList = frame.Locals[0];
    if (threadHandleList.Type() != Pulsar::ValueType::List)
        return Pulsar::RuntimeState::TypeError;
    
    auto threadData = eContext.GetCustomTypeData<ThreadTypeData>(type);
    if (!threadData)
        return Pulsar::RuntimeState::NoCustomTypeData;
    
    Pulsar::ValueList threadResults;
    Pulsar::ValueList& handleList = threadHandleList.AsList();
    Pulsar::ValueList::NodeType* handleNode = handleList.Front();
    while (handleNode) {
        Pulsar::Value& handle = handleNode->Value();
        if (handle.Type() != Pulsar::ValueType::Custom
            || handle.AsCustom().Type != type)
            return Pulsar::RuntimeState::TypeError;

        auto handleThreadPair = threadData->Threads.Find(handle.AsCustom().Handle);
        if (!handleThreadPair)
            return Pulsar::RuntimeState::InvalidCustomTypeHandle;
        std::shared_ptr<PulsarThread> thread = *handleThreadPair.Value;
        
        ThreadJoin(thread, frame.Stack);
        Pulsar::ValueList threadResult;
        threadResult.Append(std::move(frame.Stack.Back()));
        frame.Stack.PopBack();
        threadResult.Append(std::move(frame.Stack.Back()));
        frame.Stack.PopBack();

        threadResults.Append()->Value().SetList(std::move(threadResult));

        threadData->Threads.Remove(handle.AsCustom().Handle);
        handleNode = handleNode->Next();
    }

    frame.Stack.EmplaceBack()
        .SetList(std::move(threadResults));
    return Pulsar::RuntimeState::OK;
}

Pulsar::RuntimeState PulsarTools::ThreadNativeBindings::Thread_IsAlive(Pulsar::ExecutionContext& eContext, uint64_t type)
{
    Pulsar::Frame& frame = eContext.CallStack.CurrentFrame();
    Pulsar::Value& threadHandle = frame.Locals[0];
    if (threadHandle.Type() != Pulsar::ValueType::Custom
        || threadHandle.AsCustom().Type != type)
        return Pulsar::RuntimeState::TypeError;
    
    auto threadData = eContext.GetCustomTypeData<ThreadTypeData>(type);
    if (!threadData)
        return Pulsar::RuntimeState::NoCustomTypeData;

    auto handleThreadPair = threadData->Threads.Find(threadHandle.AsCustom().Handle);
    if (handleThreadPair && (*handleThreadPair.Value)->ThreadContext->IsRunning.load()) {
        frame.Stack.EmplaceBack()
            .SetInteger(1);
        return Pulsar::RuntimeState::OK;
    }
    frame.Stack.EmplaceBack()
        .SetInteger(0);
    return Pulsar::RuntimeState::OK;
}

Pulsar::RuntimeState PulsarTools::ThreadNativeBindings::Thread_IsValid(Pulsar::ExecutionContext& eContext, uint64_t type)
{
    Pulsar::Frame& frame = eContext.CallStack.CurrentFrame();
    Pulsar::Value& threadHandle = frame.Locals[0];
    if (threadHandle.Type() != Pulsar::ValueType::Custom
        || threadHandle.AsCustom().Type != type)
        return Pulsar::RuntimeState::TypeError;
    
    auto threadData = eContext.GetCustomTypeData<ThreadTypeData>(type);
    if (!threadData)
        return Pulsar::RuntimeState::NoCustomTypeData;

    auto handleThreadPair = threadData->Threads.Find(threadHandle.AsCustom().Handle);
    frame.Stack.EmplaceBack()
        .SetInteger(handleThreadPair ? 1 : 0);
    return Pulsar::RuntimeState::OK;
}

void PulsarTools::ThreadNativeBindings::ThreadJoin(std::shared_ptr<PulsarThread> thread, Pulsar::ValueStack& stack)
{
    thread->Thread.join();
    Pulsar::ValueList threadResult;
    if (thread->ThreadContext->State != Pulsar::RuntimeState::OK) {
        // An error occurred
        stack.EmplaceBack().SetList(Pulsar::ValueList());
        stack.EmplaceBack().SetInteger((int64_t)thread->ThreadContext->State);
        return;
    }

    Pulsar::ValueList returnValues;
    for (size_t i = 0; i < thread->ThreadContext->Stack.Size(); i++)
        returnValues.Append(std::move(thread->ThreadContext->Stack[i]));
    thread->ThreadContext->Stack.Clear();
    stack.EmplaceBack().SetList(std::move(returnValues));
    stack.EmplaceBack().SetInteger(0);
}

Pulsar::RuntimeState PulsarTools::ThreadNativeBindings::Channel_New(Pulsar::ExecutionContext& eContext, uint64_t type)
{
    Pulsar::Frame& frame = eContext.CallStack.CurrentFrame();
    auto channelData = eContext.GetCustomTypeData<ChannelTypeData>(type);
    if (!channelData)
        return Pulsar::RuntimeState::NoCustomTypeData;
    
    std::unique_lock channelDataLock(channelData->Mutex);
    std::shared_ptr<Channel> channel = std::make_shared<Channel>();
    int64_t handle = channelData->NextHandle++;
    channelData->Channels.Insert(handle, std::move(channel));
    channelDataLock.unlock();
    
    frame.Stack.EmplaceBack()
        .SetCustom({ .Type=type, .Handle=handle });
    return Pulsar::RuntimeState::OK;
}

Pulsar::RuntimeState PulsarTools::ThreadNativeBindings::Channel_Send(Pulsar::ExecutionContext& eContext, uint64_t type)
{
    Pulsar::Frame& frame = eContext.CallStack.CurrentFrame();
    Pulsar::Value& channelHandle = frame.Locals[1];
    if (channelHandle.Type() != Pulsar::ValueType::Custom
        || channelHandle.AsCustom().Type != type)
        return Pulsar::RuntimeState::TypeError;

    auto channelData = eContext.GetCustomTypeData<ChannelTypeData>(type);
    if (!channelData)
        return Pulsar::RuntimeState::NoCustomTypeData;
    
    std::unique_lock channelDataLock(channelData->Mutex);
    auto handleChannelPair = channelData->Channels.Find(channelHandle.AsCustom().Handle);
    if (!handleChannelPair)
        return Pulsar::RuntimeState::InvalidCustomTypeHandle;
    std::shared_ptr<Channel> channel = *handleChannelPair.Value;
    channelDataLock.unlock();

    std::unique_lock channelLock(channel->Mutex);
    if (channel->IsClosed)
        return Pulsar::RuntimeState::InvalidCustomTypeHandle;
    channel->Pipe.Append(std::move(frame.Locals[0]));
    channelLock.unlock();

    channel->CV.notify_one();
    return Pulsar::RuntimeState::OK;
}

Pulsar::RuntimeState PulsarTools::ThreadNativeBindings::Channel_Receive(Pulsar::ExecutionContext& eContext, uint64_t type)
{
    Pulsar::Frame& frame = eContext.CallStack.CurrentFrame();
    Pulsar::Value& channelHandle = frame.Locals[0];
    if (channelHandle.Type() != Pulsar::ValueType::Custom
        || channelHandle.AsCustom().Type != type)
        return Pulsar::RuntimeState::TypeError;

    auto channelData = eContext.GetCustomTypeData<ChannelTypeData>(type);
    if (!channelData)
        return Pulsar::RuntimeState::NoCustomTypeData;
    
    std::unique_lock channelDataLock(channelData->Mutex);
    auto handleChannelPair = channelData->Channels.Find(channelHandle.AsCustom().Handle);
    if (!handleChannelPair)
        return Pulsar::RuntimeState::InvalidCustomTypeHandle;
    std::shared_ptr<Channel> channel = *handleChannelPair.Value;
    channelDataLock.unlock();

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
    Pulsar::Value& channelHandle = frame.Locals[0];
    if (channelHandle.Type() != Pulsar::ValueType::Custom
        || channelHandle.AsCustom().Type != type)
        return Pulsar::RuntimeState::TypeError;

    auto channelData = eContext.GetCustomTypeData<ChannelTypeData>(type);
    if (!channelData)
        return Pulsar::RuntimeState::NoCustomTypeData;
    
    std::unique_lock channelDataLock(channelData->Mutex);
    auto handleChannelPair = channelData->Channels.Find(channelHandle.AsCustom().Handle);
    if (!handleChannelPair)
        return Pulsar::RuntimeState::InvalidCustomTypeHandle;
    std::shared_ptr<Channel> channel = *handleChannelPair.Value;
    channelDataLock.unlock();

    std::unique_lock channelLock(channel->Mutex);
    channel->IsClosed = true;
    channel->CV.notify_all();
    return Pulsar::RuntimeState::OK;
}

Pulsar::RuntimeState PulsarTools::ThreadNativeBindings::Channel_Free(Pulsar::ExecutionContext& eContext, uint64_t type)
{
    Pulsar::Frame& frame = eContext.CallStack.CurrentFrame();
    Pulsar::Value& channelHandle = frame.Locals[0];
    if (channelHandle.Type() != Pulsar::ValueType::Custom
        || channelHandle.AsCustom().Type != type)
        return Pulsar::RuntimeState::TypeError;

    auto channelData = eContext.GetCustomTypeData<ChannelTypeData>(type);
    if (!channelData)
        return Pulsar::RuntimeState::NoCustomTypeData;
    
    std::unique_lock channelDataLock(channelData->Mutex);
    auto handleChannelPair = channelData->Channels.Find(channelHandle.AsCustom().Handle);
    if (!handleChannelPair)
        return Pulsar::RuntimeState::OK;
    std::shared_ptr<Channel> channel = *handleChannelPair.Value;
    channelData->Channels.Remove(channelHandle.AsCustom().Handle);
    channelDataLock.unlock();

    std::unique_lock channelLock(channel->Mutex);
    channel->IsClosed = true;
    channel->CV.notify_all();
    return Pulsar::RuntimeState::OK;
}

Pulsar::RuntimeState PulsarTools::ThreadNativeBindings::Channel_IsEmpty(Pulsar::ExecutionContext& eContext, uint64_t type)
{
    Pulsar::Frame& frame = eContext.CallStack.CurrentFrame();
    Pulsar::Value& channelHandle = frame.Locals[0];
    if (channelHandle.Type() != Pulsar::ValueType::Custom
        || channelHandle.AsCustom().Type != type)
        return Pulsar::RuntimeState::TypeError;

    auto channelData = eContext.GetCustomTypeData<ChannelTypeData>(type);
    if (!channelData)
        return Pulsar::RuntimeState::NoCustomTypeData;
    
    std::unique_lock channelDataLock(channelData->Mutex);
    auto handleChannelPair = channelData->Channels.Find(channelHandle.AsCustom().Handle);
    if (!handleChannelPair)
        return Pulsar::RuntimeState::InvalidCustomTypeHandle;
    std::shared_ptr<Channel> channel = *handleChannelPair.Value;
    channelDataLock.unlock();

    std::unique_lock channelLock(channel->Mutex);
    frame.Stack.EmplaceBack()
        .SetInteger(channel->Pipe.Front() ? 0 : 1);
    return Pulsar::RuntimeState::OK;
}

Pulsar::RuntimeState PulsarTools::ThreadNativeBindings::Channel_IsClosed(Pulsar::ExecutionContext& eContext, uint64_t type)
{
    Pulsar::Frame& frame = eContext.CallStack.CurrentFrame();
    Pulsar::Value& channelHandle = frame.Locals[0];
    if (channelHandle.Type() != Pulsar::ValueType::Custom
        || channelHandle.AsCustom().Type != type)
        return Pulsar::RuntimeState::TypeError;

    auto channelData = eContext.GetCustomTypeData<ChannelTypeData>(type);
    if (!channelData)
        return Pulsar::RuntimeState::NoCustomTypeData;
    
    std::unique_lock channelDataLock(channelData->Mutex);
    auto handleChannelPair = channelData->Channels.Find(channelHandle.AsCustom().Handle);
    if (!handleChannelPair)
        return Pulsar::RuntimeState::InvalidCustomTypeHandle;
    std::shared_ptr<Channel> channel = *handleChannelPair.Value;
    channelDataLock.unlock();

    std::unique_lock channelLock(channel->Mutex);
    frame.Stack.EmplaceBack()
        .SetInteger(channel->IsClosed ? 1 : 0);
    return Pulsar::RuntimeState::OK;
}

Pulsar::RuntimeState PulsarTools::ThreadNativeBindings::Channel_IsValid(Pulsar::ExecutionContext& eContext, uint64_t type)
{
    Pulsar::Frame& frame = eContext.CallStack.CurrentFrame();
    Pulsar::Value& channelHandle = frame.Locals[0];
    if (channelHandle.Type() != Pulsar::ValueType::Custom
        || channelHandle.AsCustom().Type != type)
        return Pulsar::RuntimeState::TypeError;

    auto channelData = eContext.GetCustomTypeData<ChannelTypeData>(type);
    if (!channelData)
        return Pulsar::RuntimeState::NoCustomTypeData;
    
    std::unique_lock channelDataLock(channelData->Mutex);
    auto handleChannelPair = channelData->Channels.Find(channelHandle.AsCustom().Handle);
    frame.Stack.EmplaceBack()
        .SetInteger(handleChannelPair ? 1 : 0);
    return Pulsar::RuntimeState::OK;
}
