#ifndef _CPULSAR_CBUFFER_H
#define _CPULSAR_CBUFFER_H

#include "cpulsar/core.h"

typedef void(CPULSAR_CALL *CPulsar_CBuffer_Free)(void*);
typedef void*(CPULSAR_CALL *CPulsar_CBuffer_Copy)(void*);

// Represents a generic portion of memory
typedef struct {
    // Data stored by the buffer
    void* Data;
    // This function will be called to free `Data`
    // It should free both the Data pointer and its contents.
    // If NULL, nothing will be done to `Data` so make sure that no memory was allocated.
    CPulsar_CBuffer_Free Free;
    // This function will be called to copy `Data`
    // If NULL, copying `Data` is not supported.
    CPulsar_CBuffer_Copy Copy;
} CPulsar_CBuffer;

#define CPULSAR_CBUFFER_NULL (CPULSAR_LIT(CPulsar_CBuffer){NULL,NULL,NULL})

#ifdef CPULSAR_CPP
extern "C" {
#endif

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
