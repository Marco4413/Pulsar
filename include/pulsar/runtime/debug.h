#ifndef _PULSAR_RUNTIME_DEBUG_H
#define _PULSAR_RUNTIME_DEBUG_H

#include "pulsar/core.h"

#include "pulsar/lexer.h"
#include "pulsar/structures/string.h"

namespace Pulsar
{
    struct SourceDebugSymbol
    {
        String Source;
    };

    struct FunctionDebugSymbol
    {
        Pulsar::Token Token;
        size_t SourceIdx;
    };

    struct BlockDebugSymbol
    {
        Pulsar::Token Token;
        size_t StartIdx;
    };
}

#endif // _PULSAR_RUNTIME_DEBUG_H
