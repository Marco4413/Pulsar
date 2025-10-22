#ifndef _PULSAR_RUNTIME_FUNCTION_H
#define _PULSAR_RUNTIME_FUNCTION_H

#include "pulsar/core.h"

#include "pulsar/runtime/debug.h"
#include "pulsar/runtime/instruction.h"
#include "pulsar/structures/list.h"
#include "pulsar/structures/string.h"
#include "pulsar/structures/stringview.h"

namespace Pulsar
{
    struct FunctionDefinition
    {
        String Name;
        size_t Arity;
        size_t Returns;
        
        size_t StackArity;
        size_t LocalsCount;

        List<Instruction> Code = List<Instruction>();
        
        FunctionDebugSymbol DebugSymbol{Token(TokenType::None), (size_t)-1};
        List<BlockDebugSymbol> CodeDebugSymbols = List<BlockDebugSymbol>();

        bool HasDebugSymbol() const { return DebugSymbol.Token.Type != TokenType::None; }
        bool HasCodeDebugSymbols() const { return !CodeDebugSymbols.IsEmpty(); }

        bool DeclarationMatches(const FunctionDefinition& def) const
        {
            return StackArity  == def.StackArity
                && Arity       == def.Arity
                && LocalsCount == def.LocalsCount
                && Returns     == def.Returns
                && Name        == def.Name;
        }

        bool FindCodeDebugSymbolFor(size_t instructionIdx, size_t& symbolIdxOut) const
        {
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

    // This struct is meant to be a lightweight version of a FunctionDefinition.
    // The main performance benefit is that Name does not alloc since it's a StringView.
    struct FunctionSignature
    {
        StringView Name;
        size_t Arity;
        size_t Returns;
        size_t StackArity = 0;

        bool Matches(const FunctionDefinition& def) const
        {
            return StackArity == def.StackArity
                && Arity      == def.Arity
                && Returns    == def.Returns
                && Name       == def.Name;
        }

        bool MatchesNative(const FunctionDefinition& def) const
        {
            return StackArity == def.StackArity
                && Arity      == def.Arity
                && Arity      == def.LocalsCount
                && Returns    == def.Returns
                && Name       == def.Name;
        }

        FunctionDefinition ToNativeDefinition() const
        {
            return { String(Name.DataFromStart(), Name.Length()), Arity, Returns, StackArity, Arity };
        }
    };
}

#endif // _PULSAR_RUNTIME_FUNCTION_H
