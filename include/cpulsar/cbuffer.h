#ifndef _CPULSAR_CBUFFER_H
#define _CPULSAR_CBUFFER_H

#include "cpulsar/core.h"

typedef void(CPULSAR_CALL *CPulsar_CBuffer_FreeFn)(void*);
typedef void*(CPULSAR_CALL *CPulsar_CBuffer_CopyFn)(void*);

// Represents a generic portion of memory
typedef struct {
    // Data stored by the buffer
    void* Data;
    // This function will be called to free `Data`
    // It should free both the Data pointer and its contents.
    // If NULL, nothing will be done to `Data` so make sure that no memory was allocated.
    CPulsar_CBuffer_FreeFn Free;
    // This function will be called to copy `Data`
    // If NULL, copying `Data` is not supported.
    CPulsar_CBuffer_CopyFn Copy;
} CPulsar_CBuffer;

#define CPULSAR_CBUFFER_NULL (CPULSAR_LIT(CPulsar_CBuffer){NULL,NULL,NULL})

#ifdef CPULSAR_CPP
extern "C" {
#endif

// `out` may be NULL, in which case this function only returns whether the operation is supported.
// Returns false if the operation is not supported.
// If false, `out` is unchanged.
// If true, `out` is freed and replaced by the copy.
CPULSAR_API bool            CPULSAR_CALL CPulsar_CBuffer_TryCopy(CPulsar_CBuffer self, CPulsar_CBuffer* out);
// Returns CPULSAR_CBUFFER_NULL if the operation is not supported.
CPULSAR_API CPulsar_CBuffer CPULSAR_CALL CPulsar_CBuffer_Copy(CPulsar_CBuffer self);
CPULSAR_API void            CPULSAR_CALL CPulsar_CBuffer_Free(CPulsar_CBuffer* self);

// Takes ownership of buffer.
CPULSAR_API CPulsar_CBuffer_Ref* CPULSAR_CALL CPulsar_CBuffer_Ref_Create(CPulsar_CBuffer buffer);
CPULSAR_API void                 CPULSAR_CALL CPulsar_CBuffer_Ref_Delete(CPulsar_CBuffer_Ref* self);
// Copies the Ref.
CPULSAR_API CPulsar_CBuffer_Ref* CPULSAR_CALL CPulsar_CBuffer_Ref_Copy(CPulsar_CBuffer_Ref* self);
CPULSAR_API CPulsar_CBuffer*     CPULSAR_CALL CPulsar_CBuffer_Ref_GetBuffer(CPulsar_CBuffer_Ref* self);

#ifdef CPULSAR_CPP
}
#endif

#endif // _CPULSAR_CBUFFER_H
