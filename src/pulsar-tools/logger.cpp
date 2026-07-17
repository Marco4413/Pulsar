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
    if (m_Prefix) {
        std::string styledPrefix = m_Color
            ? std::format("{}[{}]:{}", color, prefix, PULSARTOOLS_FMT_RESET)
            : std::format("[{}]:", prefix);
        if (msg.ends_with('\n')) {
            std::fputs(std::format("{} {}", styledPrefix, msg).c_str(), file);
        } else {
            std::fputs(std::format("{} {}\n", styledPrefix, msg).c_str(), file);
        }
    } else {
        std::fputs(std::format("{}\n", msg).c_str(), file);
    }
}
