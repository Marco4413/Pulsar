#ifndef _PULSARTOOLS_BINDINGS_LEXER_H
#define _PULSARTOOLS_BINDINGS_LEXER_H

#include "pulsar/lexer.h"

#include "pulsar-tools/binding.h"

namespace PulsarTools::Bindings
{
    class Lexer :
        public IBinding
    {
    public:
        class LexerType :
            public Pulsar::CustomDataHolder,
            public Pulsar::Lexer
        {
        public:
            using Ref = Pulsar::SharedRef<LexerType>;
            using Pulsar::Lexer::Lexer;
        };

    public:
        Lexer();

    public:
        static Pulsar::RuntimeState FFromFile(Pulsar::ExecutionContext& eContext, uint64_t lexerTypeId);
        static Pulsar::RuntimeState FNextToken(Pulsar::ExecutionContext& eContext, uint64_t lexerTypeId);
        static Pulsar::RuntimeState FIsValid(Pulsar::ExecutionContext& eContext, uint64_t lexerTypeId);
    };
}

#endif // _PULSARTOOLS_BINDINGS_LEXER_H
