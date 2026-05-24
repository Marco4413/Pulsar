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
