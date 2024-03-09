#ifndef _PULSAR_PARSER_H
#define _PULSAR_PARSER_H

#include "pulsar/core.h"

#include "pulsar/lexer.h"
#include "pulsar/runtime.h"
#include "pulsar/structures/list.h"

namespace Pulsar
{
    struct InstructionDescription { InstructionCode Code; bool MayFail = true; };
    static const std::unordered_map<String, InstructionDescription> InstructionMappings {
        { "pop",        { InstructionCode::Pop                  } },
        { "swap",       { InstructionCode::Swap                 } },
        { "dup",        { InstructionCode::Dup                  } },
        { "floor",      { InstructionCode::Floor                } },
        { "ceil",       { InstructionCode::Ceil                 } },
        { "compare",    { InstructionCode::Compare              } },
        { "icall",      { InstructionCode::ICall                } },
        { "length",     { InstructionCode::Length               } },
        { "empty-list", { InstructionCode::PushEmptyList, false } },
        { "prepend",    { InstructionCode::Prepend              } },
        { "append",     { InstructionCode::Append               } },
        { "concat",     { InstructionCode::Concat               } },
        { "head",       { InstructionCode::Head                 } },
        { "tail",       { InstructionCode::Tail                 } },
        { "index",      { InstructionCode::Index                } },
        { "prefix",     { InstructionCode::Prefix               } },
        { "suffix",     { InstructionCode::Suffix               } },
        { "substr",     { InstructionCode::Substr               } },
    };

    struct SkippableBlock
    {
        const bool AllowBreak = false;
        const bool AllowContinue = false;
        // Used for back-patching jump addresses.
        List<size_t> BreakStatements = List<size_t>();
        List<size_t> ContinueStatements = List<size_t>();
    };

    struct ParseSettings
    {
        bool StoreDebugSymbols              = true;
        bool AppendStackTraceToErrorMessage = true;
        size_t StackTraceMaxDepth           = 10;
        bool AppendNotesToErrorMessage      = true;
        bool AllowIncludeDirective          = true;
    };
    
    constexpr ParseSettings ParseSettings_Default{};

    enum class ParseResult
    {
        OK = 0,
        Error = 1,
        FileNotRead,
        UnexpectedToken,
        NegativeResultCount,
        UsageOfUndeclaredLocal,
        UsageOfUnknownInstruction,
        UsageOfUndeclaredFunction,
        UsageOfUndeclaredNativeFunction,
        WritingToConstantGlobal,
        GlobalEvaluationError,
        IllegalDirective
    };

    const char* ParseResultToString(ParseResult presult);

    class Parser
    {
    public:
        typedef List<String> LocalsBindings;
        Parser() { }

        bool AddSource(const String& path, const String& src);
        bool AddSource(const String& path, String&& src);
        ParseResult AddSourceFile(const String& path);

        ParseResult ParseIntoModule(Module& module, const ParseSettings& settings=ParseSettings_Default);
        const String& GetLastErrorSource() const  { return *m_LastErrorSource; }
        const String& GetLastErrorPath() const    { return *m_LastErrorPath; }
        ParseResult GetLastError() const          { return m_LastError; }
        const String& GetLastErrorMessage() const { return m_LastErrorMsg; }
        const Token& GetLastErrorToken() const    { return m_LastErrorToken; }
    private:
        ParseResult ParseModuleStatement(Module& module, const ParseSettings& settings);
        ParseResult ParseGlobalDefinition(Module& module, const ParseSettings& settings);
        ParseResult ParseFunctionDefinition(Module& module, const ParseSettings& settings);
        ParseResult ParseFunctionBody(Module& module, FunctionDefinition& func, const LocalsBindings& locals, SkippableBlock* skippableBlock, const ParseSettings& settings);
        ParseResult ParseIfStatement(Module& module, FunctionDefinition& func, const LocalsBindings& locals, SkippableBlock* skippableBlock, const ParseSettings& settings);
        ParseResult ParseWhileLoop(Module& module, FunctionDefinition& func, const LocalsBindings& locals, const ParseSettings& settings);
        ParseResult ParseDoBlock(Module& module, FunctionDefinition& func, const LocalsBindings& locals, const ParseSettings& settings);
        ParseResult PushLValue(Module& module, FunctionDefinition& func, const LocalsBindings& locals, const Token& lvalue, const ParseSettings& settings);
        ParseResult SetError(ParseResult errorType, const Token& token, const String& errorMsg)
        {
            m_LastErrorSource = &m_LexerPool.Back().Lexer.GetSource();
            m_LastErrorPath = &m_LexerPool.Back().Path;
            m_LastError = errorType;
            m_LastErrorToken = token;
            m_LastErrorMsg = errorMsg;
            return errorType;
        }
    private:
        struct LexerSource
        {
            String Path;
            Pulsar::Lexer Lexer;
        };

        std::unordered_set<String> m_ParsedSources;
        List<LexerSource> m_LexerPool;

        Lexer* m_Lexer = nullptr;
        const String* m_LastErrorSource = nullptr;
        const String* m_LastErrorPath = nullptr;
        ParseResult m_LastError = ParseResult::OK;
        Token m_LastErrorToken = Token(TokenType::None);
        String m_LastErrorMsg = "";
    };
}


#endif // _PULSAR_PARSER_H
