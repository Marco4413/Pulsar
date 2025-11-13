#ifndef _CPULSAR_RUNTIME_VALUE_H
#define _CPULSAR_RUNTIME_VALUE_H

#include "cpulsar/core.h"
#include "cpulsar/runtime/customdata.h"

#ifdef CPULSAR_CPP
extern "C" {
#endif

CPULSAR_API CPulsar_Value* CPULSAR_CALL CPulsar_Value_Create(void);
CPULSAR_API void           CPULSAR_CALL CPulsar_Value_Delete(CPulsar_Value* self);

CPULSAR_API int     CPULSAR_CALL CPulsar_Value_IsInteger(const CPulsar_Value* self);
CPULSAR_API int64_t CPULSAR_CALL CPulsar_Value_AsInteger(const CPulsar_Value* self);
CPULSAR_API void    CPULSAR_CALL CPulsar_Value_SetInteger(CPulsar_Value* self, int64_t value);

CPULSAR_API int    CPULSAR_CALL CPulsar_Value_IsDouble(const CPulsar_Value* self);
CPULSAR_API double CPULSAR_CALL CPulsar_Value_AsDouble(const CPulsar_Value* self);
CPULSAR_API void   CPULSAR_CALL CPulsar_Value_SetDouble(CPulsar_Value* self, double value);

// Methods that check if Value is a numeric and auto-casts to either integer or double
CPULSAR_API int     CPULSAR_CALL CPulsar_Value_IsNumber(const CPulsar_Value* self);
CPULSAR_API int64_t CPULSAR_CALL CPulsar_Value_AsIntegerNumber(const CPulsar_Value* self);
CPULSAR_API double  CPULSAR_CALL CPulsar_Value_AsDoubleNumber(const CPulsar_Value* self);

// TODO: Maybe add a CPulsar_String type
CPULSAR_API int         CPULSAR_CALL CPulsar_Value_IsString(const CPulsar_Value* self);
CPULSAR_API const char* CPULSAR_CALL CPulsar_Value_AsString(const CPulsar_Value* self);
CPULSAR_API void        CPULSAR_CALL CPulsar_Value_SetString(CPulsar_Value* self, const char* value);

CPULSAR_API int                CPULSAR_CALL CPulsar_Value_IsList(const CPulsar_Value* self);
CPULSAR_API CPulsar_ValueList* CPULSAR_CALL CPulsar_Value_AsList(CPulsar_Value* self);
CPULSAR_API CPulsar_ValueList* CPULSAR_CALL CPulsar_Value_SetEmptyList(CPulsar_Value* self);

CPULSAR_API int                 CPULSAR_CALL CPulsar_Value_IsCustom(const CPulsar_Value* self);
CPULSAR_API CPulsar_CustomData* CPULSAR_CALL CPulsar_Value_AsCustom(CPulsar_Value* self);
// Returns NULL if `typeId` does not match the type of this value or the stored type is not a CBuffer.
CPULSAR_API CPulsar_CBuffer* CPULSAR_CALL CPulsar_Value_AsCustomBuffer(CPulsar_Value* self, uint64_t typeId);
// Copies the reference from `data` so make sure to delete the instance that was passed.
CPULSAR_API CPulsar_CustomData* CPULSAR_CALL CPulsar_Value_SetCustom(CPulsar_Value* self, CPulsar_CustomData* data);
// Takes ownership of `buffer`.
// Returns a reference to the buffer inside Value.
CPULSAR_API CPulsar_CBuffer* CPULSAR_CALL CPulsar_Value_SetCustomBuffer(CPulsar_Value* self, uint64_t typeId, CPulsar_CBuffer buffer);

CPULSAR_API CPulsar_ValueList* CPULSAR_CALL CPulsar_ValueList_Create(void);
CPULSAR_API void              CPULSAR_CALL CPulsar_ValueList_Delete(CPulsar_ValueList* self);

CPULSAR_API CPulsar_Value* CPULSAR_CALL CPulsar_ValueList_Pop(CPulsar_ValueList* self);
// Pushes a new empty value owned by `self` and returns its reference.
CPULSAR_API CPulsar_Value* CPULSAR_CALL CPulsar_ValueList_PushEmpty(CPulsar_ValueList* self);
// Moves `value` into the list. `value` must be deleted by its owner
CPULSAR_API void CPULSAR_CALL CPulsar_ValueList_Push(CPulsar_ValueList* self, CPulsar_Value* value);
// Copies `value` into the list. `value` must be deleted by its owner
CPULSAR_API void CPULSAR_CALL CPulsar_ValueList_PushCopy(CPulsar_ValueList* self, const CPulsar_Value* value);

#ifdef CPULSAR_CPP
}
#endif

#endif // _CPULSAR_RUNTIME_VALUE_H
