#ifndef _PULSAR_PARSER_H
#define _PULSAR_PARSER_H

#include <cinttypes>
#include <string>
#include <unordered_map>
#include <vector>

#include "pulsar/lexer.h"
#include "pulsar/runtime.h"

namespace Pulsar
{
    enum class ParseResult
    {
        OK = 0,
        Error = 1,
        UnexpectedToken
    };

    class Parser
    {
    public:
        typedef std::vector<std::string> LocalsBindings;
        Parser(const std::string& src)
            : m_Lexer(src) { }
        Parser(std::string&& src)
            : m_Lexer(src) { }

        ParseResult ParseIntoModule(Module& module);
    private:
        ParseResult ParseFunctionDefinition(Module& module);
        ParseResult ParseFunctionBody(Module& module, FunctionDefinition& func, const LocalsBindings& locals);
        ParseResult ParseIfStatement(Module& module, FunctionDefinition& func, const LocalsBindings& locals);
    private:
        Lexer m_Lexer;
    };
}


#endif // _PULSAR_PARSER_H
