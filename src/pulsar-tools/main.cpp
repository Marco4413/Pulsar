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
    } else if (logLevel == "warning") {
        logger.SetLogLevel(PulsarTools::LogLevel::Warning);
    } else if (logLevel == "error") {
        logger.SetLogLevel(PulsarTools::LogLevel::Error);
    }

    PulsarTools::PositionSettings positionSettings = PulsarTools::PositionSettings_Default;

    std::string_view positionEncoding = *program.Options.PositionEncoding;
    if (positionEncoding == "utf16") {
        positionSettings.Encoding = PulsarTools::PositionEncoding::UTF16;
    } else if (positionEncoding == "utf32") {
        positionSettings.Encoding = PulsarTools::PositionEncoding::UTF32;
    } else if (positionEncoding == "utf8") {
        positionSettings.Encoding = PulsarTools::PositionEncoding::UTF8;
    }

    positionSettings.LineIndexedFrom = 0;
    positionSettings.CharIndexedFrom = 0;
    if (program.Options.PositionIndexedFrom) {
        if (*program.Options.PositionIndexedFrom > 0) {
            positionSettings.LineIndexedFrom = static_cast<size_t>(*program.Options.PositionIndexedFrom);
            positionSettings.CharIndexedFrom = static_cast<size_t>(*program.Options.PositionIndexedFrom);
        }
    } else {
        if (*program.Options.PositionLineIndexedFrom > 0) {
            positionSettings.LineIndexedFrom = static_cast<size_t>(*program.Options.PositionLineIndexedFrom);
        }
        if (*program.Options.PositionCharIndexedFrom > 0) {
            positionSettings.CharIndexedFrom = static_cast<size_t>(*program.Options.PositionCharIndexedFrom);
        }
    }

    PulsarTools::CLI::SetPreferredPositionSettings(positionSettings);

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
