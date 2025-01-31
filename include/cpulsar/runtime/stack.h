#ifndef _CPULSAR_RUNTIME_STACK_H
#define _CPULSAR_RUNTIME_STACK_H

#include "cpulsar/core.h"

#include "cpulsar/runtime/value.h"

typedef struct CPulsar_Stack_S* CPulsar_Stack;

#ifdef CPULSAR_CPP
extern "C" {
#endif

CPULSAR_API CPulsar_Value CPulsar_Stack_Pop(CPulsar_Stack self);
// Pushes a new empty value owned by `self` and returns its reference.
CPULSAR_API CPulsar_Value CPulsar_Stack_PushEmpty(CPulsar_Stack self);
// Moves `value` into the list. `value` must be deleted by its owner
CPULSAR_API void CPulsar_Stack_Push(CPulsar_Stack self, CPulsar_Value value);
// Copies `value` into the list. `value` must be deleted by its owner
CPULSAR_API void CPulsar_Stack_PushCopy(CPulsar_Stack self, CPulsar_Value value);

#ifdef CPULSAR_CPP
}
#endif

#endif // _CPULSAR_RUNTIME_STACK_H
