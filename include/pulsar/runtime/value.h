#ifndef _PULSAR_RUNTIME_VALUE_H
#define _PULSAR_RUNTIME_VALUE_H

#include "pulsar/core.h"

#include "pulsar/structures/linkedlist.h"
#include "pulsar/structures/string.h"

namespace Pulsar
{
    enum class ValueType
    {
        Void = 0,
        Integer, Double,
        FunctionReference, NativeFunctionReference,
        List, String
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

    class Value; // Forward Declaration
    typedef LinkedList<Value> ValueList;

    class Value
    {
    public:
        // Type = Void
        Value() { PULSAR_MEMSET((void*)this, 0, sizeof(Value)); }
        ~Value() { DeleteValue(); }

        Value(const Value& other)
            : Value() { *this = other; }

        Value(Value&& other)
            : Value() { *this = std::move(other); }

        Value& operator=(const Value& other)
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
            }
            return *this;
        }

        Value& operator=(Value&& other)
        {
            switch (other.Type()) {
            case ValueType::Void:
            case ValueType::Integer:
            case ValueType::Double:
            case ValueType::FunctionReference:
            case ValueType::NativeFunctionReference:
                *this = (const Value&)other;
                break;
            case ValueType::List:
                SetList(std::move(other.m_AsList));
                break;
            case ValueType::String:
                SetString(std::move(other.m_AsString));
                break;
            }
            other.DeleteValue();
            return *this;
        }

        bool operator==(const Value& other) const
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
            case ValueType::List:
                const ValueList::NodeType* aNext = AsList().Front();
                const ValueList::NodeType* bNext = other.AsList().Front();
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
        
        bool operator!=(const Value& other) const { return !(*this == other); }

        ValueType Type() const    { return m_Type; }
        int64_t AsInteger() const { return m_AsInteger; }
        double AsDouble() const   { return m_AsDouble; }
        ValueList& AsList()             { return m_AsList; }
        const ValueList& AsList() const { return m_AsList; }
        String& AsString()             { return m_AsString; }
        const String& AsString() const { return m_AsString; }

        Value& SetVoid()                               { DeleteValue(); m_Type = ValueType::Void; m_AsInteger = 0; return *this; }
        Value& SetInteger(int64_t val)                 { DeleteValue(); m_Type = ValueType::Integer; m_AsInteger = val; return *this; }
        Value& SetDouble(double val)                   { DeleteValue(); m_Type = ValueType::Double; m_AsDouble = val; return *this; }
        Value& SetFunctionReference(int64_t val)       { DeleteValue(); m_Type = ValueType::FunctionReference; m_AsInteger = val; return *this; }
        Value& SetNativeFunctionReference(int64_t val) { DeleteValue(); m_Type = ValueType::NativeFunctionReference; m_AsInteger = val; return *this; }
        Value& SetList(ValueList&& val)                { DeleteValue(); m_Type = ValueType::List; m_AsList = std::move(val); return *this; }
        Value& SetList(const ValueList& val)           { DeleteValue(); m_Type = ValueType::List; m_AsList = val; return *this; }
        Value& SetString(String&& val)                 { DeleteValue(); m_Type = ValueType::String; m_AsString = std::move(val); return *this; }
        Value& SetString(const String& val)            { DeleteValue(); m_Type = ValueType::String; m_AsString = val; return *this; }

    private:
        void DeleteValue()
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
            }
            m_Type = ValueType::Void;
        }

        ValueType m_Type;
        union
        {
            int64_t m_AsInteger;
            double m_AsDouble;
            ValueList m_AsList;
            String m_AsString;
        };
    };
}

#endif // _PULSAR_RUNTIME_VALUE_H
