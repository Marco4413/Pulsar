#define _CPULSAR_IMPLEMENTATION
#include "cpulsar/core.h"

#include "pulsar/core.h"

extern "C"
{

CPULSAR_API void* CPULSAR_CALL CPulsar_Malloc(size_t size)
{
    return Pulsar::Core::Malloc(size);
}

CPULSAR_API void* CPULSAR_CALL CPulsar_Realloc(void* block, size_t size)
{
    return Pulsar::Core::Realloc(block, size);
}

CPULSAR_API void CPULSAR_CALL CPulsar_Free(void* block)
{
    Pulsar::Core::Free(block);
}

}
