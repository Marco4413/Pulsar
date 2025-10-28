#ifndef _PULSARTOOLS_FMT_H
#define _PULSARTOOLS_FMT_H

#include <fmt/format.h>

#include "pulsar/lexer.h"
#include "pulsar/runtime/value.h"
#include "pulsar/structures/string.h"

template <>
struct fmt::formatter<Pulsar::String> : formatter<string_view>
{
    auto format(const Pulsar::String& val, format_context& ctx) const
    {
        return fmt::format_to(ctx.out(), "{:.{}}", val.CString(), val.Length());
    }
};

template <>
struct fmt::formatter<Pulsar::Value> : formatter<string_view>
{
    auto format(const Pulsar::Value& val, format_context& ctx) const
    {
        switch (val.Type()) {
        case Pulsar::ValueType::Void:
            return fmt::format_to(ctx.out(), "Void");
        case Pulsar::ValueType::Custom:
            return fmt::format_to(ctx.out(), "Custom(.Type={},.Data={})", val.AsCustom().Type, (void*)val.AsCustom().Data.Get());
        case Pulsar::ValueType::Integer:
            return fmt::format_to(ctx.out(), "{}", val.AsInteger());
        case Pulsar::ValueType::FunctionReference:
            return fmt::format_to(ctx.out(), "<&(@{})", val.AsInteger());
        case Pulsar::ValueType::NativeFunctionReference:
            return fmt::format_to(ctx.out(), "<&(*@{})", val.AsInteger());
        case Pulsar::ValueType::Double:
            return fmt::format_to(ctx.out(), "{}", val.AsDouble());
        case Pulsar::ValueType::String:
            return fmt::format_to(ctx.out(), "{}", Pulsar::ToStringLiteral(val.AsString()));
        case Pulsar::ValueType::List: {
            auto start = val.AsList().Front();
            if (!start) return fmt::format_to(ctx.out(), "[ ]");
            fmt::format_to(ctx.out(), "[ {}", start->Value());
            auto next = start->Next();
            while (next) {
                fmt::format_to(ctx.out(), ", {}", next->Value());
                next = next->Next();
            }
            return fmt::format_to(ctx.out(), " ]");
        }
        }
        return fmt::format_to(ctx.out(), "FormatError");
    }
};

#endif // _PULSARTOOLS_FMT_H
