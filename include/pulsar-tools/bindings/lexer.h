#ifndef _PULSARTOOLS_BINDINGS_LEXER_H
#define _PULSARTOOLS_BINDINGS_LEXER_H

#include "pulsar/lexer.h"
#include "pulsar/structures/string.h"

#include "pulsar-tools/binding.h"

namespace PulsarTools::Bindings
{
    class Lexer :
        public IBinding
    {
    public:
        class LexerType :
            public Pulsar::CustomDataHolder
        {
        public:
            using Ref = Pulsar::SharedRef<LexerType>;

            LexerType(Pulsar::String&& source) :
                m_Source(std::move(source)),
                m_Lexer(m_Source)
            {}

            virtual ~LexerType() = default;

            // Lexer has no virtual destructor so we must wrap it
            Pulsar::Lexer& operator*() { return m_Lexer; }
            const Pulsar::Lexer& operator*() const { return m_Lexer; }

        private:
            Pulsar::String m_Source;
            Pulsar::Lexer m_Lexer;
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
