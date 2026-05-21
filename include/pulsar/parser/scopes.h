#ifndef _PULSAR_PARSER_SCOPES_H
#define _PULSAR_PARSER_SCOPES_H

#include "pulsar/core.h"

#include "pulsar/lexer/token.h"
#include "pulsar/structures/hashmap.h"
#include "pulsar/structures/string.h"

namespace Pulsar
{
    struct SkippableBlock
    {
        const bool AllowBreak = false;
        const bool AllowContinue = false;
        // Used for back-patching jump addresses.
        List<size_t> BreakStatements = List<size_t>();
        List<size_t> ContinueStatements = List<size_t>();
    };

    struct GlobalScope
    {
        // Path -> Idx map
        HashMap<String, size_t> SourceDebugSymbols;
        // Name -> Idx maps
        HashMap<String, size_t> Functions;
        HashMap<String, size_t> NativeFunctions;
        HashMap<String, size_t> Globals;
    };

    struct FunctionScope
    {
        struct Label
        {
            Token Label;
            size_t CodeDstIdx;
        };

        struct LabelBackPatch
        {
            Token Label;
            size_t CodeIdx;
        };

        HashMap<String, Label> Labels;
        List<LabelBackPatch> LabelUsages;
    };

    struct LocalScope
    {
        struct LocalVar
        {
            String Name;
            SourcePosition DeclaredAt;
        };

        const GlobalScope& Global;
        FunctionScope* const Function;
        List<LocalVar> Locals = List<LocalVar>();
    };
}

#endif // _PULSAR_PARSER_SCOPES_H
