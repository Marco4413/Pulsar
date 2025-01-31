#ifndef _CPULSAR_CORE_H
#define _CPULSAR_CORE_H

#include "stddef.h" // size_t
#include "inttypes.h" // int64_t

#ifdef __cplusplus
  #define CPULSAR_CPP
#endif // __cplusplus

// Derefs an opaque pointer, this is mainly useful to search where opaques are dereferenced.
#define CPULSAR_DEREF(upType, opaque) (*(upType*)opaque)
#define CPULSAR_REF(downType, value) ((downType*)&value)

#ifdef CPULSAR_CPP
  #define CPULSAR_LITERAL_S(structType, init) \
      (structType init)
#else // CPULSAR_CPP
  #define CPULSAR_LITERAL_S(structType, init) \
      ((structType)init)
#endif // CPULSAR_CPP

#ifdef CPULSAR_WINDOWS

  #define CPULSAR_EXPORT __declspec(dllexport)
  #define CPULSAR_IMPORT __declspec(dllimport)

#else // defined(CPULSAR_UNIX)

  #define CPULSAR_EXPORT
  #define CPULSAR_IMPORT

#endif

#ifdef CPULSAR_SHAREDLIB

  #ifdef _CPULSAR_IMPLEMENTATION
    #define CPULSAR_API CPULSAR_EXPORT
  #else // _CPULSAR_IMPLEMENTATION
    #define CPULSAR_API CPULSAR_IMPORT
  #endif // _CPULSAR_IMPLEMENTATION

#else // CPULSAR_SHAREDLIB

  #define CPULSAR_API

#endif // CPULSAR_SHAREDLIB

#ifdef CPULSAR_CPP
extern "C" {
#endif

CPULSAR_API void* CPulsar_Malloc(size_t size);
CPULSAR_API void* CPulsar_Realloc(void* block, size_t size);
CPULSAR_API void CPulsar_Free(void* block);

#ifdef CPULSAR_CPP
}
#endif

#endif // _CPULSAR_CORE_H
