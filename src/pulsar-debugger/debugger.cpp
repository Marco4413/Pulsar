#include "pulsar-debugger/debugger.h"

#include <filesystem>

#include <pulsar/parser.h>
#include <pulsar-bindings/std.h>

namespace PulsarDebugger::Bindings
{
    template<typename T>
    concept ConstructibleWithDebugger = requires(Debugger& debugger) { T(debugger); };

namespace Std
{
    using Debug      = PulsarBindings::Std::Debug;
    using Error      = PulsarBindings::Std::Error;
    using FileSystem = PulsarBindings::Std::FileSystem;
    using Lexer      = PulsarBindings::Std::Lexer;
    using Module     = PulsarBindings::Std::Module;
    using Print      = PulsarBindings::Std::Print;
    using Stdio      = PulsarBindings::Std::Stdio;
    using Thread     = class ThreadImpl;
    using Time       = PulsarBindings::Std::Time;

    class ThreadTypeImpl final : public PulsarBindings::Std::Thread::IThreadType
    {
    public:
        ThreadTypeImpl(PulsarDebugger::Ref<PulsarDebugger::Thread> thread)
            : m_Thread(thread)
        {
        }

        virtual ~ThreadTypeImpl() = default;

        bool IsRunning() const override
        {
            return m_Thread->IsAlive();
        }

        void Join() override
        {
            while (!m_Thread->Join(std::chrono::milliseconds(16)));
        }

        Pulsar::RuntimeState GetState() const override
        {
            ScopeLock _threadLock(*m_Thread);
            return m_Thread->GetContext().GetState();
        }

        void PullStack(Pulsar::Stack& out) override
        {
            ScopeLock _threadLock(*m_Thread);
            out = std::move(m_Thread->GetContext().GetStack());
        }

    private:
        PulsarDebugger::Ref<PulsarDebugger::Thread> m_Thread;
    };

    class ThreadImpl : public PulsarBindings::Std::Thread
    {
    public:
        ThreadImpl(Debugger& debugger)
            : m_Debugger(debugger) {}
        virtual ~ThreadImpl() = default;

        IThreadType::Ref CreateThread(const Pulsar::ExecutionContext& parentContext, const Pulsar::FunctionDefinition& function, Pulsar::Stack&& initStack) const override
        {
            auto thread = m_Debugger.SpawnThread("PulsarStd/Thread", parentContext);
            ScopeLock _threadLock(*thread);

            Pulsar::ExecutionContext& context = thread->GetContext();
            context.GetStack() = std::move(initStack);
            context.CallFunction(function);

            thread->Continue();

            return Pulsar::SharedRef<ThreadTypeImpl>::New(thread);
        }
    private:
        // TODO: WeakRef from Debugger
        //       This requires changes to pulsar/structures/ref.h
        Debugger& m_Debugger;
    };
}

    template<typename T>
    T& Register_Add(PulsarBindings::BindingsRegister& bindings, Debugger& debugger)
        requires ConstructibleWithDebugger<T>
    {
        return bindings.Add<T>(debugger);
    }

    template<typename T>
    T& Register_Add(PulsarBindings::BindingsRegister& bindings, Debugger& debugger)
    {
        PULSAR_UNUSED(debugger);
        return bindings.Add<T>();
    }
}

namespace PulsarDebugger
{

Debugger::Debugger()
    : ILockable<std::recursive_mutex>()
    , m_EventHandler(nullptr)
    , m_Module(Ref<DebuggableModule>::New())
    , m_Breakpoints(Ref<BreakpointRegister>::New(m_Module))
    , m_MainThreadId(0)
    , m_Threads()
{}

std::optional<Debugger::LaunchError> Debugger::Launch(
        Pulsar::StringView scriptPath, Pulsar::Value::List&& args, Pulsar::StringView entryPoint)
{
    // This is required because the Debugger is unlocked while parsing
    std::lock_guard _launchLock(m_LaunchMutex);
    ScopeLock _lock(*this);

    /* === Reset State === */
    m_Breakpoints->Clear();
    // TODO: Join Threads
    m_Threads.Clear();

    auto module = Ref<DebuggableModule>::New();
    m_Module       = module;
    m_Breakpoints  = Ref<BreakpointRegister>::New(m_Module);
    m_MainThreadId = 0;

    // TODO: Add support for external bindings.
    // FIXME: stdio interferes with the communication of DAPServer.
    /* === Bind Natives === */
    auto& bindings = module->GetBindings();
    #define X(binding) PulsarDebugger::Bindings::Register_Add<PulsarDebugger::Bindings::Std::binding>(bindings, *this);
    PULSARBINDINGS_STD_X
    #undef X
    bindings.BindAll(module->Get());

    // TODO: See if Neutron support is possible.
    /* === Parse Source File === */
    Pulsar::ParseResult parseResult;
    Pulsar::Parser parser;

    parseResult = parser.AddSourceFile(scriptPath.ToString());
    if (parseResult != Pulsar::ParseResult::OK) {
        const auto& errorMessage = parser.GetErrorMessage();
        return "Parser Error: " + errorMessage.Message;
    }

    auto parseSettings = Pulsar::ParseSettings_Default;
    parseSettings.Notifications = module->GetParserNotificationsListener();

    { // Allow Threads to run in global producers
        _lock.Unlock();
        parseResult = parser.ParseIntoModule(module->Get(), parseSettings);
        _lock.Lock();
    }

    if (parseResult != Pulsar::ParseResult::OK) {
        const auto& errorMessage = parser.GetErrorMessage();
        return "Parser Error: " + errorMessage.Message;
    }

    /* === Spawn Main Thread === */
    Pulsar::Stack initStack;
    args.Prepend()->Value().SetString(scriptPath.ToString());
    initStack.EmplaceList(std::move(args));
    auto mainThread = SpawnThread("MainThread", entryPoint, std::move(initStack));
    m_MainThreadId = mainThread->GetId();

    return std::nullopt;
}

Ref<BreakpointRegister> Debugger::GetBreakpoints()
{
    return m_Breakpoints;
}

void Debugger::Continue(ThreadId threadId)
{
    auto thread = GetThread(threadId);
    if (thread) thread->Continue();
}

void Debugger::Pause(ThreadId threadId)
{
    auto thread = GetThread(threadId);
    if (thread) thread->Pause();
}


void Debugger::StepInstruction(ThreadId threadId)
{
    auto thread = GetThread(threadId);
    if (thread) thread->StepInstruction();
}

void Debugger::StepOver(ThreadId threadId)
{
    auto thread = GetThread(threadId);
    if (thread) thread->StepOver();
}

void Debugger::StepInto(ThreadId threadId)
{
    auto thread = GetThread(threadId);
    if (thread) thread->StepInto();
}

void Debugger::StepOut(ThreadId threadId)
{
    auto thread = GetThread(threadId);
    if (thread) thread->StepOut();
}

void Debugger::Terminate()
{
    ScopeLock _lock(*this);
    ForEachThread([](auto thread)
    {
        thread->Join(std::chrono::milliseconds(1000));
    });
}

ThreadId Debugger::GetMainThreadId()
{
    ScopeLock _lock(*this);
    return m_MainThreadId;
}

void Debugger::SetEventHandler(EventHandler handler)
{
    ScopeLock _lock(*this);
    m_EventHandler = handler;
}

DebuggableModule::CRef Debugger::GetModule()
{
    ScopeLock _lock(*this);
    return m_Module;
}

Ref<Thread> Debugger::SpawnThread(Pulsar::StringView name, Pulsar::StringView entryPoint, Pulsar::Stack&& initStack)
{
    ScopeLock _lock(*this);
    Pulsar::ExecutionContext eContext(m_Module->Get());
    eContext.GetStack() = std::move(initStack);
    // TODO: Use StringView more often in Pulsar
    eContext.CallFunction(entryPoint.ToString());
    return SpawnThread(name, std::move(eContext));
}

Ref<Thread> Debugger::SpawnThread(Pulsar::StringView name, const Pulsar::ExecutionContext& parentContext)
{
    return SpawnThread(name, parentContext.Fork());
}

Ref<Thread> Debugger::SpawnThread(Pulsar::StringView name, Pulsar::ExecutionContext&& context)
{
    ScopeLock _lock(*this);

    Ref<Thread> thread = Ref<Thread>::New(
            name, m_Breakpoints, std::forward<Pulsar::ExecutionContext>(context));
    thread->SetEventHandler([this](ThreadId threadId, Thread::EEventKind event)
    {
        ScopeLock _lock(*this);
        this->DispatchEvent(threadId, event);
        switch (event) {
        case Thread::EEventKind::Step:
        case Thread::EEventKind::Breakpoint:
        case Thread::EEventKind::Continue:
        case Thread::EEventKind::Pause:
            break;
        case Thread::EEventKind::Done:
        case Thread::EEventKind::Error: {
            this->RemoveThread(threadId);
        } break;
        }
    });

    m_Threads.Emplace(thread->GetId(), thread);
    thread->Start();

    return thread;
}

Ref<Thread> Debugger::GetThread(ThreadId threadId)
{
    ScopeLock _lock(*this);

    if (auto threadIdPair = m_Threads.Find(threadId); threadIdPair) {
        return threadIdPair->Value();
    }

    return nullptr;
}

void Debugger::ForEachThread(std::function<void(Ref<Thread>)> fn)
{
    ScopeLock _lock(*this);
    for (auto [ _, thread ] : m_Threads) {
        if (thread) fn(thread);
    }
}

void Debugger::RemoveThread(ThreadId threadId)
{
    ScopeLock _lock(*this);

    auto threadBucket = m_Threads.Find(threadId);
    if (!threadBucket) return;

    auto thread = threadBucket->Value();
    threadBucket->Delete();
    if (!thread) return;

    thread->Join(std::chrono::milliseconds(1000));
}

void Debugger::DispatchEvent(ThreadId threadId, EEventKind kind)
{
    if (m_EventHandler)
        m_EventHandler(threadId, kind, *this);
}

}
