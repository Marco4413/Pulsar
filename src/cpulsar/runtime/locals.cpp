#define _CPULSAR_IMPLEMENTATION
#include "cpulsar/runtime/locals.h"

#include "pulsar/runtime.h"

using Value = Pulsar::Value;
using Locals = Pulsar::List<Value>;

extern "C"
{

CPULSAR_API CPulsar_Value CPulsar_Locals_Get(CPulsar_Locals _self, size_t idx)
{
    return CPULSAR_REF(CPulsar_Value_S, CPULSAR_DEREF(Locals, _self)[idx]);
}

CPULSAR_API void CPulsar_Locals_Set(CPulsar_Locals _self, size_t idx, CPulsar_Value _value)
{
    CPULSAR_DEREF(Locals, _self)[idx] = std::move(CPULSAR_DEREF(Value, _value));
}

CPULSAR_API void CPulsar_Locals_SetCopy(CPulsar_Locals _self, size_t idx, const CPulsar_Value _value)
{
    CPULSAR_DEREF(Locals, _self)[idx] = CPULSAR_DEREF(const Value, _value);
}

}
