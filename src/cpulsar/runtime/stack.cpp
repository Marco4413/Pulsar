#define _CPULSAR_IMPLEMENTATION
#include "cpulsar/runtime/stack.h"
#include "cpulsar/_opaque.hpp"

#include "pulsar/runtime.h"

extern "C"
{

CPULSAR_API CPulsar_Value* CPULSAR_CALL CPulsar_Stack_PushEmpty(CPulsar_Stack* _self)
{
    return CPulsar_Stack_Emplace(_self);
}

CPULSAR_API CPulsar_Value* CPULSAR_CALL CPulsar_Stack_Pop(CPulsar_Stack* _self)
{
    Pulsar::Stack& self = CPULSAR_UNWRAP(_self);
    CPulsar_Value* value = CPULSAR_WRAP(*PULSAR_NEW(Pulsar::Value, self.Pop()));
    return value;
}

CPULSAR_API CPulsar_Value* CPULSAR_CALL CPulsar_Stack_Emplace(CPulsar_Stack* _self)
{
    return CPULSAR_WRAP(CPULSAR_UNWRAP(_self).Emplace());
}

CPULSAR_API void CPULSAR_CALL CPulsar_Stack_Push(CPulsar_Stack* _self, CPulsar_Value* _value)
{
    CPULSAR_UNWRAP(_self).Push(std::move(CPULSAR_UNWRAP(_value)));
}

CPULSAR_API void CPULSAR_CALL CPulsar_Stack_PushCopy(CPulsar_Stack* _self, const CPulsar_Value* _value)
{
    CPULSAR_UNWRAP(_self).Push(CPULSAR_UNWRAP(_value));
}

}
