#include "pulsar-tools/bindings/thread.h"

void PulsarTools::ThreadNativeBindings::BindToModule(Pulsar::Module& module)
{
    uint64_t type = module.BindCustomType("Thread");
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
    
    m_Mutex.lock();
    int64_t handle = m_NextHandle++;
    frame.Stack.EmplaceBack()
        .SetCustom({ .Type=type, .Handle=handle });
    m_Threads.Insert(handle, std::move(thread));
    m_Mutex.unlock();

    return Pulsar::RuntimeState::OK;
}

Pulsar::RuntimeState PulsarTools::ThreadNativeBindings::Thread_Join(Pulsar::ExecutionContext& eContext, uint64_t type)
{
    Pulsar::Frame& frame = eContext.CallStack.CurrentFrame();
    Pulsar::Value& threadHandle = frame.Locals[0];
    if (threadHandle.Type() != Pulsar::ValueType::Custom
        || threadHandle.AsCustom().Type != type)
        return Pulsar::RuntimeState::TypeError;

    m_Mutex.lock();
    auto handleThreadPair = m_Threads.Find(threadHandle.AsCustom().Handle);
    if (!handleThreadPair) {
        m_Mutex.unlock();
        return Pulsar::RuntimeState::Error;
    }
    std::shared_ptr<PulsarThread> thread = *handleThreadPair.Value;
    m_Mutex.unlock();

    frame.Stack.PushBack(Pulsar::Value().SetList(ThreadJoin(thread)));

    m_Mutex.lock();
    m_Threads.Remove(threadHandle.AsCustom().Handle);
    m_Mutex.unlock();
    return Pulsar::RuntimeState::OK;
}

Pulsar::RuntimeState PulsarTools::ThreadNativeBindings::Thread_JoinAll(Pulsar::ExecutionContext& eContext, uint64_t type)
{
    Pulsar::Frame& frame = eContext.CallStack.CurrentFrame();
    Pulsar::Value& threadHandleList = frame.Locals[0];
    if (threadHandleList.Type() != Pulsar::ValueType::List)
        return Pulsar::RuntimeState::TypeError;
    
    Pulsar::ValueList threadResults;
    Pulsar::ValueList& handleList = threadHandleList.AsList();
    Pulsar::ValueList::NodeType* handleNode = handleList.Front();
    while (handleNode) {
        Pulsar::Value& handle = handleNode->Value();
        if (handle.Type() != Pulsar::ValueType::Custom
            || handle.AsCustom().Type != type)
            return Pulsar::RuntimeState::TypeError;

        m_Mutex.lock();
        auto handleThreadPair = m_Threads.Find(handle.AsCustom().Handle);
        if (!handleThreadPair) {
            m_Mutex.unlock();
            return Pulsar::RuntimeState::Error;
        }
        std::shared_ptr<PulsarThread> thread = *handleThreadPair.Value;
        m_Mutex.unlock();
        
        threadResults.Append()->Value().SetList(ThreadJoin(thread));

        m_Mutex.lock();
        m_Threads.Remove(handle.AsCustom().Handle);
        m_Mutex.unlock();
        handleNode = handleNode->Next();
    }

    frame.Stack.PushBack(Pulsar::Value().SetList(std::move(threadResults)));
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

    m_Mutex.lock();
    auto handleThreadPair = m_Threads.Find(threadHandle.AsCustom().Handle);
    if (handleThreadPair && (*handleThreadPair.Value)->ThreadContext->IsRunning.load()) {
        m_Mutex.unlock();
        frame.Stack.PushBack(Pulsar::Value().SetInteger(1));
        return Pulsar::RuntimeState::OK;
    }
    m_Mutex.unlock();
    frame.Stack.PushBack(Pulsar::Value().SetInteger(0));
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

    m_Mutex.lock();
    auto handleThreadPair = m_Threads.Find(threadHandle.AsCustom().Handle);
    frame.Stack.PushBack(Pulsar::Value().SetInteger(handleThreadPair ? 1 : 0));
    m_Mutex.unlock();
    return Pulsar::RuntimeState::OK;
}

Pulsar::ValueList PulsarTools::ThreadNativeBindings::ThreadJoin(std::shared_ptr<PulsarThread> thread) const
{
    thread->Thread.join();
    Pulsar::ValueList threadResult;
    if (thread->ThreadContext->State != Pulsar::RuntimeState::OK) {
        // An error occurred
        threadResult.Append()->Value().SetInteger((int64_t)thread->ThreadContext->State);
        threadResult.Append()->Value().SetList(Pulsar::ValueList());
    } else {
        threadResult.Append()->Value().SetInteger(0);
        Pulsar::ValueList returnValues;
        for (size_t i = 0; i < thread->ThreadContext->Stack.Size(); i++)
            returnValues.Append()->Value() = std::move(thread->ThreadContext->Stack[i]);
        thread->ThreadContext->Stack.Clear();
        threadResult.Append()->Value().SetList(std::move(returnValues));
    }
    return threadResult;
}
