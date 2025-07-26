#include "pulsar-debugger/helpers.h"

#include <format>

#include <pulsar/lexer.h>

const char* PulsarDebugger::ValueTypeToString(Pulsar::ValueType type)
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

Pulsar::String PulsarDebugger::ValueToString(const Pulsar::Value& value, bool recursive)
{
    switch (value.Type()) {
    case Pulsar::ValueType::Void:
        return "Void";
    case Pulsar::ValueType::Integer:
        return std::format("{}", value.AsInteger()).c_str();
    case Pulsar::ValueType::Double:
        return std::format("{}", value.AsDouble()).c_str();
    case Pulsar::ValueType::FunctionReference:
        return std::format("<&(@{})", value.AsInteger()).c_str();
    case Pulsar::ValueType::NativeFunctionReference:
        return std::format("<&(*@{})", value.AsInteger()).c_str();
    case Pulsar::ValueType::List: {
        if (!value.AsList().Front())
            return "[]";

        if (!recursive)
            return "[...]";

        Pulsar::String asString;
        asString  = "[ ";

        const auto* node = value.AsList().Front();
        asString += ValueToString(node->Value(), recursive);
        node = node->Next();

        for (; node; node = node->Next()) {
            asString += ", ";
            asString += ValueToString(node->Value(), recursive);
        }

        asString += " ]";
        return asString;
    }
    case Pulsar::ValueType::String:
        return Pulsar::ToStringLiteral(value.AsString());
    case Pulsar::ValueType::Custom:
        return std::format("Custom(.Type={},.Data={})", value.AsCustom().Type, (void*)value.AsCustom().Data.Get()).c_str();
    default:
        return "Unknown";
    }
}
