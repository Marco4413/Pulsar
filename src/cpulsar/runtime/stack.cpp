#define _CPULSAR_IMPLEMENTATION
#include "cpulsar/runtime/stack.h"

#include "pulsar/runtime.h"

using Value = Pulsar::Value;
using Stack = Pulsar::ValueStack;

extern "C"
{

CPULSAR_API CPulsar_Value CPulsar_Stack_Pop(CPulsar_Stack _self)
{
    Stack& self = CPULSAR_DEREF(Stack, _self);
    CPulsar_Value value = PULSAR_NEW(Value, std::move(self.Back()));
    self.PopBack();
    return value;
}

CPULSAR_API void CPulsar_Stack_Push(CPulsar_Stack _self, CPulsar_Value _value)
{
    CPULSAR_DEREF(Stack, _self).EmplaceBack(std::move(CPULSAR_DEREF(Value, _value)));
}

CPULSAR_API void CPulsar_Stack_PushCopy(CPulsar_Stack _self, CPulsar_Value _value)
{
    CPULSAR_DEREF(Stack, _self).EmplaceBack(CPULSAR_DEREF(Value, _value));
}

}
