#include <lsp/io/standardio.h>

#include "pulsar-lsp/server.h"

int main(int argc, const char** argv)
{
    PULSAR_UNUSED(argc, argv);

    lsp::Connection connection(lsp::io::standardIO());
    PulsarLSP::Server server(connection);
    server.Run();

    return 0;
}
