#ifndef _PULSARTOOLS_PRINT_H
#define _PULSARTOOLS_PRINT_H

#include "pulsar-tools/core.h"

#include "pulsar/lexer.h"
#include "pulsar/runtime.h"
#include "pulsar/structures/string.h"
#include "pulsar/structures/stringview.h"

namespace PulsarTools
{
    struct TokenViewRange { size_t Before; size_t After; };
    constexpr TokenViewRange TokenViewRange_Default{20, 20};

    size_t PrintTokenView(const Pulsar::String& source, const Pulsar::Token& token, TokenViewRange viewRange = TokenViewRange_Default);
    void PrintPrettyError(
        const Pulsar::String* source, const Pulsar::String* filepath,
        const Pulsar::Token& token, const Pulsar::String& message,
        TokenViewRange viewRange = TokenViewRange_Default);
    void PrintPrettyRuntimeError(const Pulsar::ExecutionContext& context, TokenViewRange viewRange = TokenViewRange_Default);
}

template <>
struct fmt::formatter<Pulsar::String> : formatter<string_view>
{
    auto format(const Pulsar::String& val, format_context& ctx) const
    {
        return fmt::format_to(ctx.out(), "{:.{}}", val.Data(), val.Length());
    }
};

template <>
struct fmt::formatter<Pulsar::StringView> : formatter<string_view>
{
    auto format(const Pulsar::StringView& val, format_context& ctx) const
    {
        return fmt::format_to(ctx.out(), "{:.{}}", val.CStringFrom(0), val.Length());
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
            return fmt::format_to(ctx.out(), "Custom(.Type={},.Handle={})", val.AsCustom().Type, val.AsCustom().Handle);
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

#endif // _PULSARTOOLS_PRINT_H
