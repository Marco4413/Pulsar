#ifndef _PULSARLSP_HELPERS_H
#define _PULSARLSP_HELPERS_H

#include <lsp/types.h>

#include "pulsar/lexer.h"
#include "pulsar/runtime/value.h"

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
