#define _CPULSAR_IMPLEMENTATION
#include "cpulsar/runtime/customdata.h"

#include "pulsar/runtime.h"

extern "C"
{

CPULSAR_API CPulsar_CustomData CPULSAR_CALL CPulsar_CustomData_Create(uint64_t typeId, CPulsar_CustomDataHolder_Ref _typeData)
{
    return CPULSAR_REF(CPulsar_CustomData_S, *PULSAR_NEW(
        Pulsar::CustomData, typeId, CPULSAR_DEREF(Pulsar::CustomDataHolder::Ref, _typeData)
    ));
}

CPULSAR_API void CPULSAR_CALL CPulsar_CustomData_Delete(CPulsar_CustomData _self)
{
    PULSAR_DELETE(Pulsar::CustomData, &CPULSAR_DEREF(Pulsar::CustomData, _self));
}

CPULSAR_API uint64_t CPULSAR_CALL CPulsar_CustomData_GetType(CPulsar_CustomData _self)
{
    return CPULSAR_DEREF(Pulsar::CustomData, _self).Type;
}

CPULSAR_API CPulsar_CustomDataHolder_Ref CPULSAR_CALL CPulsar_CustomData_GetData(CPulsar_CustomData _self)
{
    return CPULSAR_REF(CPulsar_CustomDataHolder_Ref_S, CPULSAR_DEREF(Pulsar::CustomData, _self).Data);
}

}
