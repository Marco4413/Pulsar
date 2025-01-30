#define _CPULSAR_IMPLEMENTATION
#include "cpulsar/core.h"

#include "pulsar/core.h"

extern "C"
{

CPULSAR_API void* CPulsar_Malloc(size_t size)
{
    return Pulsar::Core::Malloc(size);
}

CPULSAR_API void* CPulsar_Realloc(void* block, size_t size)
{
    return Pulsar::Core::Realloc(block, size);
}

CPULSAR_API void CPulsar_Free(void* block)
{
    Pulsar::Core::Free(block);
}

}
