#ifndef _PULSAR_RUNTIME_FUNCTION_H
#define _PULSAR_RUNTIME_FUNCTION_H

#include <string>
#include <vector>

#include "pulsar/runtime/debug.h"
#include "pulsar/runtime/instruction.h"

namespace Pulsar
{
    struct FunctionDefinition
    {
        std::string Name;
        size_t Arity;
        size_t Returns;
        
        size_t LocalsCount = Arity;
        std::vector<Instruction> Code = {};
        
        DebugSymbol FunctionDebugSymbol{Token(TokenType::None)};
        std::vector<BlockDebugSymbol> CodeDebugSymbols = {};

        bool HasDebugSymbol() const { return FunctionDebugSymbol.Token.Type != TokenType::None; }
        bool HasCodeDebugSymbols() const { return CodeDebugSymbols.size() > 0; }

        bool MatchesDeclaration(const FunctionDefinition& other) const
        {
            return Arity == other.Arity
                && LocalsCount == other.LocalsCount
                && Returns == other.Returns
                && Name == other.Name;
        }
    };
}

#endif // _PULSAR_RUNTIME_FUNCTION_H
