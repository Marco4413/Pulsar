#ifndef _PULSAR_RUNTIME_FUNCTION_H
#define _PULSAR_RUNTIME_FUNCTION_H

#include "pulsar/core.h"

#include "pulsar/runtime/debug.h"
#include "pulsar/runtime/instruction.h"
#include "pulsar/structures/list.h"

namespace Pulsar
{
    struct FunctionDefinition
    {
        String Name;
        size_t Arity;
        size_t Returns;
        
        size_t LocalsCount = Arity;
        List<Instruction> Code = List<Instruction>();
        
        FunctionDebugSymbol DebugSymbol{Token(TokenType::None), (size_t)-1};
        List<BlockDebugSymbol> CodeDebugSymbols = List<BlockDebugSymbol>();

        bool HasDebugSymbol() const { return DebugSymbol.Token.Type != TokenType::None; }
        bool HasCodeDebugSymbols() const { return !CodeDebugSymbols.IsEmpty(); }

        bool MatchesDeclaration(const FunctionDefinition& other, bool matchLocals=true) const
        {
            return Arity == other.Arity
                && (!matchLocals || LocalsCount == other.LocalsCount)
                && Returns == other.Returns
                && Name == other.Name;
        }
    };
}

#endif // _PULSAR_RUNTIME_FUNCTION_H
