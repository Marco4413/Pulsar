#include <pulsar-lsp/server.h>

#include <lsp/io/standardio.h>

int main(int argc, const char** argv)
{
    (void)argc;
    (void)argv;

    lsp::Connection connection{lsp::io::standardInput(), lsp::io::standardOutput()};
    PulsarLSP::Server server(connection);
    server.Run();

    return 0;
}
