#ifndef _PULSAR_RUNTIME_GLOBAL_H
#define _PULSAR_RUNTIME_GLOBAL_H

#include "pulsar/core.h"

#include "pulsar/runtime/debug.h"
#include "pulsar/runtime/value.h"

namespace Pulsar
{
    struct GlobalInstance
    {
        Pulsar::Value Value;
        bool IsConstant = false;
    };

    struct GlobalDefinition
    {
        String Name;
        Value InitialValue;
        bool IsConstant = false;

        GlobalInstance CreateInstance() const { return { InitialValue, IsConstant }; }
        
        GlobalDebugSymbol DebugSymbol{Token(TokenType::None), (size_t)-1};
        bool HasDebugSymbol() const { return DebugSymbol.Token.Type != TokenType::None; }
    };
}

#endif // _PULSAR_RUNTIME_GLOBAL_H
