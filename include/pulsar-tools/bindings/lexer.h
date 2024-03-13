#ifndef _PULSARTOOLS_BINDINGS_LEXER_H
#define _PULSARTOOLS_BINDINGS_LEXER_H

#include "pulsar-tools/core.h"

#include "pulsar/lexer.h"
#include "pulsar/runtime.h"
#include "pulsar/structures/hashmap.h"

namespace PulsarTools
{
    class LexerNativeBindings
    {
    public:
        LexerNativeBindings() = default;
        ~LexerNativeBindings() = default;

        void BindToModule(Pulsar::Module& module);

        Pulsar::RuntimeState Lexer_FromFile(Pulsar::ExecutionContext& eContext, uint64_t type);
        Pulsar::RuntimeState Lexer_NextToken(Pulsar::ExecutionContext& eContext, uint64_t type);
        Pulsar::RuntimeState Lexer_Free(Pulsar::ExecutionContext& eContext, uint64_t type);
        Pulsar::RuntimeState Lexer_IsValid(Pulsar::ExecutionContext& eContext, uint64_t type);

    private:
        int64_t m_NextHandle = 1;
        Pulsar::HashMap<int64_t, Pulsar::Lexer> m_Lexers;
    };
}

#endif // _PULSARTOOLS_BINDINGS_LEXER_H
