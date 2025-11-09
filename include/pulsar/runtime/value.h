#ifndef _PULSAR_RUNTIME_VALUE_H
#define _PULSAR_RUNTIME_VALUE_H

#include "pulsar/core.h"

#include "pulsar/structures/linkedlist.h"
#include "pulsar/structures/ref.h"
#include "pulsar/structures/string.h"

namespace Pulsar
{
    class CustomDataHolder
    {
    public:
        using Ref = SharedRef<Pulsar::CustomDataHolder>;
        virtual ~CustomDataHolder() = default;
    };

    struct CustomData
    {
        uint64_t Type;
        CustomDataHolder::Ref Data = nullptr;

        // Should cast to a derived of CustomDataHolder
        template<typename T>
        SharedRef<T> As() { return Data.CastTo<T>(); }
    };

    enum class ValueType
    {
        Void    = 0x00,
        Integer = 0x01,
        Double  = 0x02,
        FunctionReference       = 0x03,
        NativeFunctionReference = 0x04,
        List    = 0x05,
        String  = 0x06,
        Custom  = 0xFF
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
    // TODO: Move into Value::List
    using ValueList = LinkedList<Value>;

    // Forward declaration for ToString and ToRepr
    class Module;

    class Value
    {
    public:
        // Type = Void
        Value();
        ~Value();

        Value(const Value& other)
            : Value() { *this = other; }

        Value(Value&& other)
            : Value() { *this = std::move(other); }

        Value& operator=(const Value& other);
        Value& operator=(Value&& other);

        bool operator==(const Value& other) const;
        bool operator!=(const Value& other) const { return !(*this == other); }

        ValueType Type() const    { return m_Type; }
        int64_t AsInteger() const { return m_AsInteger; }
        double AsDouble() const   { return m_AsDouble; }
        ValueList& AsList()             { return m_AsList; }
        const ValueList& AsList() const { return m_AsList; }
        String& AsString()             { return m_AsString; }
        const String& AsString() const { return m_AsString; }
        CustomData& AsCustom()             { return m_AsCustom; }
        const CustomData& AsCustom() const { return m_AsCustom; }

        Value& SetVoid()                               { Reset(); m_Type = ValueType::Void;                    m_AsInteger = 0;              return *this; }
        Value& SetInteger(int64_t val)                 { Reset(); m_Type = ValueType::Integer;                 m_AsInteger = val;            return *this; }
        Value& SetDouble(double val)                   { Reset(); m_Type = ValueType::Double;                  m_AsDouble  = val;            return *this; }
        Value& SetFunctionReference(int64_t val)       { Reset(); m_Type = ValueType::FunctionReference;       m_AsInteger = val;            return *this; }
        Value& SetNativeFunctionReference(int64_t val) { Reset(); m_Type = ValueType::NativeFunctionReference; m_AsInteger = val;            return *this; }
        Value& SetList(ValueList&& val)                { Reset(); m_Type = ValueType::List;                    m_AsList    = std::move(val); return *this; }
        Value& SetList(const ValueList& val)           { Reset(); m_Type = ValueType::List;                    m_AsList    = val;            return *this; }
        Value& SetString(String&& val)                 { Reset(); m_Type = ValueType::String;                  m_AsString  = std::move(val); return *this; }
        Value& SetString(const String& val)            { Reset(); m_Type = ValueType::String;                  m_AsString  = val;            return *this; }
        Value& SetCustom(const CustomData& val)        { Reset(); m_Type = ValueType::Custom;                  m_AsCustom  = val;            return *this; }

    public:
        struct ToReprOptions
        {
            // If set, names of types are queried from it.
            const Pulsar::Module* Module = nullptr;
            size_t MaxDepth = 16;
            size_t FloatPrecision = 6;
        };

        static inline const ToReprOptions ToReprOptions_Default{nullptr,16,6};

        // Unlike ::ToRepr(), if Value is a String, the String is returned un-quoted.
        String ToString(ToReprOptions options=ToReprOptions_Default) const;
        String ToRepr(ToReprOptions options=ToReprOptions_Default) const;

    private:
        void Reset();

    private:
        ValueType m_Type;
        union
        {
            int64_t m_AsInteger;
            double m_AsDouble;
            ValueList m_AsList;
            String m_AsString;
            CustomData m_AsCustom;
        };
    };
}

#endif // _PULSAR_RUNTIME_VALUE_H
