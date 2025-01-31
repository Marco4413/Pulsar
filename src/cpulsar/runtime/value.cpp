#define _CPULSAR_IMPLEMENTATION
#include "cpulsar/runtime/value.h"

#include "pulsar/runtime.h"

using Value = Pulsar::Value;
using ValueList = Pulsar::ValueList;

extern "C"
{

CPULSAR_API CPulsar_Value CPulsar_Value_Create(void)
{
    return CPULSAR_REF(CPulsar_Value_S, *PULSAR_NEW(Value));
}

CPULSAR_API void CPulsar_Value_Delete(CPulsar_Value _self)
{
    PULSAR_DELETE(Value, &CPULSAR_DEREF(Value, _self));
}

CPULSAR_API int CPulsar_Value_IsInteger(const CPulsar_Value _self)
{
    return CPULSAR_DEREF(const Value, _self).Type() == Pulsar::ValueType::Integer;
}

CPULSAR_API int64_t CPulsar_Value_AsInteger(const CPulsar_Value _self)
{
    return CPULSAR_DEREF(const Value, _self).AsInteger();
}

CPULSAR_API void CPulsar_Value_SetInteger(CPulsar_Value _self, int64_t value)
{
    CPULSAR_DEREF(Value, _self).SetInteger(value);
}

CPULSAR_API int CPulsar_Value_IsString(const CPulsar_Value _self)
{
    return CPULSAR_DEREF(const Value, _self).Type() == Pulsar::ValueType::String;
}

CPULSAR_API const char* CPulsar_Value_AsString(const CPulsar_Value _self)
{
    return CPULSAR_DEREF(const Value, _self).AsString().CString();
}

CPULSAR_API void CPulsar_Value_SetString(CPulsar_Value _self, const char* value)
{
    CPULSAR_DEREF(Value, _self).SetString(value);
}

CPULSAR_API int CPulsar_Value_IsList(const CPulsar_Value _self)
{
    return CPULSAR_DEREF(const Value, _self).Type() == Pulsar::ValueType::List;
}

CPULSAR_API CPulsar_ValueList CPulsar_Value_AsList(CPulsar_Value _self)
{
    return CPULSAR_REF(CPulsar_ValueList_S, CPULSAR_DEREF(Value, _self).AsList());
}

CPULSAR_API CPulsar_ValueList CPulsar_Value_SetEmptyList(CPulsar_Value _self)
{
    CPULSAR_DEREF(Value, _self).SetList(ValueList());
    return CPulsar_Value_AsList(_self);
}

CPULSAR_API int CPulsar_Value_IsCustom(const CPulsar_Value _self)
{
    return CPULSAR_DEREF(const Value, _self).Type() == Pulsar::ValueType::Custom;
}

CPULSAR_API CPulsar_CustomData CPulsar_Value_AsCustom(CPulsar_Value _self)
{
    return CPULSAR_REF(CPulsar_CustomData_S, CPULSAR_DEREF(Value, _self).AsCustom());
}

CPULSAR_API CPulsar_CBuffer* CPulsar_Value_AsCustomBuffer(CPulsar_Value self, uint64_t typeId)
{
    CPulsar_CustomData data = CPulsar_Value_AsCustom(self);
    if (CPulsar_CustomData_GetType(data) != typeId) return NULL;
    CPulsar_CustomDataHolder_Ref dataHolder = CPulsar_CustomData_GetData(data);
    return CPulsar_CustomDataHolder_Ref_GetBuffer(dataHolder);
}

CPULSAR_API CPulsar_CustomData CPulsar_Value_SetCustom(CPulsar_Value _self, CPulsar_CustomData _data)
{
    CPULSAR_DEREF(Value, _self).SetCustom(CPULSAR_DEREF(Pulsar::CustomData, _data));
    return _data;
}

CPULSAR_API CPulsar_CBuffer* CPulsar_Value_SetCustomBuffer(CPulsar_Value self, uint64_t typeId, CPulsar_CBuffer buffer)
{
    CPulsar_CustomDataHolder_Ref dataHolder = CPulsar_CustomDataHolder_Ref_FromBuffer(buffer);
    CPulsar_CustomData data = CPulsar_CustomData_Create(typeId, dataHolder);

    CPulsar_Value_SetCustom(self, data);

    CPulsar_CustomData_Delete(data);
    CPulsar_CustomDataHolder_Ref_Delete(dataHolder);

    return CPulsar_Value_AsCustomBuffer(self, typeId);
}

CPULSAR_API CPulsar_ValueList CPulsar_ValueList_Create(void)
{
    return CPULSAR_REF(CPulsar_ValueList_S, *PULSAR_NEW(ValueList));
}

CPULSAR_API void CPulsar_ValueList_Delete(CPulsar_ValueList _self)
{
    PULSAR_DELETE(ValueList, &CPULSAR_DEREF(ValueList, _self));
}

CPULSAR_API CPulsar_Value CPulsar_ValueList_Pop(CPulsar_ValueList _self)
{
    ValueList& self = CPULSAR_DEREF(ValueList, _self);
    CPulsar_Value value = CPULSAR_REF(CPulsar_Value_S, *PULSAR_NEW(
        Value, std::move(self.Back()->Value())
    ));
    self.RemoveBack(1);
    return value;
}

CPULSAR_API void CPulsar_ValueList_Push(CPulsar_ValueList _self, CPulsar_Value _value)
{
    CPULSAR_DEREF(ValueList, _self).Append(std::move(CPULSAR_DEREF(Value, _value)));
}

CPULSAR_API void CPulsar_ValueList_PushCopy(CPulsar_ValueList _self, CPulsar_Value _value)
{
    CPULSAR_DEREF(ValueList, _self).Append(CPULSAR_DEREF(Value, _value));
}

}
