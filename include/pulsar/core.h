#ifndef _PULSAR_CORE_H
#define _PULSAR_CORE_H

// All std includes from header files are here.
#include <cinttypes>
#include <cmath> // std::floor, std::ceil
#include <cstdlib> // std::malloc, ::free, ...
#include <cstring>
#include <functional>
#include <type_traits>
#include <utility>

#ifdef PULSAR_DEBUG
  #include <cstdio>
  #ifndef PULSAR_ASSERT
    #define PULSAR_ASSERT(cond, msg)                      \
        do {                                              \
            if (!(cond)) {                                \
                std::fprintf(stderr,                      \
                    "%s:%d: ASSERTION (%s) FAILED: %s\n", \
                    __FILE__, __LINE__, #cond, (msg));    \
                std::abort();                             \
            }                                             \
        } while (0)
  #endif // PULSAR_ASSERT
#else // PULSAR_DEBUG
  #ifndef PULSAR_ASSERT
    #define PULSAR_ASSERT(cond, msg) \
        do { (void)(cond); (void)(msg); } while (0)
  #endif // PULSAR_ASSERT
#endif // PULSAR_DEBUG

#ifndef PULSAR_NO_ATOMIC
#include <atomic>
#endif // PULSAR_NO_ATOMIC

#ifndef PULSAR_ATOMIC_SIZE_T
  #ifdef PULSAR_NO_ATOMIC
    #define PULSAR_ATOMIC_SIZE_T size_t
  #else // PULSAR_NO_ATOMIC
    #define PULSAR_ATOMIC_SIZE_T std::atomic_size_t
  #endif // PULSAR_NO_ATOMIC
#endif // PULSAR_ATOMIC_SIZE_T

#ifndef PULSAR_MALLOC
#define PULSAR_MALLOC(size) \
    std::malloc(size)
#endif // PULSAR_MALLOC

#ifndef PULSAR_REALLOC
#define PULSAR_REALLOC(ptr, size) \
    std::realloc(ptr, size)
#endif // PULSAR_REALLOC

#ifndef PULSAR_FREE
#define PULSAR_FREE(ptr) \
    std::free(ptr)
#endif // PULSAR_FREE

#ifndef PULSAR_MEMSET
#define PULSAR_MEMSET(dst, val, size) \
    std::memset(dst, val, size)
#endif // PULSAR_MEMSET

#ifndef PULSAR_MEMCPY
#define PULSAR_MEMCPY(dst, src, size) \
    std::memcpy(dst, src, size)
#endif // PULSAR_MEMCPY

#ifndef PULSAR_PLACEMENT_NEW
#define PULSAR_PLACEMENT_NEW(T, ptr, ...) \
    Pulsar::Core::PlacementNew<T>(ptr, __VA_ARGS__)
#endif // PULSAR_PLACEMENT_NEW

#ifndef PULSAR_NEW
#define PULSAR_NEW(T, ...) \
    Pulsar::Core::New<T>(__VA_ARGS__)
#endif // PULSAR_NEW

#ifndef PULSAR_DELETE
#define PULSAR_DELETE(T, ptr) \
    Pulsar::Core::Delete<T>(ptr)
#endif // PULSAR_DELETE

namespace Pulsar::Core
{
    // New and Delete that strictly use PULSAR_ macros to allocate memory
    template<typename T, typename ...Args>
    inline T* PlacementNew(T* ptr, Args&& ...args)
    {
        if constexpr (std::is_aggregate<T>()) {
            return new(ptr) T{std::forward<Args>(args)...};
        } else {
            return new(ptr) T(std::forward<Args>(args)...);
        }
    }

    template<typename T, typename ...Args>
    inline T* New(Args&& ...args)
    {
        return PULSAR_PLACEMENT_NEW(T,
            (T*)PULSAR_MALLOC(sizeof(T)),
            std::forward<Args>(args)...);
    }

    template<typename T>
    inline void Delete(T* ptr)
    {
        if (ptr) {
            ptr->~T();
            PULSAR_FREE((void*)ptr);
        }
    }
}

#endif // _PULSAR_CORE_H
