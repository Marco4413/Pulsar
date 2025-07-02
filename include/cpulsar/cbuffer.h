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

#define CPULSAR_CBUFFER_NULL CPULSAR_LITERAL_S(CPulsar_CBuffer, {0})

#endif // _CPULSAR_CBUFFER_H
