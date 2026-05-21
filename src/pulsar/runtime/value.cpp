#include "pulsar/runtime/value.h"

#include "pulsar/lexer/utils.h"
#include "pulsar/runtime/module.h"

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
        const List::Node* aNext = AsList().Front();
        const List::Node* bNext = other.AsList().Front();
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

Pulsar::String Pulsar::Value::ToString(ToReprOptions options) const
{
    if (Type() == ValueType::String)
        return AsString();
    return ToRepr(options);
}

Pulsar::String Pulsar::Value::ToRepr(ToReprOptions options) const
{
    switch (Type()) {
    case ValueType::Void:
        return "(!Void)";
    case ValueType::Integer:
        return IntToString(AsInteger());
    case ValueType::Double:
        return DoubleToString(AsDouble(), options.FloatPrecision);
    case ValueType::FunctionReference: {
        String s = "&(";
        int64_t functionIdx = AsInteger();
        if (options.Module && functionIdx >= 0 && static_cast<size_t>(functionIdx) < options.Module->Functions.Size()) {
            const String& functionName = options.Module->Functions[static_cast<size_t>(functionIdx)].Name;
            s += IsIdentifier(functionName) ? functionName : ToStringLiteral(functionName);
        }

        s += '@';
        s += IntToString(AsInteger());
        s += ')';
        return s;
    }
    case ValueType::NativeFunctionReference: {
        String s = "&(*";
        int64_t nativeIdx = AsInteger();
        if (options.Module && nativeIdx >= 0 && static_cast<size_t>(nativeIdx) < options.Module->NativeBindings.Size()) {
            const String& nativeName = options.Module->NativeBindings[static_cast<size_t>(nativeIdx)].Name;
            s += IsIdentifier(nativeName) ? nativeName : ToStringLiteral(nativeName);
        }

        s += '@';
        s += IntToString(AsInteger());
        s += ')';
        return s;
    }
    case ValueType::List: {
        // TODO: Non-recursive
        auto node = AsList().Front();
        if (!node) return "[ ]";
        else if (options.MaxDepth <= 0) return "[ ... ]";

        ToReprOptions recOptions = options;
        --recOptions.MaxDepth;

        String result = "[ ";
        result += node->Value().ToRepr(recOptions);
        while (node->HasNext()) {
            node = node->Next();
            result += ", ";
            result += node->Value().ToRepr(recOptions);
        }
        result += " ]";

        return result;
    }
    case ValueType::String:
        return ToStringLiteral(AsString());
    case ValueType::Custom: {
        String s = "(!Custom/";
        if (options.Module && options.Module->HasCustomType(AsCustom().Type)) {
            const String& customTypeName = options.Module->GetCustomType(AsCustom().Type).Name;
            s += IsIdentifier(customTypeName) ? customTypeName : ToStringLiteral(customTypeName);
        }

        s += '#';
        s += UIntToString(AsCustom().Type);
        s += " 0x";
        PutHexString(s,
                reinterpret_cast<uint64_t>(AsCustom().Data.Get()),
                sizeof(uintptr_t)*2,
                true);
        s += ')';
        return s;
    }
    }
    return "(!Unknown)";
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
        PULSAR_MEMSET((void*)&m_AsList, 0, sizeof(List));
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
