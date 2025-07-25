#ifndef _PULSARDEBUGGER_DAPSERVER_H
#define _PULSARDEBUGGER_DAPSERVER_H

#include <atomic>
#include <optional>

#include <dap/io.h>
#include <dap/protocol.h>
#include <dap/session.h>

#include "pulsar-debugger/debugger.h"

namespace Pulsar
{
    class DebugLaunchRequest
        : public dap::LaunchRequest
    {
    public:
        using Response = dap::LaunchResponse;

    public:
        dap::string scriptPath;
        dap::optional<dap::array<dap::string>> scriptArgs;
        dap::optional<dap::string> entryPoint;
        dap::optional<dap::boolean> stopOnEntry;
    };

    class DAPServer
    {
    public:
        using Session = std::unique_ptr<dap::Session>;
        using LogFile = std::shared_ptr<dap::Writer>;

    public:
        DAPServer(Session &session, LogFile logFile=nullptr);
        ~DAPServer() = default;

        DAPServer(const DAPServer&)  = delete;
        DAPServer(DAPServer&&)       = delete;
        DAPServer& operator=(const DAPServer&)  = delete;
        DAPServer& operator=(DAPServer&&)       = delete;

        std::optional<Debugger::LaunchError> Launch(const DebugLaunchRequest& launchRequest);
        std::optional<Debugger::LaunchError> Launch(const char* scriptPath, const dap::array<dap::string>& scriptArgs, const char* entryPoint="main");

        std::optional<dap::array<dap::StackFrame>> GetStackFrames(dap::integer threadId);
        std::optional<dap::array<dap::Scope>> GetScopes(dap::integer frameId);
        std::optional<dap::array<dap::Variable>> GetVariables(dap::integer scopeId);
        std::optional<dap::string> GetSourceContent(dap::integer sourceReference);

        void Terminate();
        void ProcessEvents();

    private:
        template<typename ...Args>
        bool LogF(const char* msg, Args&&... args) const
        {
            if (!m_LogFile) return false;
            dap::writef(m_LogFile, "\n");
            dap::writef(m_LogFile, msg, std::forward<Args>(args)...);
            dap::writef(m_LogFile, "\n");
            return true;
        }

    private:
        std::atomic_bool m_Terminate;
        std::atomic_bool m_LinesStartAt1;
        std::atomic_bool m_ColumnsStartAt1;

        Session &m_Session;
        LogFile m_LogFile;
        Debugger m_Debugger;
    };
}

namespace dap
{
    DAP_DECLARE_STRUCT_TYPEINFO(Pulsar::DebugLaunchRequest);
}

#endif // _PULSARDEBUGGER_DAPSERVER_H
