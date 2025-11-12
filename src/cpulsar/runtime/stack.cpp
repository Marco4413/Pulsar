#define _CPULSAR_IMPLEMENTATION
#include "cpulsar/runtime/stack.h"

#include "pulsar/runtime.h"

using Value = Pulsar::Value;
using Stack = Pulsar::Stack;

extern "C"
{

CPULSAR_API CPulsar_Value CPULSAR_CALL CPulsar_Stack_PushEmpty(CPulsar_Stack _self)
{
    return CPulsar_Stack_Emplace(_self);
}

CPULSAR_API CPulsar_Value CPULSAR_CALL CPulsar_Stack_Pop(CPulsar_Stack _self)
{
    Stack& self = CPULSAR_DEREF(Stack, _self);
    CPulsar_Value value = CPULSAR_REF(CPulsar_Value_S, *PULSAR_NEW(Value, self.Pop()));
    return value;
}

CPULSAR_API CPulsar_Value CPULSAR_CALL CPulsar_Stack_Emplace(CPulsar_Stack _self)
{
    return CPULSAR_REF(CPulsar_Value_S, CPULSAR_DEREF(Stack, _self).Emplace());
}

CPULSAR_API void CPULSAR_CALL CPulsar_Stack_Push(CPulsar_Stack _self, CPulsar_Value _value)
{
    CPULSAR_DEREF(Stack, _self).Push(std::move(CPULSAR_DEREF(Value, _value)));
}

CPULSAR_API void CPULSAR_CALL CPulsar_Stack_PushCopy(CPulsar_Stack _self, CPulsar_Value _value)
{
    CPULSAR_DEREF(Stack, _self).Push(CPULSAR_DEREF(Value, _value));
}

}
