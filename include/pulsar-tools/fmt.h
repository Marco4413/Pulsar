#ifndef _PULSARTOOLS_FMT_H
#define _PULSARTOOLS_FMT_H

#include <fmt/format.h>

#include "pulsar/structures/string.h"

template <>
struct fmt::formatter<Pulsar::String> : formatter<string_view>
{
    auto format(const Pulsar::String& val, format_context& ctx) const
    {
        return fmt::format_to(ctx.out(), "{:.{}}", val.CString(), val.Length());
    }
};

#endif // _PULSARTOOLS_FMT_H
