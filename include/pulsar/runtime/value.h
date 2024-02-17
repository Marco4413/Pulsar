#ifndef _PULSAR_RUNTIME_VALUE_H
#define _PULSAR_RUNTIME_VALUE_H

#include <cinttypes>

namespace Pulsar
{
    enum class ValueType
    {
        Void, Integer, Double,
        FunctionReference, NativeFunctionReference
    };

    constexpr bool IsNumericValueType(ValueType vtype)
    {
        return vtype == ValueType::Integer || vtype == ValueType::Double;
    }

    constexpr bool IsReferenceValueType(ValueType vtype)
    {
        return vtype == ValueType::FunctionReference
            || vtype == ValueType::NativeFunctionReference;
    }

    struct Value
    {
        constexpr Value()
            : Type(ValueType::Void), AsInteger(0) { }
        constexpr Value(int64_t val)
            : Value(val, ValueType::Integer) { }
        constexpr Value(int64_t val, ValueType type)
            : Type(type), AsInteger(val) { }
        constexpr Value(double val)
            : Type(ValueType::Double), AsDouble(val) { }

        ValueType Type;
        union
        {
            int64_t AsInteger;
            double AsDouble;
        };
    };
}

#endif // _PULSAR_RUNTIME_VALUE_H
