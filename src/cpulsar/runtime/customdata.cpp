#define _CPULSAR_IMPLEMENTATION
#include "cpulsar/runtime/customdata.h"
#include "cpulsar/_opaque.hpp"

#include "pulsar/runtime.h"

extern "C"
{

CPULSAR_API CPulsar_CustomData* CPULSAR_CALL CPulsar_CustomData_Create(uint64_t typeId, CPulsar_CustomDataHolder_Ref* _typeData)
{
    return CPULSAR_WRAP(*PULSAR_NEW(
        Pulsar::CustomData, typeId, CPULSAR_UNWRAP(_typeData)
    ));
}

CPULSAR_API void CPULSAR_CALL CPulsar_CustomData_Delete(CPulsar_CustomData* _self)
{
    PULSAR_DELETE(Pulsar::CustomData, &CPULSAR_UNWRAP(_self));
}

CPULSAR_API uint64_t CPULSAR_CALL CPulsar_CustomData_GetType(const CPulsar_CustomData* _self)
{
    return CPULSAR_UNWRAP(_self).Type;
}

CPULSAR_API CPulsar_CustomDataHolder_Ref* CPULSAR_CALL CPulsar_CustomData_GetData(CPulsar_CustomData* _self)
{
    return CPULSAR_WRAP(CPULSAR_UNWRAP(_self).Data);
}

}
