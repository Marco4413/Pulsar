#define _CPULSAR_IMPLEMENTATION
#include "cpulsar/runtime/value.h"
#include "cpulsar/_opaque.hpp"

#include "pulsar/runtime.h"

extern "C"
{

CPULSAR_API CPulsar_Value* CPULSAR_CALL CPulsar_Value_Create(void)
{
    return CPULSAR_WRAP(*PULSAR_NEW(Pulsar::Value));
}

CPULSAR_API void CPULSAR_CALL CPulsar_Value_Delete(CPulsar_Value* _self)
{
    PULSAR_DELETE(Pulsar::Value, &CPULSAR_UNWRAP(_self));
}

CPULSAR_API int CPULSAR_CALL CPulsar_Value_IsInteger(const CPulsar_Value* _self)
{
    return CPULSAR_UNWRAP(_self).Type() == Pulsar::ValueType::Integer;
}

CPULSAR_API int64_t CPULSAR_CALL CPulsar_Value_AsInteger(const CPulsar_Value* _self)
{
    return CPULSAR_UNWRAP(_self).AsInteger();
}

CPULSAR_API void CPULSAR_CALL CPulsar_Value_SetInteger(CPulsar_Value* _self, int64_t value)
{
    CPULSAR_UNWRAP(_self).SetInteger(value);
}

CPULSAR_API int CPULSAR_CALL CPulsar_Value_IsDouble(const CPulsar_Value* _self)
{
    return CPULSAR_UNWRAP(_self).Type() == Pulsar::ValueType::Double;
}

CPULSAR_API double CPULSAR_CALL CPulsar_Value_AsDouble(const CPulsar_Value* _self)
{
    return CPULSAR_UNWRAP(_self).AsDouble();
}

CPULSAR_API void CPULSAR_CALL CPulsar_Value_SetDouble(CPulsar_Value* _self, double value)
{
    CPULSAR_UNWRAP(_self).SetDouble(value);
}

CPULSAR_API int CPULSAR_CALL CPulsar_Value_IsNumber(const CPulsar_Value* self)
{
    return CPulsar_Value_IsInteger(self) || CPulsar_Value_IsDouble(self);
}

CPULSAR_API int64_t CPULSAR_CALL CPulsar_Value_AsIntegerNumber(const CPulsar_Value* self)
{
    return CPulsar_Value_IsInteger(self)
        ? CPulsar_Value_AsInteger(self)
        : (int64_t)CPulsar_Value_AsDouble(self);
}

CPULSAR_API double CPULSAR_CALL CPulsar_Value_AsDoubleNumber(const CPulsar_Value* self)
{
    return CPulsar_Value_IsDouble(self)
        ? CPulsar_Value_AsDouble(self)
        : (double)CPulsar_Value_AsInteger(self);
}

CPULSAR_API int CPULSAR_CALL CPulsar_Value_IsString(const CPulsar_Value* _self)
{
    return CPULSAR_UNWRAP(_self).Type() == Pulsar::ValueType::String;
}

CPULSAR_API const char* CPULSAR_CALL CPulsar_Value_AsString(const CPulsar_Value* _self)
{
    return CPULSAR_UNWRAP(_self).AsString().CString();
}

CPULSAR_API void CPULSAR_CALL CPulsar_Value_SetString(CPulsar_Value* _self, const char* value)
{
    CPULSAR_UNWRAP(_self).SetString(value);
}

CPULSAR_API int CPULSAR_CALL CPulsar_Value_IsList(const CPulsar_Value* _self)
{
    return CPULSAR_UNWRAP(_self).Type() == Pulsar::ValueType::List;
}

CPULSAR_API CPulsar_ValueList* CPULSAR_CALL CPulsar_Value_AsList(CPulsar_Value* _self)
{
    return CPULSAR_WRAP(CPULSAR_UNWRAP(_self).AsList());
}

CPULSAR_API CPulsar_ValueList* CPULSAR_CALL CPulsar_Value_SetEmptyList(CPulsar_Value* _self)
{
    CPULSAR_UNWRAP(_self).SetList(Pulsar::Value::List());
    return CPulsar_Value_AsList(_self);
}

CPULSAR_API int CPULSAR_CALL CPulsar_Value_IsCustom(const CPulsar_Value* _self)
{
    return CPULSAR_UNWRAP(_self).Type() == Pulsar::ValueType::Custom;
}

CPULSAR_API CPulsar_CustomData* CPULSAR_CALL CPulsar_Value_AsCustom(CPulsar_Value* _self)
{
    return CPULSAR_WRAP(CPULSAR_UNWRAP(_self).AsCustom());
}

CPULSAR_API CPulsar_CBuffer* CPULSAR_CALL CPulsar_Value_AsCustomBuffer(CPulsar_Value* self, uint64_t typeId)
{
    CPulsar_CustomData* data = CPulsar_Value_AsCustom(self);
    if (CPulsar_CustomData_GetType(data) != typeId) return NULL;
    CPulsar_CustomDataHolder_Ref* dataHolder = CPulsar_CustomData_GetData(data);
    return CPulsar_CustomDataHolder_Ref_GetBuffer(dataHolder);
}

CPULSAR_API CPulsar_CustomData* CPULSAR_CALL CPulsar_Value_SetCustom(CPulsar_Value* _self, CPulsar_CustomData* _data)
{
    CPULSAR_UNWRAP(_self).SetCustom(CPULSAR_UNWRAP(_data));
    return _data;
}

CPULSAR_API CPulsar_CBuffer* CPULSAR_CALL CPulsar_Value_SetCustomBuffer(CPulsar_Value* self, uint64_t typeId, CPulsar_CBuffer buffer)
{
    CPulsar_CustomDataHolder_Ref* dataHolder = CPulsar_CustomDataHolder_Ref_FromBuffer(buffer);
    CPulsar_CustomData* data = CPulsar_CustomData_Create(typeId, dataHolder);

    CPulsar_Value_SetCustom(self, data);

    CPulsar_CustomData_Delete(data);
    CPulsar_CustomDataHolder_Ref_Delete(dataHolder);

    return CPulsar_Value_AsCustomBuffer(self, typeId);
}

CPULSAR_API CPulsar_ValueList* CPULSAR_CALL CPulsar_ValueList_Create(void)
{
    return CPULSAR_WRAP(*PULSAR_NEW(Pulsar::Value::List));
}

CPULSAR_API void CPULSAR_CALL CPulsar_ValueList_Delete(CPulsar_ValueList* _self)
{
    PULSAR_DELETE(Pulsar::Value::List, &CPULSAR_UNWRAP(_self));
}

CPULSAR_API CPulsar_Value* CPULSAR_CALL CPulsar_ValueList_Pop(CPulsar_ValueList* _self)
{
    Pulsar::Value::List& self = CPULSAR_UNWRAP(_self);
    CPulsar_Value* value = CPULSAR_WRAP(*PULSAR_NEW(
        Pulsar::Value, std::move(self.Back()->Value())
    ));
    self.RemoveBack(1);
    return value;
}

CPULSAR_API CPulsar_Value* CPULSAR_CALL CPulsar_ValueList_PushEmpty(CPulsar_ValueList* _self)
{
    Pulsar::Value::List::Node* node = CPULSAR_UNWRAP(_self).Append();
    return CPULSAR_WRAP(node->Value());
}

CPULSAR_API void CPULSAR_CALL CPulsar_ValueList_Push(CPulsar_ValueList* _self, CPulsar_Value* _value)
{
    CPULSAR_UNWRAP(_self).Append(std::move(CPULSAR_UNWRAP(_value)));
}

CPULSAR_API void CPULSAR_CALL CPulsar_ValueList_PushCopy(CPulsar_ValueList* _self, const CPulsar_Value* _value)
{
    CPULSAR_UNWRAP(_self).Append(CPULSAR_UNWRAP(_value));
}

}
