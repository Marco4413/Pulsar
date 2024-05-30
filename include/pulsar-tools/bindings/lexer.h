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
        class LexerType : public Pulsar::CustomDataHolder, public Pulsar::Lexer
        {
        public:
            using Ref_T = Pulsar::SharedRef<LexerType>;
            using Pulsar::Lexer::Lexer;
        };

        void BindToModule(Pulsar::Module& module);

        Pulsar::RuntimeState Lexer_FromFile(Pulsar::ExecutionContext& eContext, uint64_t type);
        Pulsar::RuntimeState Lexer_NextToken(Pulsar::ExecutionContext& eContext, uint64_t type);
        Pulsar::RuntimeState Lexer_IsValid(Pulsar::ExecutionContext& eContext, uint64_t type);
    }
}

#endif // _PULSARTOOLS_BINDINGS_LEXER_H
