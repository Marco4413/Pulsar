#ifndef _CPULSAR_CBUFFER_H
#define _CPULSAR_CBUFFER_H

#include "cpulsar/core.h"

typedef void(*CPulsar_CBuffer_Free)(void*);

// Represents a generic portion of memory
typedef struct {
    // Data stored by the buffer
    void* Data;
    // This function will be called to free `Data`
    // It should free both the Data pointer and its contents.
    // If NULL, nothing will be done to `Data` so make sure that no memory was allocated.
    CPulsar_CBuffer_Free Free;
} CPulsar_CBuffer;

#ifdef CPULSAR_CPP
  #define CPULSAR_CBUFFER_NULL (CPulsar_CBuffer{0})
#else // CPULSAR_CPP
  #define CPULSAR_CBUFFER_NULL ((CPulsar_CBuffer){0})
#endif // CPULSAR_CPP

#endif // _CPULSAR_CBUFFER_H
