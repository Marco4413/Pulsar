#ifndef _PULSAR_RUNTIME_DEBUG_H
#define _PULSAR_RUNTIME_DEBUG_H

#include <cinttypes>

#include "pulsar/lexer.h"

namespace Pulsar
{
    struct DebugSymbol
    {
        Pulsar::Token Token;
    };

    struct BlockDebugSymbol : public DebugSymbol
    {
        BlockDebugSymbol(const Pulsar::Token& token, size_t startIdx)
            : DebugSymbol{token}, StartIdx(startIdx) { }
        size_t StartIdx;
    };
}

#endif // _PULSAR_RUNTIME_DEBUG_H
