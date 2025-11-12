#ifndef _PULSAR_RUNTIME_STATE_H
#define _PULSAR_RUNTIME_STATE_H

#include "pulsar/core.h"

namespace Pulsar
{
    enum class RuntimeState
    {
        OK = 0,
        Error = 1,
        TypeError,
        StackOverflow,
        StackUnderflow,
        OutOfBoundsConstantIndex,
        OutOfBoundsLocalIndex,
        OutOfBoundsGlobalIndex,
        WritingOnConstantGlobal,
        OutOfBoundsFunctionIndex,
        CallStackUnderflow,
        NativeFunctionBindingsMismatch,
        UnboundNativeFunction,
        FunctionNotFound,
        ListIndexOutOfBounds,
        StringIndexOutOfBounds,
        NoCustomTypeGlobalData,
        InvalidCustomTypeHandle,
        InvalidCustomTypeReference,
    };

    const char* RuntimeStateToString(RuntimeState rstate);
}

#endif // _PULSAR_RUNTIME_STATE_H
