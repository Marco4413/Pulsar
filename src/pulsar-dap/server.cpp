#include "pulsar-dap/server.h"

#include <format>

#include "pulsar-debugger/types.h"
#include "pulsar-debugger/helpers.h"

namespace dap
{
    DAP_IMPLEMENT_STRUCT_TYPEINFO_EXT(
            PulsarDAP::DebugLaunchRequest,
            LaunchRequest,
            "launch",
            DAP_FIELD(scriptPath, "scriptPath"),
            DAP_FIELD(scriptArgs, "scriptArgs"),
            DAP_FIELD(entryPoint, "entryPoint"),
            DAP_FIELD(stopOnEntry, "stopOnEntry"),
            DAP_FIELD(showAllVariables, "showAllVariables"));
}

namespace PulsarDAP
{

Server::Server(Session& session, LogFile logFile)
    : m_Terminate(false)
    , m_LinesStartAt1(true)
    , m_ColumnsStartAt1(true)
    , m_Session(session)
    , m_LogFile(logFile)
{
    m_Debugger.SetEventHandler([this](
            PulsarDebugger::ThreadId threadId,
            PulsarDebugger::Debugger::EEventKind eventKind,
            PulsarDebugger::Debugger& debugger)
    {
        using EEventKind = PulsarDebugger::Debugger::EEventKind;
        // Invalidate context
        this->m_DebuggerContext.Store(nullptr);
        switch (eventKind) {
        case EEventKind::Step: {
            dap::StoppedEvent ev;
            ev.reason   = "step";
            ev.threadId = threadId;
            ev.allThreadsStopped = false;
            m_Session->send(ev);
        } break;
        case EEventKind::Breakpoint: {
            dap::StoppedEvent ev;
            ev.reason   = "breakpoint";
            ev.threadId = threadId;
            ev.allThreadsStopped = false;
            m_Session->send(ev);
        } break;
        case EEventKind::Continue: {
            dap::ContinuedEvent ev;
            ev.threadId = threadId;
            ev.allThreadsContinued = false;
            m_Session->send(ev);
        } break;
        case EEventKind::Pause: {
            dap::StoppedEvent ev;
            ev.reason   = "pause";
            ev.threadId = threadId;
            ev.allThreadsStopped = false;
            m_Session->send(ev);
        } break;
        case EEventKind::Done: {
            dap::ThreadEvent ev;
            ev.reason   = "exited";
            ev.threadId = threadId;
            m_Session->send(ev);
            if (threadId == debugger.GetMainThreadId())
                this->Terminate();
        } break;
        case EEventKind::Error: {
            dap::StoppedEvent ev;
            ev.reason   = "exception";
            ev.threadId = threadId;
            ev.allThreadsStopped = false;
            auto thread = debugger.GetThread(threadId);
            if (thread) {
                PulsarDebugger::ScopeLock _threadLock(*thread);
                auto runtimeState = thread->GetContext().GetState();
                ev.text = Pulsar::RuntimeStateToString(runtimeState);
            } else {
                ev.text = "Could not retrieve runtime state.";
            }
            m_Session->send(ev);
        } break;
        default: break;
        }
    });

    m_Session->onError([this](const char* errorMessage)
    {
        this->LogF("[Session Error]: {}\n", errorMessage);
        // this->Terminate();
    });

    m_Session->registerHandler([this](const dap::InitializeRequest& req)
    {
        this->m_LinesStartAt1   = !req.linesStartAt1   || *req.linesStartAt1;
        this->m_ColumnsStartAt1 = !req.columnsStartAt1 || *req.columnsStartAt1;

        dap::InitializeResponse res;
        res.supportsConfigurationDoneRequest = true;
        res.supportsSingleThreadExecutionRequests = false;
        return res;
    });

    m_Session->registerSentHandler([this](const dap::ResponseOrError<dap::InitializeResponse>&)
    {
        this->m_Session->send(dap::InitializedEvent());
    });

    m_Session->registerHandler([](const dap::ConfigurationDoneRequest&)
    {
        return dap::ConfigurationDoneResponse();
    });

    m_Session->registerHandler([this](const dap::ThreadsRequest&)
    {
        dap::ThreadsResponse res;
        res.threads = this->GetThreads();
        return res;
    });

    m_Session->registerHandler([this](const dap::ScopesRequest& req) -> dap::ResponseOrError<dap::ScopesResponse>
    {
        auto scopes = this->GetScopes(req.frameId);
        if (!scopes) {
            return dap::Error("Unknown frameId '%d'.", int(req.frameId));
        }

        dap::ScopesResponse res;
        res.scopes = std::move(*scopes);
        return res;
    });

    m_Session->registerHandler([this](const dap::VariablesRequest& req) -> dap::ResponseOrError<dap::VariablesResponse>
    {
        auto variables = this->GetVariables(req.variablesReference, req.start.value(0), req.count.value(0));
        if (!variables) {
            return dap::Error("Unknown variablesReference '%d'.", int(req.variablesReference));
        }

        dap::VariablesResponse res;
        res.variables = std::move(*variables);
        return res;
    });

    m_Session->registerHandler([this](const dap::StackTraceRequest& req) -> dap::ResponseOrError<dap::StackTraceResponse>
    {
        dap::StackTraceResponse res;
        res.totalFrames = 0;

        auto stackFrames = this->GetStackFrames(req.threadId, req.startFrame.value(0), req.levels.value(0), &(*res.totalFrames));
        if (!stackFrames) {
            return dap::Error("Unknown threadId '%d'.", int(req.threadId));
        }

        res.stackFrames = *stackFrames;
        return res;
    });

    m_Session->registerHandler([this](const dap::SetBreakpointsRequest& req) -> dap::ResponseOrError<dap::SetBreakpointsResponse>
    {
        auto breakpoints = this->m_Debugger.GetBreakpoints();
        if (!breakpoints) return dap::SetBreakpointsResponse{};

        PulsarDebugger::ScopeLock _breakpointsLock(*breakpoints);

        auto module = this->m_Debugger.GetModule();
        PulsarDebugger::SourceReference sourceReference = req.source.sourceReference.value(
                PulsarDebugger::INVALID_SOURCE_REFERENCE);
        if (!req.source.sourceReference && req.source.path) {
            sourceReference = module->FindSourceReferenceForPath(req.source.path->c_str());
        }

        dap::SetBreakpointsResponse res;
        if (sourceReference == PulsarDebugger::INVALID_SOURCE_REFERENCE) {
            if (req.breakpoints) {
                res.breakpoints.resize(req.breakpoints->size());

                for (size_t i = 0; i < req.breakpoints->size(); ++i) {
                    auto& resBreakpoint = res.breakpoints[i];
                    resBreakpoint.verified = false;
                    resBreakpoint.message  = "Source not loaded.";
                }
            }
        } else {
            breakpoints->Clear(sourceReference);

            if (req.breakpoints) {
                res.breakpoints.resize(req.breakpoints->size());

                for (size_t i = 0; i < req.breakpoints->size(); ++i) {
                    const auto& reqBreakpoint = (*req.breakpoints)[i];
                    auto& resBreakpoint = res.breakpoints[i];

                    dap::Source breakpointSource;
                    breakpointSource.path = module->GetSourcePath(sourceReference)->CString();
                    breakpointSource.sourceReference = sourceReference;
                    resBreakpoint.source = std::move(breakpointSource);

                    auto breakpointError = breakpoints->Set(
                            sourceReference,
                            this->m_LinesStartAt1
                                ? static_cast<size_t>(reqBreakpoint.line-1)
                                : static_cast<size_t>(reqBreakpoint.line));

                    resBreakpoint.verified = !breakpointError;
                    if (!resBreakpoint.verified) {
                        resBreakpoint.reason  = "failed";
                        resBreakpoint.message = breakpointError->CString();
                    }
                }
            }
        }

        return res;
    });

    m_Session->registerHandler([](const dap::SetExceptionBreakpointsRequest&)
    {
        return dap::SetExceptionBreakpointsResponse();
    });

    m_Session->registerHandler([this](const dap::ContinueRequest& req)
    {
        this->m_Debugger.Continue(req.threadId);
        return dap::ContinueResponse();
    });

    m_Session->registerHandler([this](const dap::PauseRequest& req)
    {
        this->m_Debugger.Pause(req.threadId);
        return dap::PauseResponse();
    });

    m_Session->registerHandler([this](const dap::NextRequest& req)
    {
        this->m_Debugger.StepOver(req.threadId);
        return dap::NextResponse();
    });

    m_Session->registerHandler([this](const dap::StepInRequest& req)
    {
        this->m_Debugger.StepInto(req.threadId);
        return dap::StepInResponse();
    });

    m_Session->registerHandler([this](const dap::StepOutRequest& req)
    {
        this->m_Debugger.StepOut(req.threadId);
        return dap::StepOutResponse();
    });

    m_Session->registerHandler([this](const dap::SourceRequest& req) -> dap::ResponseOrError<dap::SourceResponse>
    {
        auto module = this->m_Debugger.GetModule();

        PulsarDebugger::SourceReference sourceReference = req.sourceReference;
        if (req.source) {
            if (req.source->sourceReference) {
                sourceReference = *req.source->sourceReference;
            } else if (req.source->path) {
                sourceReference = module->FindSourceReferenceForPath(req.source->path->c_str());
            }
        }

        if (sourceReference == PulsarDebugger::INVALID_SOURCE_REFERENCE) {
            return dap::Error("Could not find source.");
        }

        auto source = module->GetSourceContent(sourceReference);
        if (!source) {
            return dap::Error("Unknown source reference '%d'.", int(sourceReference));
        }

        dap::SourceResponse res;
        res.content = source->CString();
        return res;
    });

    m_Session->registerHandler([this](const DebugLaunchRequest& req) -> dap::ResponseOrError<DebugLaunchRequest::Response>
    {
        auto launchError = this->Launch(req);
        if (launchError) {
            return dap::Error("%s\n", launchError->CString());
        }
        return DebugLaunchRequest::Response();
    });

    m_Session->registerHandler([this](const dap::DisconnectRequest&)
    {
        this->Terminate();
        return dap::DisconnectResponse();
    });
}

std::optional<PulsarDebugger::Debugger::LaunchError> Server::Launch(const DebugLaunchRequest& req)
{
    m_ShowAllVariables = req.showAllVariables && *req.showAllVariables;
    return Launch(
            req.scriptPath.c_str(),
            req.scriptArgs ? *req.scriptArgs : dap::array<dap::string>{},
            req.entryPoint ? req.entryPoint->c_str() : "main",
            !req.stopOnEntry || *req.stopOnEntry);
}

std::optional<PulsarDebugger::Debugger::LaunchError> Server::Launch(
        const char* scriptPath, const dap::array<dap::string>& scriptArgs,
        const char* entryPoint, bool stopOnEntry)
{
    Pulsar::Value::List args;
    for (const auto& arg : scriptArgs)
        args.Append()->Value().SetString(arg.c_str());

    LogF("Launching ({}) in '{}'.\n", entryPoint, scriptPath);
    auto launchError = m_Debugger.Launch(scriptPath, std::move(args), entryPoint);
    if (launchError) return launchError;

    LogF("Launched ({}) in '{}'.\n", entryPoint, scriptPath);
    dap::ThreadEvent ev;
    ev.reason   = "started";
    ev.threadId = m_Debugger.GetMainThreadId();
    m_Session->send(ev);

    if (stopOnEntry) {
        this->m_Debugger.Pause(this->m_Debugger.GetMainThreadId());
    } else {
        this->m_Debugger.Continue(this->m_Debugger.GetMainThreadId());
    }
    return std::nullopt;
}

std::optional<dap::string> Server::GetSourceContent(dap::integer sourceReference)
{
    auto module = m_Debugger.GetModule();
    auto source = module->GetSourceContent(sourceReference);
    if (!source) return std::nullopt;
    return source->CString();
}

dap::array<dap::Thread> Server::GetThreads()
{
    PulsarDebugger::ScopeLock _debuggerLock(m_Debugger);
    dap::array<dap::Thread> dapThreads;
    auto debuggerContext = GetOrCreateContext();
    debuggerContext->ForEachThread([&dapThreads](
            PulsarDebugger::ThreadId threadId,
            const PulsarDebugger::DebuggerContext::Thread& thread)
    {
        dap::Thread dapThread;
        dapThread.id   = threadId;
        dapThread.name = thread.Name.CString();
        dapThreads.emplace_back(std::move(dapThread));
    });
    return dapThreads;
}

std::optional<dap::array<dap::StackFrame>> Server::GetStackFrames(dap::integer threadId, dap::integer startFrame, dap::integer levels, dap::integer* _totalFrames)
{
    PulsarDebugger::ScopeLock _debuggerLock(m_Debugger);
    auto debuggerContext = GetOrCreateContext();
    size_t totalFrames = 0;
    auto debuggerStackFrames = debuggerContext->GetOrLoadStackFrames(
            threadId,
            startFrame >= 0 ? static_cast<size_t>(startFrame) : 0,
            levels     >= 0 ? static_cast<size_t>(levels)     : 0,
            &totalFrames);
    if (!debuggerStackFrames) return std::nullopt;

    if (_totalFrames) *_totalFrames = static_cast<dap::integer>(totalFrames);

    dap::array<dap::StackFrame> stackFrames;
    for (size_t i = 0; i < debuggerStackFrames->Size(); ++i) {
        const auto& debuggerStackFrame = (*debuggerStackFrames)[i];
        dap::StackFrame stackFrame;

        dap::Source source;
        source.sourceReference = debuggerStackFrame.SourceReference;
        auto pulsarSource = debuggerContext->GetSource(debuggerStackFrame.SourceReference);
        if (pulsarSource) source.path = pulsarSource->Path.CString();
        stackFrame.source = source;

        stackFrame.id   = debuggerStackFrame.Id;
        stackFrame.name = debuggerStackFrame.Name.CString();
        stackFrame.column = debuggerStackFrame.SourcePos.Char+(m_ColumnsStartAt1 ? 1 : 0);
        stackFrame.line   = debuggerStackFrame.SourcePos.Line+(m_LinesStartAt1   ? 1 : 0);

        stackFrames.emplace_back(std::move(stackFrame));
    }

    return stackFrames;
}

std::optional<dap::array<dap::Scope>> Server::GetScopes(dap::integer frameId)
{
    PulsarDebugger::ScopeLock _debuggerLock(m_Debugger);
    auto debuggerContext = GetOrCreateContext();
    auto debuggerScopes  = debuggerContext->GetOrLoadScopes(frameId);
    if (!debuggerScopes) return std::nullopt;

    dap::array<dap::Scope> scopes;
    for (size_t i = 0; i < debuggerScopes->Size(); ++i) {
        const auto& debuggerScope = (*debuggerScopes)[i];
        dap::Scope scope;
        scope.name               = debuggerScope.Name.CString();
        scope.variablesReference = debuggerScope.VariablesReference;
        scopes.emplace_back(std::move(scope));
    }

    return scopes;
}

std::optional<dap::array<dap::Variable>> Server::GetVariables(dap::integer variablesReference, dap::integer start, dap::integer count)
{
    auto debuggerContext   = GetOrCreateContext();
    auto debuggerVariables = debuggerContext->GetVariables(
            variablesReference,
            start >= 0 ? static_cast<size_t>(start) : 0,
            count >= 0 ? static_cast<size_t>(count) : 0,
            nullptr);
    if (!debuggerVariables) return std::nullopt;

    dap::array<dap::Variable> variables;
    for (size_t i = 0; i < debuggerVariables->Size(); ++i) {
        const auto& debuggerVariable = (*debuggerVariables)[i];
        if (m_ShowAllVariables || debuggerVariable.Visibility == PulsarDebugger::DebuggerContext::Variable::EVisibility::Visible) {
            dap::Variable var;
            var.name = debuggerVariable.Visibility == PulsarDebugger::DebuggerContext::Variable::EVisibility::Shadowed ? "//" : "";
            var.name              += debuggerVariable.Name.CString();
            var.type               = PulsarDebugger::ValueTypeToString(debuggerVariable.Type);
            var.value              = debuggerVariable.Value.CString();
            var.variablesReference = debuggerVariable.VariablesReference;
            variables.emplace_back(std::move(var));
        }
    }

    return variables;
}

void Server::Terminate()
{
    if (m_Terminate) return;
    m_Terminate = true;
    m_Terminate.notify_all();
    m_Session->send(dap::TerminatedEvent());
}

void Server::ProcessEvents()
{
    while (!m_Terminate) {
        m_Terminate.wait(false);
    }

    m_Debugger.Terminate();
}

PulsarDebugger::Ref<PulsarDebugger::DebuggerContext> Server::GetOrCreateContext()
{
    PulsarDebugger::ScopeLock _debuggerContextRefLock(m_DebuggerContext);
    auto context = m_DebuggerContext.Load();
    if (!context) {
        context = PulsarDebugger::Ref<PulsarDebugger::DebuggerContext>::New(m_Debugger);
        m_DebuggerContext.Store(context);
    }
    return context;
}

}
