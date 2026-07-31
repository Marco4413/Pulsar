#include "pulsar/core.h"

void* Pulsar::Core::Malloc(size_t size)
{
    return std::malloc(size);
}

void* Pulsar::Core::Realloc(void* block, size_t newSize)
{
    return std::realloc(block, newSize);
}


void Pulsar::Core::Free(void* block)
{
    std::free(block);
}

