#include "pulsar-tools/cli.h"

#include "pulsar-tools/version.h"

int main(int argc, const char** argv)
{
    PulsarTools::CLI::Program program(argv[0]);
    program.Parser.Parse(argc, argv);

    std::string_view logLevel = *program.Options.LogLevel;
    PulsarTools::Logger& logger = PulsarTools::CLI::GetLogger();

    logger.SetPrefix(*program.Options.Prefix);
    logger.SetColor(*program.Options.Color);
    if (logLevel == "all") {
        logger.SetLogLevel(PulsarTools::LogLevel::All);
    } else if (logLevel == "error") {
        logger.SetLogLevel(PulsarTools::LogLevel::Error);
    }

    if (!program.Parser) {
        logger.Error(program.Parser.GetError());
        return 1;
    }

    if (*program.Options.Version) {
        logger.Info("Pulsar-Tools v{}", PulsarTools::Version::ToString(PulsarTools::GetToolsVersion()));
        logger.Info("Pulsar v{}", PulsarTools::Version::ToString(PulsarTools::GetPulsarVersion()));
        logger.Info("Neutron v{}", PulsarTools::GetNeutronVersion());
        return 0;
    } else if (program.CmdCheck) {
        return program.CmdCheck();
    } else if (program.CmdCompile) {
        return program.CmdCompile();
    } else if (program.CmdRun) {
        return program.CmdRun();
    } else {
        Argue::TextBuilder helpMsg;
        program.CmdHelp(helpMsg);
        logger.Info(helpMsg.Build());
    }

    return 0;
}
