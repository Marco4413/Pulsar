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
        
        size_t StackArity = 0;
        size_t LocalsCount = Arity;

        List<Instruction> Code = List<Instruction>();
        
        FunctionDebugSymbol DebugSymbol{Token(TokenType::None), (size_t)-1};
        List<BlockDebugSymbol> CodeDebugSymbols = List<BlockDebugSymbol>();

        bool HasDebugSymbol() const { return DebugSymbol.Token.Type != TokenType::None; }
        bool HasCodeDebugSymbols() const { return !CodeDebugSymbols.IsEmpty(); }

        bool MatchesDeclaration(const FunctionDefinition& other) const
        {
            return StackArity == other.StackArity
                && Arity == other.Arity
                && LocalsCount == other.LocalsCount
                && Returns == other.Returns
                && Name == other.Name;
        }

        bool MatchesSignature(const FunctionDefinition& other) const
        {
            return StackArity == other.StackArity
                && Arity == other.Arity
                && Returns == other.Returns
                && Name == other.Name;
        }

        bool FindCodeDebugSymbolFor(size_t instructionIdx, size_t& symbolIdxOut) const {
            if (!HasCodeDebugSymbols())
                return false;
            symbolIdxOut = 0;
            for (size_t i = 0; i < CodeDebugSymbols.Size(); i++) {
                if (CodeDebugSymbols[i].StartIdx > instructionIdx)
                    break;
                symbolIdxOut = i;
            }
            return true;
        }
    };
}

#endif // _PULSAR_RUNTIME_FUNCTION_H
