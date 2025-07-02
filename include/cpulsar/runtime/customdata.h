#ifndef _CPULSAR_RUNTIME_CUSTOMDATA_H
#define _CPULSAR_RUNTIME_CUSTOMDATA_H

#include "cpulsar/core.h"

#include "cpulsar/runtime/customtype.h"

typedef struct CPulsar_CustomData_S* CPulsar_CustomData;

#ifdef CPULSAR_CPP
extern "C" {
#endif

CPULSAR_API CPulsar_CustomData CPULSAR_CALL CPulsar_CustomData_Create(uint64_t typeId, CPulsar_CustomDataHolder_Ref typeData);
CPULSAR_API void               CPULSAR_CALL CPulsar_CustomData_Delete(CPulsar_CustomData self);

CPULSAR_API uint64_t                     CPULSAR_CALL CPulsar_CustomData_GetType(CPulsar_CustomData self);
CPULSAR_API CPulsar_CustomDataHolder_Ref CPULSAR_CALL CPulsar_CustomData_GetData(CPulsar_CustomData self);

#ifdef CPULSAR_CPP
}
#endif

#endif // _CPULSAR_RUNTIME_CUSTOMDATA_H
