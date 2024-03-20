#include "pulsar-tools/bindings/thread.h"

void PulsarTools::ThreadNativeBindings::BindToModule(Pulsar::Module& module)
{
    uint64_t type = module.BindCustomType("Thread", []() {
        return std::make_shared<ThreadTypeData>();
    });
    module.BindNativeFunction({ "thread/run", 2, 1 },
        [&, type](auto& ctx) { return Thread_Run(ctx, type); });
    module.BindNativeFunction({ "thread/join", 1, 1 },
        [&, type](auto& ctx) { return Thread_Join(ctx, type); });
    module.BindNativeFunction({ "thread/join-all", 1, 1 },
        [&, type](auto& ctx) { return Thread_JoinAll(ctx, type); });
    module.BindNativeFunction({ "thread/alive?", 1, 2 },
        [&, type](auto& ctx) { return Thread_IsAlive(ctx, type); });
    module.BindNativeFunction({ "thread/valid?", 1, 2 },
        [&, type](auto& ctx) { return Thread_IsValid(ctx, type); });
}

Pulsar::RuntimeState PulsarTools::ThreadNativeBindings::Thread_Run(Pulsar::ExecutionContext& eContext, uint64_t type)
{
    Pulsar::Frame& frame = eContext.CallStack.CurrentFrame();
    Pulsar::Value& threadFn = frame.Locals[0];
    if (threadFn.Type() != Pulsar::ValueType::FunctionReference)
        return Pulsar::RuntimeState::TypeError;
    int64_t funcIdx = threadFn.AsInteger();
    if (funcIdx < 0 || (size_t)funcIdx >= eContext.OwnerModule->Functions.Size())
        return Pulsar::RuntimeState::OutOfBoundsFunctionIndex;
    Pulsar::Value& threadArgs = frame.Locals[1];
    if (threadArgs.Type() != Pulsar::ValueType::List)
        return Pulsar::RuntimeState::TypeError;
    
    const Pulsar::FunctionDefinition& func = eContext.OwnerModule->Functions[(size_t)funcIdx];
    std::shared_ptr<PulsarThreadContext> threadContext = std::make_shared<PulsarThreadContext>(
        eContext.OwnerModule->CreateExecutionContext(),
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
    std::shared_ptr<PulsarThread> thread = std::make_shared<PulsarThread>(
        std::thread([func, threadContext]() mutable {
            threadContext->IsRunning.store(true);
            threadContext->State = threadContext->Context.OwnerModule->ExecuteFunction(
                func, threadContext->Stack, threadContext->Context);
            threadContext->IsRunning.store(false);
        }), threadContext
    );
    
    auto threadData = eContext.GetCustomTypeData<ThreadTypeData>(type);
    if (!threadData)
        return Pulsar::RuntimeState::NoCustomTypeData;

    int64_t handle = threadData->NextHandle++;
    frame.Stack.EmplaceBack()
        .SetCustom({ .Type=type, .Handle=handle });
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
        threadResult.Append()->Value() = std::move(frame.Stack.Back());
        frame.Stack.PopBack();
        threadResult.Append()->Value() = std::move(frame.Stack.Back());
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
    frame.Stack.PushBack(threadHandle);
    
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
    frame.Stack.PushBack(threadHandle);
    
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
        returnValues.Append()->Value() = std::move(thread->ThreadContext->Stack[i]);
    thread->ThreadContext->Stack.Clear();
    stack.EmplaceBack().SetList(std::move(returnValues));
    stack.EmplaceBack().SetInteger(0);
}
