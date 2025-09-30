#ifndef _CPULSAR_CORE_H
#define _CPULSAR_CORE_H

#include <stddef.h> // size_t
#include <inttypes.h> // int64_t

#include <cpulsar/platform.h>

#ifdef __cplusplus
#  define CPULSAR_CPP
#endif // __cplusplus

// Derefs an opaque pointer, this is mainly useful to search where opaques are dereferenced.
#define CPULSAR_DEREF(upType, opaque) (*(upType*)opaque)
#define CPULSAR_REF(downType, value) ((downType*)&value)

#ifdef CPULSAR_CPP
#  define CPULSAR_LITERAL_S(structType, init) (structType init)
#else // CPULSAR_CPP
#  define CPULSAR_LITERAL_S(structType, init) ((structType)init)
#endif // CPULSAR_CPP

#if defined(CPULSAR_PLATFORM_WINDOWS)
#  define CPULSAR_CALL __cdecl
#  define CPULSAR_EXPORT __declspec(dllexport)
#  define CPULSAR_IMPORT __declspec(dllimport)
#else // CPULSAR_PLATFORM_*
#  define CPULSAR_CALL
#  define CPULSAR_EXPORT
#  define CPULSAR_IMPORT
#endif

#ifdef CPULSAR_SHAREDLIB
#  ifdef _CPULSAR_IMPLEMENTATION
#    define CPULSAR_API CPULSAR_EXPORT
#  else // _CPULSAR_IMPLEMENTATION
#    define CPULSAR_API CPULSAR_IMPORT
#  endif // _CPULSAR_IMPLEMENTATION
#else // CPULSAR_SHAREDLIB
#  define CPULSAR_API
#endif // CPULSAR_SHAREDLIB

#ifdef CPULSAR_CPP
extern "C" {
#endif

CPULSAR_API void* CPULSAR_CALL CPulsar_Malloc(size_t size);
CPULSAR_API void* CPULSAR_CALL CPulsar_Realloc(void* block, size_t size);
CPULSAR_API void  CPULSAR_CALL CPulsar_Free(void* block);

#ifdef CPULSAR_CPP
}
#endif

#endif // _CPULSAR_CORE_H
