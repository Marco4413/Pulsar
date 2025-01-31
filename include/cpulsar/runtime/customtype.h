#ifndef _CPULSAR_RUNTIME_CUSTOMTYPE_H
#define _CPULSAR_RUNTIME_CUSTOMTYPE_H

#include "cpulsar/core.h"

#include "cpulsar/cbuffer.h"

typedef struct CPulsar_CustomTypeData_Ref_S* CPulsar_CustomTypeData_Ref;
typedef struct CPulsar_CustomDataHolder_Ref_S* CPulsar_CustomDataHolder_Ref;

typedef CPulsar_CustomTypeData_Ref(*CPulsar_CustomType_DataFactoryFn)(void);

#ifdef CPULSAR_CPP
extern "C" {
#endif

// TODO: Add methods to copy references

CPULSAR_API CPulsar_CustomTypeData_Ref CPulsar_CustomTypeData_Ref_FromBuffer(CPulsar_CBuffer buffer);
// Returns a reference of the underlying buffer if it was created with CPulsar.
// Otherwise, NULL is returned.
CPULSAR_API CPulsar_CBuffer* CPulsar_CustomTypeData_Ref_GetBuffer(CPulsar_CustomTypeData_Ref self);
CPULSAR_API void CPulsar_CustomTypeData_Ref_Delete(CPulsar_CustomTypeData_Ref self);

CPULSAR_API CPulsar_CustomDataHolder_Ref CPulsar_CustomDataHolder_Ref_FromBuffer(CPulsar_CBuffer buffer);
// Returns a reference of the underlying buffer if it was created with CPulsar.
// Otherwise, NULL is returned.
CPULSAR_API CPulsar_CBuffer* CPulsar_CustomDataHolder_Ref_GetBuffer(CPulsar_CustomDataHolder_Ref self);
CPULSAR_API void CPulsar_CustomDataHolder_Ref_Delete(CPulsar_CustomDataHolder_Ref self);

#ifdef CPULSAR_CPP
}
#endif

#endif // _CPULSAR_RUNTIME_CUSTOMTYPE_H
