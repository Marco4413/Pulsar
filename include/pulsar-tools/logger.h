#ifndef _PULSARTOOLS_LOGGER_H
#define _PULSARTOOLS_LOGGER_H

#include <cstdio>
#include <string>

#include "pulsar-tools/fmt.h"

namespace PulsarTools
{
    enum class LogLevel
    {
        All, Error
    };

    class Logger
    {
    public:
        Logger(std::FILE* out, std::FILE* err) :
            m_Out(out), m_Err(err)
        {}

        ~Logger() = default;

        LogLevel GetLogLevel() const { return m_LogLevel; }
        void SetLogLevel(LogLevel newLevel)
        {
            m_LogLevel = newLevel;
        }

        template<typename ...Args>
        void Info(fmt::format_string<Args...> format, Args&& ...args) const
        {
            if (!DoLogInfo()) return;
            std::string msg = fmt::vformat(format, fmt::make_format_args(args...));
            Info(msg);
        }

        template<typename ...Args>
        void Error(fmt::format_string<Args...> format, Args&& ...args) const
        {
            if (!DoLogError()) return;
            std::string msg = fmt::vformat(format, fmt::make_format_args(args...));
            Error(msg);
        }

        void Info(const std::string& msg) const;
        void Error(const std::string& msg) const;

        void PutNewLine() const
        {
            std::fputc('\n', m_Out);
        }
    private:
        bool DoLogInfo() const  { return m_LogLevel == LogLevel::All; }
        bool DoLogError() const { return true; }

    private:
        LogLevel m_LogLevel;
        std::FILE* m_Out;
        std::FILE* m_Err;
    };
}

#endif // _PULSARTOOLS_LOGGER_H
