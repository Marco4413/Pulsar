#ifndef _PULSARTOOLS_FMT_H
#define _PULSARTOOLS_FMT_H

#include <format>

#include "pulsar/structures/string.h"

#define PULSARTOOLS_FMT_LIGHT_BLUE "\x1B[38;2;173;216;230m"
#define PULSARTOOLS_FMT_ORANGE     "\x1B[38;2;255;165;0m"
#define PULSARTOOLS_FMT_RED        "\x1B[38;2;255;0;0m"
#define PULSARTOOLS_FMT_RESET      "\x1B[0m"

template <>
struct std::formatter<Pulsar::String> : formatter<string_view>
{
    auto format(const Pulsar::String& val, format_context& ctx) const
    {
        return std::format_to(ctx.out(), "{:.{}}", val.CString(), val.Length());
    }
};

#endif // _PULSARTOOLS_FMT_H
