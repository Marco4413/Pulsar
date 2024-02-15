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
        UnexpectedToken,
        NegativeResultCount,
        UsageOfUndeclaredLocal,
        UsageOfUndeclaredFunction,
        UsageOfUndeclaredNativeFunction
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
        const std::string& GetSource() const    { return m_Lexer.GetSource(); }
        ParseResult GetLastError() const        { return m_LastError; }
        const char* GetLastErrorMessage() const { return m_LastErrorMsg.c_str(); }
        const Token& GetLastErrorToken() const  { return m_LastErrorToken; }
    private:
        ParseResult ParseFunctionDefinition(Module& module);
        ParseResult ParseFunctionBody(Module& module, FunctionDefinition& func, const LocalsBindings& locals);
        ParseResult ParseIfStatement(Module& module, FunctionDefinition& func, const LocalsBindings& locals);
        ParseResult SetError(ParseResult errorType, const Token& token, const char* errorMsg)
        {
            m_LastError = errorType;
            m_LastErrorToken = token;
            m_LastErrorMsg = errorMsg;
            return errorType;
        }
    private:
        Lexer m_Lexer;
        ParseResult m_LastError = ParseResult::OK;
        Token m_LastErrorToken = Token(TokenType::None);
        std::string m_LastErrorMsg = "";
    };
}


#endif // _PULSAR_PARSER_H
