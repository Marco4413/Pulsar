#ifndef _PULSAR_PARSER_H
#define _PULSAR_PARSER_H

#include "pulsar/core.h"

#include "pulsar/lexer.h"
#include "pulsar/runtime.h"
#include "pulsar/structures/hashmap.h"
#include "pulsar/structures/list.h"

namespace Pulsar
{
    struct InstructionDescription { InstructionCode Code; bool MayFail = true; };
    static const HashMap<String, InstructionDescription> InstructionMappings {
        { "pop",        { InstructionCode::Pop                  } },
        { "swap",       { InstructionCode::Swap                 } },
        { "dup",        { InstructionCode::Dup                  } },
        { "floor",      { InstructionCode::Floor                } },
        { "ceil",       { InstructionCode::Ceil                 } },
        { "compare",    { InstructionCode::Compare              } },
        { "equals?",    { InstructionCode::Equals               } },
        { "icall",      { InstructionCode::ICall                } },
        { "length",     { InstructionCode::Length               } },
        { "empty?",     { InstructionCode::IsEmpty              } },
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
        { "void?",      { InstructionCode::IsVoid               } },
        { "integer?",   { InstructionCode::IsInteger            } },
        { "double?",    { InstructionCode::IsDouble             } },
        { "number?",    { InstructionCode::IsNumber             } },
        { "fn-ref?",        { InstructionCode::IsFunctionReference       } },
        { "native-fn-ref?", { InstructionCode::IsNativeFunctionReference } },
        { "any-fn-ref?",    { InstructionCode::IsAnyFunctionReference    } },
        { "list?",      { InstructionCode::IsList               } },
        { "string?",    { InstructionCode::IsString             } },
        { "custom?",    { InstructionCode::IsCustom             } },
        { "j!",         { InstructionCode::J, false             } },
        { "jz!",        { InstructionCode::JZ                   } },
        { "jnz!",       { InstructionCode::JNZ                  } },
        { "jgz!",       { InstructionCode::JGZ                  } },
        { "jgez!",      { InstructionCode::JGEZ                 } },
        { "jlz!",       { InstructionCode::JLZ                  } },
        { "jlez!",      { InstructionCode::JLEZ                 } },
    };

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
        const GlobalScope& Global;
        FunctionScope* const Function;
        List<String> Locals = List<String>();
    };

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
        IllegalDirective,
        NativeFunctionRedeclaration,
        UnsafeChainedIfStatement,
        FileSystemNotAvailable,
        UsageOfUndeclaredLabel,
        IllegalUsageOfLabel,
        LabelNotAllowedInContext,
        RedeclarationOfLabel,
    };

    const char* ParseResultToString(ParseResult presult);

    class Parser; // Forward Declaration
    struct ParseSettings
    {
        bool StoreDebugSymbols              = true;
        bool AppendStackTraceToErrorMessage = true;
        size_t StackTraceMaxDepth           = 10;
        bool AppendNotesToErrorMessage      = true;
        bool AllowIncludeDirective          = true;
        bool AllowLabels                    = true;
        /**
         * @brief (parser, cwf, token) -> ParseResult
         * @param parser The Parser that called the function.
         *     Parser#AddSource or #AddSourceFile should be called to add the source.
         * @param cwf The path to the current file.
         * @param token The StringLiteral Token containing the path to the file to include.
         */
        std::function<ParseResult(Parser&, String, Token)> IncludeResolver = nullptr;
    };

    inline const ParseSettings ParseSettings_Default{};

    class Parser
    {
    public:
        Parser() = default;
        ~Parser() = default;

        bool AddSource(const String& path, const String& src);
        bool AddSource(const String& path, String&& src);
        ParseResult AddSourceFile(const String& path);

        ParseResult ParseIntoModule(Module& module, const ParseSettings& settings=ParseSettings_Default);
        void Reset(); // Call Reset after ParseIntoModule to reuse the Parser

        const String* GetErrorSource() const  { return m_ErrorSource; }
        const String* GetErrorPath() const    { return m_ErrorPath; }
        ParseResult GetError() const          { return m_Error; }
        const String& GetErrorMessage() const { return m_ErrorMsg; }
        const Token& GetErrorToken() const    { return m_ErrorToken; }
        ParseResult SetError(ParseResult errorType, const Token& token, const String& errorMsg);
        void ClearError();

    private:
        ParseResult ParseModuleStatement(Module& module, GlobalScope& globalScope, const ParseSettings& settings);
        ParseResult ParseGlobalDefinition(Module& module, GlobalScope& globalScope, const ParseSettings& settings);
        ParseResult ParseFunctionDefinition(Module& module, GlobalScope& globalScope, const ParseSettings& settings);
        ParseResult BackPatchFunctionLabels(FunctionDefinition& func, const FunctionScope& funcScope);
        ParseResult ParseFunctionBody(Module& module, FunctionDefinition& func, const LocalScope& localScope, SkippableBlock* skippableBlock, const ParseSettings& settings);
        ParseResult ParseIfStatement(Module& module, FunctionDefinition& func, const LocalScope& localScope, SkippableBlock* skippableBlock, bool isChained, const ParseSettings& settings);
        ParseResult ParseLocalBlock(Module& module, FunctionDefinition& func, const LocalScope& localScope, SkippableBlock* skippableBlock, const ParseSettings& settings);
        ParseResult ParseWhileLoop(Module& module, FunctionDefinition& func, const LocalScope& localScope, const ParseSettings& settings);
        ParseResult ParseDoBlock(Module& module, FunctionDefinition& func, const LocalScope& localScope, const ParseSettings& settings);
        ParseResult PushLValue(Module& module, FunctionDefinition& func, const LocalScope& localScope, const Token& lvalue, const ParseSettings& settings);
    
        const Token& NextToken();
        const Token& CurrentToken() const;
        bool IsEndOfFile() const;

        // Pointers are guaranteed to be valid until m_LexerPool changes.
        // i.e. If parsing is on hold or has been aborted, they're safe to use.
        const String* CurrentPath() const;
        const String* CurrentSource() const;
    private:
        struct LexerSource
        {
            String Path;
            Pulsar::Lexer Lexer;
        };

        HashMap<String, std::nullptr_t> m_ParsedSources;
        List<LexerSource> m_LexerPool;

        Lexer* m_Lexer = nullptr;
        Token m_CurrentToken = Token(TokenType::None);

        const String* m_ErrorSource = nullptr;
        const String* m_ErrorPath = nullptr;
        ParseResult m_Error = ParseResult::OK;
        Token m_ErrorToken = Token(TokenType::None);
        String m_ErrorMsg = "";
    };
}


#endif // _PULSAR_PARSER_H
