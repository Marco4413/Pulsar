#ifndef _PULSAR_RUNTIME_DEBUG_H
#define _PULSAR_RUNTIME_DEBUG_H

#include "pulsar/core.h"

#include "pulsar/lexer.h"

namespace Pulsar
{
    struct FunctionDebugSymbol
    {
        Pulsar::Token Token;
    };

    struct BlockDebugSymbol
    {
        Pulsar::Token Token;
        size_t StartIdx;
    };
}

#endif // _PULSAR_RUNTIME_DEBUG_H
