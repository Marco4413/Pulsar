#include "pulsar/runtime/value.h"

Pulsar::Value::Value()
{
    PULSAR_MEMSET((void*)this, 0, sizeof(Value));
}

Pulsar::Value::~Value()
{
    Reset();
}

Pulsar::Value& Pulsar::Value::operator=(const Value& other)
{
    switch (other.Type()) {
    case ValueType::Void:
        return SetVoid();
    case ValueType::Integer:
        return SetInteger(other.AsInteger());
    case ValueType::Double:
        return SetDouble(other.AsDouble());
    case ValueType::FunctionReference:
        return SetFunctionReference(other.AsInteger());
    case ValueType::NativeFunctionReference:
        return SetNativeFunctionReference(other.AsInteger());
    case ValueType::List:
        return SetList(other.AsList());
    case ValueType::String:
        return SetString(other.AsString());
    case ValueType::Custom:
        return SetCustom(other.AsCustom());
    }
    return *this;
}

Pulsar::Value& Pulsar::Value::operator=(Value&& other)
{
    switch (other.Type()) {
    case ValueType::Void:
    case ValueType::Integer:
    case ValueType::Double:
    case ValueType::FunctionReference:
    case ValueType::NativeFunctionReference:
    case ValueType::Custom:
        *this = (const Value&)other;
        break;
    case ValueType::List:
        SetList(std::move(other.m_AsList));
        break;
    case ValueType::String:
        SetString(std::move(other.m_AsString));
        break;
    }
    other.Reset();
    return *this;
}

bool Pulsar::Value::operator==(const Value& other) const
{
    if (m_Type != other.m_Type)
        return false;
    switch (m_Type) {
    case ValueType::Void:
        return true;
    case ValueType::Integer:
    case ValueType::FunctionReference:
    case ValueType::NativeFunctionReference:
        return AsInteger() == other.AsInteger();
    case ValueType::Double:
        return AsDouble() == other.AsDouble();
    case ValueType::String:
        return AsString() == other.AsString();
    case ValueType::Custom:
        return AsCustom().Type == other.AsCustom().Type
            && AsCustom().Data == other.AsCustom().Data;
    case ValueType::List:
        const ValueList::Node* aNext = AsList().Front();
        const ValueList::Node* bNext = other.AsList().Front();
        while (true) {
            if (aNext == bNext)
                return true;
            else if (!aNext || !bNext)
                return false;
            else if (aNext->Value() != bNext->Value())
                return false;
            aNext = aNext->Next();
            bNext = bNext->Next();
        }
    }
    return false;
}

void Pulsar::Value::Reset()
{
    switch (m_Type) {
    case ValueType::Void:
    case ValueType::Integer:
    case ValueType::Double:
    case ValueType::FunctionReference:
    case ValueType::NativeFunctionReference:
        m_AsInteger = 0;
        break;
    case ValueType::List:
        m_AsList.~LinkedList();
        PULSAR_MEMSET((void*)&m_AsList, 0, sizeof(ValueList));
        break;
    case ValueType::String:
        m_AsString.~String();
        PULSAR_MEMSET((void*)&m_AsString, 0, sizeof(String));
        break;
    case ValueType::Custom:
        // Call Destructor
        m_AsCustom.~CustomData();
        PULSAR_MEMSET((void*)&m_AsCustom, 0, sizeof(CustomData));
        break;
    }
    m_Type = ValueType::Void;
}
