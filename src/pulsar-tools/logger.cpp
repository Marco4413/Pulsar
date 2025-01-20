#include "pulsar-tools/logger.h"

#include "fmt/color.h"

void PulsarTools::Logger::Info(const std::string& msg) const
{
    if (!DoLogInfo()) return;

    std::string prefix = fmt::to_string(fmt::styled<std::string>("[INFO]:", fmt::fg(fmt::color::light_blue)));
    if (msg.ends_with('\n')) {
        fmt::print(m_Out, "{} {}", prefix, msg);
    } else {
        fmt::println(m_Out, "{} {}", prefix, msg);
    }
}

void PulsarTools::Logger::Error(const std::string& msg) const
{
    if (!DoLogError()) return;

    std::string prefix = fmt::to_string(fmt::styled<std::string>("[ERROR]:", fmt::fg(fmt::color::red)));
    if (msg.ends_with('\n')) {
        fmt::print(m_Err, "{} {}", prefix, msg);
    } else {
        fmt::println(m_Err, "{} {}", prefix, msg);
    }
}
