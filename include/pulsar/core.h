#ifndef _PULSAR_CORE_H
#define _PULSAR_CORE_H

// All std includes from header files are here.
#include <atomic>
#include <cinttypes>
#include <cstring>
#include <functional>
#include <memory>
#include <initializer_list>

#ifndef PULSAR_MALLOC
#define PULSAR_MALLOC std::malloc
#endif // PULSAR_MALLOC

#ifndef PULSAR_REALLOC
#define PULSAR_REALLOC std::realloc
#endif // PULSAR_REALLOC

#ifndef PULSAR_FREE
#define PULSAR_FREE std::free
#endif // PULSAR_FREE

#ifndef PULSAR_MEMSET
#define PULSAR_MEMSET std::memset
#endif // PULSAR_MEMSET

#ifndef PULSAR_MEMCPY
#define PULSAR_MEMCPY std::memcpy
#endif // PULSAR_MEMCPY

namespace Pulsar::Core
{
    // New and Delete that strictly use PULSAR_ macros to allocate memory
    template<typename T, typename ...Args>
    inline T* New(Args&& ...args)
    {
        T* ptr = (T*)PULSAR_MALLOC(sizeof(T));
        return new(ptr) T(args...);
    }

    template<typename T>
    inline void Delete(T* ptr)
    {
        if (ptr) {
            ptr->~T();
            PULSAR_FREE(ptr);
        }
    }
}

#ifndef PULSAR_NEW
#define PULSAR_NEW(T, ...) Pulsar::Core::New<T>(__VA_ARGS__)
#endif // PULSAR_NEW

#ifndef PULSAR_DELETE
#define PULSAR_DELETE(T, ptr) Pulsar::Core::Delete<T>(ptr)
#endif // PULSAR_DELETE

#endif // _PULSAR_CORE_H
