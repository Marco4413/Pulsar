#include "pulsar-debugger/dapserver.h"

#include "pulsar-debugger/types.h"
#include "pulsar-debugger/helpers.h"

#include <format>

namespace dap
{
    DAP_IMPLEMENT_STRUCT_TYPEINFO_EXT(
            PulsarDebugger::DebugLaunchRequest,
            LaunchRequest,
            "launch",
            DAP_FIELD(scriptPath, "scriptPath"),
            DAP_FIELD(scriptArgs, "scriptArgs"),
            DAP_FIELD(entryPoint, "entryPoint"),
            DAP_FIELD(stopOnEntry, "stopOnEntry"),
            DAP_FIELD(showAllVariables, "showAllVariables"));
}

namespace PulsarDebugger
{

DAPServer::DAPServer(Session& session, LogFile logFile)
    : m_Terminate(false)
    , m_LinesStartAt1(true)
    , m_ColumnsStartAt1(true)
    , m_Session(session)
    , m_LogFile(logFile)
{
    m_Debugger.SetEventHandler([this](ThreadId threadId, Debugger::EEventKind debugEv, Debugger& debugger)
    {
        // Invalidate context
        this->m_DebuggerContext = nullptr;
        switch (debugEv) {
        case Debugger::EEventKind::Step: {
            dap::StoppedEvent ev;
            ev.reason   = "step";
            ev.threadId = threadId;
            m_Session->send(ev);
        } break;
        case Debugger::EEventKind::Breakpoint: {
            dap::StoppedEvent ev;
            ev.reason   = "breakpoint";
            ev.threadId = threadId;
            m_Session->send(ev);
        } break;
        case Debugger::EEventKind::Continue: {
            dap::ContinuedEvent ev;
            ev.threadId = threadId;
            m_Session->send(ev);
        } break;
        case Debugger::EEventKind::Pause: {
            dap::StoppedEvent ev;
            ev.reason   = "pause";
            ev.threadId = threadId;
            m_Session->send(ev);
        } break;
        case Debugger::EEventKind::Done: {
            dap::ThreadEvent ev;
            ev.reason   = "exited";
            ev.threadId = threadId;
            m_Session->send(ev);
            this->Terminate();
        } break;
        case Debugger::EEventKind::Error: {
            dap::StoppedEvent ev;
            ev.reason   = "exception";
            ev.threadId = threadId;
            auto thread = debugger.GetThread(threadId);
            if (thread) {
                auto runtimeState = thread->GetCurrentState();
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
        ThreadId mainThreadId = m_Debugger.GetMainThreadId();
        this->m_Debugger.ForEachThread([&res, mainThreadId](std::shared_ptr<Thread> thread)
        {
            ThreadId threadId = thread->GetId();

            dap::Thread dapThread;
            dapThread.id = threadId;
            if (threadId != mainThreadId) {
                dapThread.name = std::format("Thread-{}", thread->GetId());
            } else {
                dapThread.name = std::format("MainThread ({})", thread->GetId());
            }
            res.threads.push_back(dapThread);
        });
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
        auto debuggableModule = this->m_Debugger.GetModule();
        SourceReference sourceReference = req.source.sourceReference.value(INVALID_SOURCE_REFERENCE);
        if (!req.source.sourceReference && req.source.path) {
            if (debuggableModule) {
                sourceReference = debuggableModule->FindSourceReferenceForPath(req.source.path->c_str());
            }
        }

        dap::SetBreakpointsResponse res;
        if (sourceReference == INVALID_SOURCE_REFERENCE) {
            if (req.breakpoints) {
                res.breakpoints.resize(req.breakpoints->size());
    
                for (size_t i = 0; i < req.breakpoints->size(); ++i) {
                    auto& resBreakpoint = res.breakpoints[i];
                    resBreakpoint.verified = false;
                    resBreakpoint.message  = "Source not loaded.";
                }
            }
        } else {
            this->m_Debugger.ClearBreakpoints(sourceReference);

            if (req.breakpoints) {
                res.breakpoints.resize(req.breakpoints->size());
    
                for (size_t i = 0; i < req.breakpoints->size(); ++i) {
                    const auto& reqBreakpoint = (*req.breakpoints)[i];
                    auto& resBreakpoint = res.breakpoints[i];
    
                    if (debuggableModule) {
                        dap::Source breakpointSource;
                        breakpointSource.path = debuggableModule->GetSourcePath(sourceReference)->CString();
                        breakpointSource.sourceReference = sourceReference;
                        resBreakpoint.source = std::move(breakpointSource);
                    }
    
                    auto breakpointError = this->m_Debugger.SetBreakpoint(
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
        auto debuggableModule = this->m_Debugger.GetModule();
        if (!debuggableModule) {
            return dap::Error("No module is being debugged.");
        }

        SourceReference sourceReference = req.sourceReference;
        if (req.source) {
            if (req.source->sourceReference) {
                sourceReference = *req.source->sourceReference;
            } else if (req.source->path) {
                sourceReference = debuggableModule->FindSourceReferenceForPath(req.source->path->c_str());
            }
        }

        if (sourceReference == INVALID_SOURCE_REFERENCE) {
            return dap::Error("Could not find source.");
        }

        auto source = debuggableModule->GetSourceContent(sourceReference);
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

std::optional<Debugger::LaunchError> DAPServer::Launch(const DebugLaunchRequest& req)
{
    m_ShowAllVariables = req.showAllVariables && *req.showAllVariables;
    return Launch(
            req.scriptPath.c_str(),
            req.scriptArgs ? *req.scriptArgs : dap::array<dap::string>{},
            req.entryPoint ? req.entryPoint->c_str() : "main",
            !req.stopOnEntry || *req.stopOnEntry);
}

std::optional<Debugger::LaunchError> DAPServer::Launch(
        const char* scriptPath, const dap::array<dap::string>& scriptArgs,
        const char* entryPoint, bool stopOnEntry)
{
    Pulsar::ValueList args;
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

std::optional<dap::string> DAPServer::GetSourceContent(dap::integer sourceReference)
{
    auto debuggableModule = m_Debugger.GetModule();
    if (!debuggableModule) return std::nullopt;
    auto source = debuggableModule->GetSourceContent(sourceReference);
    if (!source) return std::nullopt;
    return source->CString();
}

std::optional<dap::array<dap::StackFrame>> DAPServer::GetStackFrames(dap::integer threadId, dap::integer startFrame, dap::integer levels, dap::integer* _totalFrames)
{
    DebuggerScopeLock _lock(m_Debugger);
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

std::optional<dap::array<dap::Scope>> DAPServer::GetScopes(dap::integer frameId)
{
    DebuggerScopeLock _lock(m_Debugger);
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

std::optional<dap::array<dap::Variable>> DAPServer::GetVariables(dap::integer variablesReference, dap::integer start, dap::integer count)
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
        if (m_ShowAllVariables || debuggerVariable.Visibility == DebuggerContext::Variable::EVisibility::Visible) {
            dap::Variable var;
            var.name = debuggerVariable.Visibility == DebuggerContext::Variable::EVisibility::Shadowed ? "//" : "";
            var.name              += debuggerVariable.Name.CString();
            var.type               = ValueTypeToString(debuggerVariable.Type);
            var.value              = debuggerVariable.Value.CString();
            var.variablesReference = debuggerVariable.VariablesReference;
            variables.emplace_back(std::move(var));
        }
    }

    return variables;
}

void DAPServer::Terminate()
{
    if (m_Terminate) return;
    m_Terminate = true;
    m_Session->send(dap::TerminatedEvent());
}

void DAPServer::ProcessEvents()
{
    while (!m_Terminate) {
        m_Debugger.WaitForEvent();
        m_Debugger.ProcessEvent();
    }
}

std::shared_ptr<DebuggerContext> DAPServer::GetOrCreateContext()
{
    if (!m_DebuggerContext) m_DebuggerContext = std::make_shared<DebuggerContext>(m_Debugger);
    return m_DebuggerContext;
}

}
