#ifndef _CPULSAR_RUNTIME_VALUE_H
#define _CPULSAR_RUNTIME_VALUE_H

#include "cpulsar/core.h"

typedef void* CPulsar_Value;
typedef void* CPulsar_ValueList;

#ifdef CPULSAR_CPP
extern "C" {
#endif

CPULSAR_API CPulsar_Value CPulsar_Value_Create();
CPULSAR_API void CPulsar_Value_Delete(CPulsar_Value self);

// TODO: Maybe add a CPulsar_String type
CPULSAR_API int CPulsar_Value_IsString(const CPulsar_Value self);
CPULSAR_API const char* CPulsar_Value_AsString(const CPulsar_Value self);
CPULSAR_API void CPulsar_Value_SetString(CPulsar_Value self, const char* value);

CPULSAR_API int CPulsar_Value_IsList(const CPulsar_Value self);
CPULSAR_API CPulsar_ValueList CPulsar_Value_AsList(CPulsar_Value self);
CPULSAR_API CPulsar_ValueList CPulsar_Value_SetEmptyList(CPulsar_Value self);

CPULSAR_API CPulsar_ValueList CPulsar_ValueList_Create();
CPULSAR_API void CPulsar_ValueList_Delete(CPulsar_ValueList self);

CPULSAR_API CPulsar_Value CPulsar_ValueList_Pop(CPulsar_ValueList self);
// Moves `value` into the list. `value` must be deleted by its owner
CPULSAR_API void CPulsar_ValueList_Push(CPulsar_ValueList self, CPulsar_Value value);
// Copies `value` into the list. `value` must be deleted by its owner
CPULSAR_API void CPulsar_ValueList_PushCopy(CPulsar_ValueList self, CPulsar_Value value);

#ifdef CPULSAR_CPP
}
#endif

#endif // _CPULSAR_RUNTIME_VALUE_H
