#ifndef _PULSARTOOLS_BINDINGS_LEXER_H
#define _PULSARTOOLS_BINDINGS_LEXER_H

#include "pulsar-tools/core.h"

#include "pulsar/lexer.h"
#include "pulsar/runtime.h"
#include "pulsar/structures/hashmap.h"

namespace PulsarTools
{
    namespace LexerNativeBindings
    {
        class LexerTypeData : public Pulsar::CustomTypeData
        {
        public:
            int64_t NextHandle = 1;
            Pulsar::HashMap<int64_t, Pulsar::Lexer> Lexers;
        };

        void BindToModule(Pulsar::Module& module);

        Pulsar::RuntimeState Lexer_FromFile(Pulsar::ExecutionContext& eContext, uint64_t type);
        Pulsar::RuntimeState Lexer_NextToken(Pulsar::ExecutionContext& eContext, uint64_t type);
        Pulsar::RuntimeState Lexer_Free(Pulsar::ExecutionContext& eContext, uint64_t type);
        Pulsar::RuntimeState Lexer_IsValid(Pulsar::ExecutionContext& eContext, uint64_t type);
    }
}

#endif // _PULSARTOOLS_BINDINGS_LEXER_H
