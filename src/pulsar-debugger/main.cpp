#include <filesystem>

#include <dap/io.h>

#include "pulsar-debugger/dapserver.h"

#ifdef PULSAR_PLATFORM_WINDOWS
#include <fcntl.h>  // _O_BINARY
#include <io.h>     // _setmode
#endif // PULSAR_PLATFORM_WINDOWS

int main(int argc, const char** argv)
{
#ifdef PULSAR_PLATFORM_WINDOWS
    // Change stdin & stdout from text mode to binary mode.
    // This ensures sequences of \r\n are not changed to \n.
    _setmode(_fileno(stdin), _O_BINARY);
    _setmode(_fileno(stdout), _O_BINARY);
#endif // PULSAR_PLATFORM_WINDOWS

    if (argc <= 0) return 1;
    const char* program = argv[0];

    std::filesystem::path      logFilePath = std::filesystem::path(program).replace_filename("pulsar-debugger.log");
    Pulsar::DAPServer::LogFile logFile     = nullptr;

    if (std::filesystem::is_directory(logFilePath.parent_path()) && (!std::filesystem::exists(logFilePath) || std::filesystem::is_regular_file(logFilePath))) {
        logFile = dap::file(logFilePath.generic_string().c_str());
    }

    auto session = dap::Session::create();
    Pulsar::DAPServer dapServer(session, logFile);

    std::shared_ptr<dap::Reader> in = dap::file(stdin, false);
    std::shared_ptr<dap::Writer> out = dap::file(stdout, false);
    if (logFile) {
        session->bind(spy(in, logFile, ""), spy(out, logFile, ""));
    } else {
        session->bind(in, out);
    }

    dapServer.ProcessEvents();
    return 0;
}
