#include "pulsar-tools/logger.h"

#include <fmt/color.h>

void PulsarTools::Logger::Info(const std::string& msg) const
{
    if (!DoLogInfo()) return;
    Log(m_Out, fmt::color::light_blue, "INFO", msg);
}

void PulsarTools::Logger::Warn(const std::string& msg) const
{
    if (!DoLogWarn()) return;
    Log(m_Out, fmt::color::orange, "WARN", msg);
}

void PulsarTools::Logger::Error(const std::string& msg) const
{
    if (!DoLogError()) return;
    Log(m_Err, fmt::color::red, "ERROR", msg);
}

void PulsarTools::Logger::Log(FILE* file, fmt::color color, const char* prefix, const std::string& msg) const
{
    if (m_Prefix) {
        std::string styledPrefix = m_Color
            ? fmt::format(fmt::fg(color), "[{}]:", prefix)
            : fmt::format("[{}]:", prefix);
        if (msg.ends_with('\n')) {
            fmt::print(file, "{} {}", styledPrefix, msg);
        } else {
            fmt::println(file, "{} {}", styledPrefix, msg);
        }
    } else {
        fmt::println(file, "{}", msg);
    }
}
