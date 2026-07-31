#define _CPULSAR_IMPLEMENTATION
#include "cpulsar/runtime/locals.h"
#include "cpulsar/_opaque.hpp"

#include "pulsar/runtime.h"

extern "C"
{

CPULSAR_API CPulsar_Value* CPULSAR_CALL CPulsar_Locals_Get(CPulsar_Locals* _self, size_t idx)
{
    return CPULSAR_WRAP(CPULSAR_UNWRAP(_self)[idx]);
}

CPULSAR_API void CPULSAR_CALL CPulsar_Locals_Set(CPulsar_Locals* _self, size_t idx, CPulsar_Value* _value)
{
    CPULSAR_UNWRAP(_self)[idx] = std::move(CPULSAR_UNWRAP(_value));
}

CPULSAR_API void CPULSAR_CALL CPulsar_Locals_SetCopy(CPulsar_Locals* _self, size_t idx, const CPulsar_Value* _value)
{
    CPULSAR_UNWRAP(_self)[idx] = CPULSAR_UNWRAP(_value);
}

}
