#ifndef _PULSARLSP_HELPERS_H
#define _PULSARLSP_HELPERS_H

#include <lsp/types.h>

#include "pulsar/lexer/token.h"
#include "pulsar/runtime.h"

namespace PulsarLSP
{
    constexpr const char* ValueTypeToString(Pulsar::ValueType type)
    {
        switch (type) {
        case Pulsar::ValueType::Void:
            return "Void";
        case Pulsar::ValueType::Integer:
            return "Integer";
        case Pulsar::ValueType::Double:
            return "Double";
        case Pulsar::ValueType::FunctionReference:
            return "FunctionReference";
        case Pulsar::ValueType::NativeFunctionReference:
            return "NativeFunctionReference";
        case Pulsar::ValueType::List:
            return "List";
        case Pulsar::ValueType::String:
            return "String";
        case Pulsar::ValueType::Custom:
            return "Custom";
        default:
            return "Unknown";
        }
    }

    // This overload resolves custom type names
    inline Pulsar::String ValueTypeToString(const Pulsar::Value& value, const Pulsar::Module& mod)
    {
        Pulsar::ValueType type = value.Type();
        if (type != Pulsar::ValueType::Custom) {
            return ValueTypeToString(type);
        }

        uint64_t customType = value.AsCustom().Type;
        if (!mod.HasCustomType(customType)) {
            return ValueTypeToString(type);
        }

        return mod.GetCustomType(customType).Name;
    }

    constexpr lsp::Range SourcePositionToRange(Pulsar::SourcePosition pos)
    {
        return lsp::Range{
            .start = {
                .line = (lsp::uint)pos.Line,
                .character = (lsp::uint)pos.Char
            },
            .end = {
                .line = (lsp::uint)pos.Line,
                .character = (lsp::uint)(pos.Char+pos.CharSpan)
            }
        };
    }

    constexpr Pulsar::SourcePosition PositionToSourcePosition(lsp::Position position)
    {
        return Pulsar::SourcePosition{
            .Line = static_cast<size_t>(position.line),
            .Char = static_cast<size_t>(position.character),
            .Index    = 0,
            .CharSpan = 0,
        };
    }

    constexpr bool IsNullRange(lsp::Range range)
    {
        return range.start.line == 0 && range.start.character == 0 &&
               range.end.line == 0 && range.end.character == 0;
    }

    constexpr bool IsPositionAfter(lsp::Position pos, Pulsar::SourcePosition marker)
    {
        return (pos.line == marker.Line && pos.character >= marker.Char)
            || pos.line > marker.Line;
    }

    constexpr bool IsPositionBefore(lsp::Position pos, Pulsar::SourcePosition marker)
    {
        // Here, <= is used on purpose.
        // Take this text for example:
        //   Hello, World|
        // Where '|' represents the cursor.
        // We want to consider the cursor as inside the word "World".
        return (pos.line == marker.Line && pos.character <= (marker.Char+marker.CharSpan))
            || pos.line < marker.Line;
    }

    constexpr bool IsPositionInBetween(lsp::Position pos, Pulsar::SourcePosition start, Pulsar::SourcePosition end)
    {
        return IsPositionAfter(pos, start) && IsPositionBefore(pos, end);
    }

    constexpr bool IsPositionInBetween(lsp::Position pos, Pulsar::SourcePosition span)
    {
        return IsPositionInBetween(pos, span, span);
    }
}

#endif // _PULSARLSP_HELPERS_H
