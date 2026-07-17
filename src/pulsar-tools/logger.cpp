#include "pulsar-tools/logger.h"

void PulsarTools::Logger::Info(const std::string& msg) const
{
    if (!DoLogInfo()) return;
    Log(m_Out, PULSARTOOLS_FMT_LIGHT_BLUE, "INFO", msg);
}

void PulsarTools::Logger::Warn(const std::string& msg) const
{
    if (!DoLogWarn()) return;
    Log(m_Out, PULSARTOOLS_FMT_ORANGE, "WARN", msg);
}

void PulsarTools::Logger::Error(const std::string& msg) const
{
    if (!DoLogError()) return;
    Log(m_Err, PULSARTOOLS_FMT_RED, "ERROR", msg);
}

void PulsarTools::Logger::Log(FILE* file, const char* color, const char* prefix, const std::string& msg) const
{
    std::string fullMessage;
    fullMessage.reserve(msg.size()+64);

    if (m_Prefix) {
        if (m_Color) fullMessage += color;
        fullMessage += "[";
        fullMessage += prefix;
        fullMessage += "]:";
        if (m_Color) fullMessage += PULSARTOOLS_FMT_RESET;
        fullMessage += ' ';
        fullMessage += msg;
    } else {
        fullMessage += msg;
    }

    if (!fullMessage.ends_with('\n'))
        fullMessage += '\n';

    fputs(fullMessage.c_str(), file);
}
