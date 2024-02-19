#ifndef _PULSAR_PARSER_H
#define _PULSAR_PARSER_H

#include "pulsar/core.h"

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
        typedef std::vector<String> LocalsBindings;
        Parser(const String& src)
            : m_Lexer(src) { }
        Parser(String&& src)
            : m_Lexer(src) { }

        ParseResult ParseIntoModule(Module& module, bool debugSymbols=false);
        const String& GetSource() const    { return m_Lexer.GetSource(); }
        ParseResult GetLastError() const        { return m_LastError; }
        const char* GetLastErrorMessage() const { return m_LastErrorMsg.Data(); }
        const Token& GetLastErrorToken() const  { return m_LastErrorToken; }
    private:
        ParseResult ParseFunctionDefinition(Module& module, bool debugSymbols);
        ParseResult ParseFunctionBody(Module& module, FunctionDefinition& func, const LocalsBindings& locals, bool debugSymbols);
        ParseResult ParseIfStatement(Module& module, FunctionDefinition& func, const LocalsBindings& locals, bool debugSymbols);
        ParseResult PushLValue(Module& module, FunctionDefinition& func, const LocalsBindings& locals, const Token& lvalue, bool debugSymbols);
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
        String m_LastErrorMsg = "";
    };
}


#endif // _PULSAR_PARSER_H
