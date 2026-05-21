#ifndef _PULSARTOOLS_LOGGER_H
#define _PULSARTOOLS_LOGGER_H

#include <cstdio>
#include <string>

#include <fmt/color.h>

#include "pulsar-tools/fmt.h"

namespace PulsarTools
{
    enum class LogLevel
    {
        All, Warning, Error
    };

    class Logger
    {
    public:
        Logger(std::FILE* out, std::FILE* err) :
            m_Out(out), m_Err(err)
        {}

        ~Logger() = default;

        bool GetPrefix() const { return m_Prefix; }
        void SetPrefix(bool prefix)
        {
            m_Prefix = prefix;
        }

        bool GetColor() const { return m_Color; }
        void SetColor(bool color)
        {
            m_Color = color;
        }

        LogLevel GetLogLevel() const { return m_LogLevel; }
        void SetLogLevel(LogLevel newLevel)
        {
            m_LogLevel = newLevel;
        }

        template<typename ...Args>
        void Info(fmt::format_string<Args...> format, Args&& ...args) const
        {
            if (!DoLogInfo()) return;
            Info(fmt::vformat(format, fmt::make_format_args(args...)));
        }

        template<typename ...Args>
        void Warn(fmt::format_string<Args...> format, Args&& ...args) const
        {
            if (!DoLogWarn()) return;
            Warn(fmt::vformat(format, fmt::make_format_args(args...)));
        }

        template<typename ...Args>
        void Error(fmt::format_string<Args...> format, Args&& ...args) const
        {
            if (!DoLogError()) return;
            Error(fmt::vformat(format, fmt::make_format_args(args...)));
        }

        void Info(const std::string& msg) const;
        void Warn(const std::string& msg) const;
        void Error(const std::string& msg) const;

        void PutNewLine() const
        {
            std::fputc('\n', m_Out);
        }
    private:
        bool DoLogInfo() const  { return m_LogLevel == LogLevel::All; }
        bool DoLogWarn() const  { return m_LogLevel == LogLevel::All || m_LogLevel == LogLevel::Warning; }
        bool DoLogError() const { return true; }

        void Log(FILE* file, fmt::color color, const char* prefix, const std::string& msg) const;

    private:
        bool m_Prefix;
        bool m_Color;
        LogLevel m_LogLevel;
        std::FILE* m_Out;
        std::FILE* m_Err;
    };
}

#endif // _PULSARTOOLS_LOGGER_H
