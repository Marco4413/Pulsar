#include "pulsar/parser.h"

#include <filesystem>
#include <fstream>

Pulsar::ParseResult Pulsar::Parser::SetError(ParseResult errorType, const Token& token, const String& errorMsg)
{
    m_ErrorSource = nullptr;
    m_ErrorPath = nullptr;
    if (m_LexerPool.Size() > 0) {
        m_ErrorSource = &m_LexerPool.Back().Lexer.GetSource();
        m_ErrorPath = &m_LexerPool.Back().Path;
    }
    m_Error = errorType;
    m_ErrorToken = token;
    m_ErrorMsg = errorMsg;
    return errorType;
}

void Pulsar::Parser::ClearError()
{
    m_ErrorSource = nullptr;
    m_ErrorPath = nullptr;
    m_Error = ParseResult::OK;
    m_ErrorToken = Token(TokenType::None);
    m_ErrorMsg.Resize(0);
}

void Pulsar::Parser::Reset()
{
    ClearError();
    m_ParsedSources.clear();
    m_LexerPool.Clear();
}

bool Pulsar::Parser::AddSource(const String& path, const String& src)
{
    ClearError();
    if (path.Length() > 0) {
        if (m_ParsedSources.contains(path))
            return false;
        m_ParsedSources.emplace(path);
    }

    m_LexerPool.EmplaceBack(path, src);
    m_Lexer = &m_LexerPool.Back().Lexer;
    return true;
}

bool Pulsar::Parser::AddSource(const String& path, String&& src)
{
    ClearError();
    if (path.Length() > 0) {
        if (m_ParsedSources.contains(path))
            return false;
        m_ParsedSources.emplace(path);
    }

    m_LexerPool.EmplaceBack(path, std::move(src));
    m_Lexer = &m_LexerPool.Back().Lexer;
    return true;
}

Pulsar::ParseResult Pulsar::Parser::AddSourceFile(const String& path)
{
    auto fsPath = std::filesystem::path(path.Data());
    std::error_code error;
    auto relativePath = std::filesystem::relative(fsPath, error);

    Token token(TokenType::None);
    if (m_Lexer) token = m_Lexer->CurrentToken();

    if (error)
        return SetError(ParseResult::FileNotRead, token, "Could not normalize path '" + path + "'.");
    else if (relativePath.empty())
        return SetError(ParseResult::FileNotRead, token, "Could not resolve file '" + path + "'.");

    String internalPath = relativePath.generic_string().c_str();
    if (!std::filesystem::exists(relativePath))
        return SetError(ParseResult::FileNotRead, token, "File '" + internalPath + "' does not exist.");
    
    std::ifstream file(relativePath, std::ios::binary);
    size_t fileSize = (size_t)std::filesystem::file_size(relativePath);

    Pulsar::String source;
    source.Resize(fileSize);
    if (!file.read((char*)source.Data(), fileSize))
        return SetError(ParseResult::FileNotRead, token, "Could not read file '" + internalPath + "'.");

    AddSource(internalPath, std::move(source));
    return ParseResult::OK;
}

Pulsar::ParseResult Pulsar::Parser::ParseIntoModule(Module& module, const ParseSettings& settings)
{
    ClearError();
    if (settings.StoreDebugSymbols) {
        for (size_t i = 0; i < m_LexerPool.Size(); i++)
            module.SourceDebugSymbols.EmplaceBack(m_LexerPool[i].Path, m_LexerPool[i].Lexer.GetSource());
    }
    while (m_LexerPool.Size() > 0) {
        auto result = ParseModuleStatement(module, settings);
        if (result == ParseResult::OK) {
            if (m_Lexer->IsEndOfFile()) {
                m_LexerPool.PopBack();
                m_Lexer = &m_LexerPool.Back().Lexer;
            }
            continue;
        }
        return result;
    }
    module.NativeFunctions.Resize(module.NativeBindings.Size(), nullptr);
    return ParseResult::OK;
}

Pulsar::ParseResult Pulsar::Parser::ParseModuleStatement(Module& module, const ParseSettings& settings)
{
    const Token& curToken = m_Lexer->NextToken();
    switch (curToken.Type) {
    case TokenType::Star:
        return ParseFunctionDefinition(module, settings);
    case TokenType::CompilerDirective: {
        if (curToken.IntegerVal != TOKEN_CD_INCLUDE)
            return SetError(ParseResult::UnexpectedToken, curToken, "Unknown compiler directive.");
        else if (!settings.AllowIncludeDirective)
            return SetError(ParseResult::IllegalDirective, curToken, "Include compiler directive was disabled.");
        m_Lexer->NextToken();
        if (curToken.Type != TokenType::StringLiteral) {
            return SetError(ParseResult::UnexpectedToken, curToken, "Expected file path.");
        } else if (settings.IncludeResolver) {
            auto res = settings.IncludeResolver(*this, m_LexerPool.Back().Path, curToken);
            if (res != ParseResult::OK)
                return res;
        } else {
            std::filesystem::path targetPath(curToken.StringVal.Data());
            std::filesystem::path workingPath(m_LexerPool.Back().Path.Data());
            std::filesystem::path filePath = workingPath.parent_path() / targetPath;
            auto result = AddSourceFile(filePath.generic_string().data());
            if (result != ParseResult::OK)
                return result;
        }
        if (settings.StoreDebugSymbols)
            module.SourceDebugSymbols.EmplaceBack(m_LexerPool.Back().Path, m_LexerPool.Back().Lexer.GetSource());
        return ParseResult::OK;
    }
    case TokenType::KW_Global:
        return ParseGlobalDefinition(module, settings);
    case TokenType::EndOfFile:
        return ParseResult::OK;
    default:
        return SetError(ParseResult::UnexpectedToken, curToken, "Expected function declaration or compiler directive.");
    }
}

Pulsar::ParseResult Pulsar::Parser::ParseGlobalDefinition(Module& module, const ParseSettings& settings)
{
    const Token& curToken = m_Lexer->CurrentToken();
    if (curToken.Type != TokenType::KW_Global)
        return SetError(ParseResult::UnexpectedToken, curToken, "Expected 'global' to begin global definition.");
    
    m_Lexer->NextToken();
    Token constToken = curToken;
    bool isConstant = constToken.Type == TokenType::KW_Const;
    if (isConstant) m_Lexer->NextToken();

    FunctionDefinition dummyFunc{"", 0, 1};
    bool isProducer = curToken.Type == TokenType::RightArrow;

    LocalsBindings locals;
    if (!isProducer) {
        ParseSettings subSettings = settings;
        // We want to know where, within the body of the global, the runtime failed.
        subSettings.StoreDebugSymbols = true;
        auto result = PushLValue(module, dummyFunc, locals, curToken, subSettings);
        if (result != ParseResult::OK)
            return result;
        m_Lexer->NextToken();
    }
    
    if (curToken.Type != TokenType::RightArrow)
        return SetError(ParseResult::UnexpectedToken, curToken, "Expected '->' to assign global value.");
    m_Lexer->NextToken();
    if (curToken.Type != TokenType::Identifier)
        return SetError(ParseResult::UnexpectedToken, curToken, "Expected name for global.");
    Token identToken = curToken;

    for (size_t i = 0; i < module.Globals.Size(); i++) {
        if (module.Globals[i].Name == identToken.StringVal) {
            if (module.Globals[i].IsConstant)
                return SetError(ParseResult::WritingToConstantGlobal, identToken, "Trying to reassign constant global.");
            else if (isConstant)
                return SetError(ParseResult::UnexpectedToken, constToken, "Redeclaring global as const.");
            break;
        }
    }

    if (isProducer) {
        m_Lexer->NextToken();
        if (curToken.Type != TokenType::Colon)
            return SetError(ParseResult::UnexpectedToken, curToken, "Expected ':' to begin global producer body.");
        ParseSettings subSettings = settings;
        // We want to know where, within the body of the global, the runtime failed.
        subSettings.StoreDebugSymbols = true;
        auto result = ParseFunctionBody(module, dummyFunc, locals, nullptr, subSettings);
        if (result != ParseResult::OK)
            return result;
    }

    // Assign name after ParseFunctionBody to prevent self-recursion
    dummyFunc.Name = "{g";
    dummyFunc.Name += identToken.StringVal;
    dummyFunc.Name += '}';
    auto stack = ValueStack();
    auto context = module.CreateExecutionContext();
    auto evalResult = module.ExecuteFunction(dummyFunc, stack, context);
    if (evalResult != RuntimeState::OK || stack.Size() == 0) {
        size_t instrIdx = context.CallStack[0].InstructionIndex;
        size_t symbolIdx = 0;
        for (size_t i = 0; i < dummyFunc.CodeDebugSymbols.Size(); i++) {
            if (dummyFunc.CodeDebugSymbols[i].StartIdx >= instrIdx)
                break;
            symbolIdx = i;
        }

        String errorMsg = String("Error while evaluating value of global (") + RuntimeStateToString(evalResult) + ").";
        if (settings.AppendStackTraceToErrorMessage)
            errorMsg += "\n" + context.GetStackTrace(settings.StackTraceMaxDepth);
        if (settings.AppendNotesToErrorMessage
            && evalResult == RuntimeState::NativeFunctionBindingsMismatch)
            errorMsg += "\nNote: Native functions may not be bound during parsing.";
        return SetError(
            ParseResult::GlobalEvaluationError,
            dummyFunc.CodeDebugSymbols[symbolIdx].Token,
            std::move(errorMsg));
    }
    
    GlobalDefinition& globalDef = module.Globals.EmplaceBack(std::move(identToken.StringVal), std::move(stack.Back()), isConstant);
    if (settings.StoreDebugSymbols) {
        const auto& lexSource = m_LexerPool.Back();
        size_t srcIdx = 0;
        for (; srcIdx < module.SourceDebugSymbols.Size() && module.SourceDebugSymbols[srcIdx].Path != lexSource.Path; srcIdx++);
        globalDef.DebugSymbol.Token = identToken;
        globalDef.DebugSymbol.SourceIdx = srcIdx;
    }

    return ParseResult::OK;
}

Pulsar::ParseResult Pulsar::Parser::ParseFunctionDefinition(Module& module, const ParseSettings& settings)
{
    const Token& curToken = m_Lexer->CurrentToken();
    if (curToken.Type != TokenType::Star)
        return SetError(ParseResult::UnexpectedToken, curToken, "Expected '*' to begin function declaration.");

    m_Lexer->NextToken();
    if (curToken.Type != TokenType::OpenParenth)
        return SetError(ParseResult::UnexpectedToken, curToken, "Expected '(' to open function name and args declaration.");
    m_Lexer->NextToken();
    bool isNative = curToken.Type == TokenType::Star;
    if (isNative) m_Lexer->NextToken();

    if (curToken.Type != TokenType::Identifier)
        return SetError(ParseResult::UnexpectedToken, curToken, "Expected function identifier.");
    FunctionDefinition def = { curToken.StringVal, 0, 0 };
    if (settings.StoreDebugSymbols) {
        const auto& lexSource = m_LexerPool.Back();
        size_t srcIdx = 0;
        // TODO: Find a better way of doing this.
        for (; srcIdx < module.SourceDebugSymbols.Size() && module.SourceDebugSymbols[srcIdx].Path != lexSource.Path; srcIdx++);
        def.DebugSymbol.Token = curToken;
        def.DebugSymbol.SourceIdx = srcIdx;
    }

    m_Lexer->NextToken();
    LocalsBindings args;
    for (;;) {
        if (curToken.Type != TokenType::Identifier)
            break;
        args.PushBack(std::move(curToken.StringVal));
        m_Lexer->NextToken();
    }
    def.Arity = args.Size();
    def.LocalsCount = args.Size();

    if (curToken.Type != TokenType::CloseParenth)
        return SetError(ParseResult::UnexpectedToken, curToken, "Expected ')' to close function name and args declaration.");
    m_Lexer->NextToken();
    if (curToken.Type == TokenType::RightArrow) {
        m_Lexer->NextToken();
        if (curToken.Type != TokenType::IntegerLiteral)
            return SetError(ParseResult::UnexpectedToken, curToken, "Expected return count.");
        else if (curToken.IntegerVal < 0)
            return SetError(ParseResult::NegativeResultCount, curToken, "Illegal return count. Return count must be >= 0");
        def.Returns = (size_t)curToken.IntegerVal;
        m_Lexer->NextToken();
    }

    if (isNative) {
        if (curToken.Type != TokenType::FullStop)
            return SetError(ParseResult::UnexpectedToken, curToken,
                "Expected '.' to confirm native function declaration. Native functions can't have a body.");
        // If the native already exists push symbols (the function may have been defined outside the Parser)
        int64_t nativeIdx = (int64_t)module.NativeBindings.Size()-1;
        for (; nativeIdx >= 0 && !module.NativeBindings[(size_t)nativeIdx].MatchesDeclaration(def); nativeIdx--);
        if (nativeIdx < 0)
            module.NativeBindings.PushBack(std::move(def));
        else if (settings.StoreDebugSymbols)
            module.NativeBindings[(size_t)nativeIdx].DebugSymbol = def.DebugSymbol;
    } else {
        if (curToken.Type != TokenType::Colon)
            return SetError(ParseResult::UnexpectedToken, curToken, "Expected '->' for return count declaration or ':' to begin function body.");
        ParseResult bodyParseResult = ParseFunctionBody(module, def, args, nullptr, settings);
        if (bodyParseResult != ParseResult::OK)
            return bodyParseResult;
        module.Functions.PushBack(std::move(def));
    }

    return ParseResult::OK;
}

#define PUSH_CODE_SYMBOL(cond, func, token) \
    if (cond) (func).CodeDebugSymbols.EmplaceBack((token), (func).Code.Size())

Pulsar::ParseResult Pulsar::Parser::ParseFunctionBody(
    Module& module, FunctionDefinition& func,
    const LocalsBindings& locals, SkippableBlock* skippableBlock,
    const ParseSettings& settings)
{
    LocalsBindings scopedLocals = locals;
    for (;;) {
        const Token& curToken = m_Lexer->NextToken();
        switch (curToken.Type) {
        case TokenType::FullStop:
            PUSH_CODE_SYMBOL(settings.StoreDebugSymbols, func, curToken);
            func.Code.EmplaceBack(InstructionCode::Return);
            return ParseResult::OK;
        case TokenType::KW_Break:
            if (!skippableBlock || !skippableBlock->AllowBreak)
                return SetError(ParseResult::UnexpectedToken, curToken, "Trying to break out of an un-breakable block.");
            PUSH_CODE_SYMBOL(settings.StoreDebugSymbols, func, curToken);
            skippableBlock->BreakStatements.PushBack(func.Code.Size());
            func.Code.EmplaceBack(InstructionCode::Jump, 0);
            return ParseResult::OK;
        case TokenType::KW_Continue:
            if (!skippableBlock || !skippableBlock->AllowContinue)
                return SetError(ParseResult::UnexpectedToken, curToken, "Trying to repeat an un-repeatable block.");
            PUSH_CODE_SYMBOL(settings.StoreDebugSymbols, func, curToken);
            skippableBlock->ContinueStatements.PushBack(func.Code.Size());
            func.Code.EmplaceBack(InstructionCode::Jump, 0);
            return ParseResult::OK;
        case TokenType::KW_Do: {
            auto res = ParseDoBlock(module, func, scopedLocals, settings);
            if (res != ParseResult::OK)
                return res;
        } break;
        case TokenType::KW_While: {
            PUSH_CODE_SYMBOL(settings.StoreDebugSymbols, func, curToken);
            auto res = ParseWhileLoop(module, func, scopedLocals, settings);
            if (res != ParseResult::OK)
                return res;
        } break;
        case TokenType::Plus:
            PUSH_CODE_SYMBOL(settings.StoreDebugSymbols, func, curToken);
            func.Code.EmplaceBack(InstructionCode::DynSum);
            break;
        case TokenType::Minus:
            PUSH_CODE_SYMBOL(settings.StoreDebugSymbols, func, curToken);
            func.Code.EmplaceBack(InstructionCode::DynSub);
            break;
        case TokenType::Star:
            PUSH_CODE_SYMBOL(settings.StoreDebugSymbols, func, curToken);
            func.Code.EmplaceBack(InstructionCode::DynMul);
            break;
        case TokenType::Slash:
            PUSH_CODE_SYMBOL(settings.StoreDebugSymbols, func, curToken);
            func.Code.EmplaceBack(InstructionCode::DynDiv);
            break;
        case TokenType::Modulus:
            PUSH_CODE_SYMBOL(settings.StoreDebugSymbols, func, curToken);
            func.Code.EmplaceBack(InstructionCode::Mod);
            break;
        case TokenType::BitAnd:
            PUSH_CODE_SYMBOL(settings.StoreDebugSymbols, func, curToken);
            func.Code.EmplaceBack(InstructionCode::BitAnd);
            break;
        case TokenType::BitOr:
            PUSH_CODE_SYMBOL(settings.StoreDebugSymbols, func, curToken);
            func.Code.EmplaceBack(InstructionCode::BitOr);
            break;
        case TokenType::BitNot:
            PUSH_CODE_SYMBOL(settings.StoreDebugSymbols, func, curToken);
            func.Code.EmplaceBack(InstructionCode::BitNot);
            break;
        case TokenType::BitXor:
            PUSH_CODE_SYMBOL(settings.StoreDebugSymbols, func, curToken);
            func.Code.EmplaceBack(InstructionCode::BitXor);
            break;
        case TokenType::LeftArrow:
        case TokenType::PushReference:
        case TokenType::OpenBracket:
        case TokenType::StringLiteral:
        case TokenType::IntegerLiteral:
        case TokenType::DoubleLiteral:
        case TokenType::Identifier: {
            auto res = PushLValue(module, func, scopedLocals, curToken, settings);
            if (res != ParseResult::OK)
                return res;
        } break;
        case TokenType::RightArrow:
        case TokenType::BothArrows: {
            bool copyIntoLocal = curToken.Type == TokenType::BothArrows;
            PUSH_CODE_SYMBOL(settings.StoreDebugSymbols, func, curToken);
            m_Lexer->NextToken();
            bool forceBinding = curToken.Type == TokenType::Negate;
            if (forceBinding) m_Lexer->NextToken();
            if (curToken.Type != TokenType::Identifier)
                return SetError(ParseResult::UnexpectedToken, curToken, "Expected identifier to create local binding.");

            int64_t localIdx = 0;
            if (forceBinding) {
                localIdx = scopedLocals.Size();
                scopedLocals.PushBack(curToken.StringVal);
            } else {
                localIdx = (int64_t)scopedLocals.Size()-1;
                for (; localIdx >= 0 && scopedLocals[(size_t)localIdx] != curToken.StringVal; localIdx--);
                if (localIdx < 0) {
                    int64_t globalIdx = (int64_t)module.Globals.Size()-1;
                    for (; globalIdx >= 0 && module.Globals[(size_t)globalIdx].Name != curToken.StringVal; globalIdx--);
                    if (globalIdx >= 0) {
                        if (module.Globals[(size_t)globalIdx].IsConstant)
                            return SetError(ParseResult::UnexpectedToken, curToken, "Trying to assign to constant global.");
                        func.Code.EmplaceBack(
                            copyIntoLocal
                                ? InstructionCode::CopyIntoGlobal
                                : InstructionCode::PopIntoGlobal,
                            globalIdx);
                        break;
                    }
                    localIdx = scopedLocals.Size();
                    scopedLocals.PushBack(curToken.StringVal);
                }
            }
            if (scopedLocals.Size() > func.LocalsCount)
                func.LocalsCount = scopedLocals.Size();
            func.Code.EmplaceBack(
                copyIntoLocal
                    ? InstructionCode::CopyIntoLocal
                    : InstructionCode::PopIntoLocal,
                localIdx);
        } break;
        case TokenType::OpenParenth: {
            m_Lexer->NextToken();
            bool isNative = curToken.Type == TokenType::Star;
            bool isInstruction = curToken.Type == TokenType::Negate;
            if (isNative || isInstruction) m_Lexer->NextToken();
            PUSH_CODE_SYMBOL(settings.StoreDebugSymbols, func, curToken);
            Token identToken = curToken;
            int64_t arg0 = 0;
            if (identToken.Type != TokenType::Identifier)
                return SetError(ParseResult::UnexpectedToken, identToken, "Expected function name for function call.");
            m_Lexer->NextToken();
            if (isInstruction && curToken.Type == TokenType::IntegerLiteral) {
                arg0 = curToken.IntegerVal;
                m_Lexer->NextToken();
            }
            if (curToken.Type != TokenType::CloseParenth)
                return SetError(ParseResult::UnexpectedToken, curToken, "Expected ')' to close function call.");
            
            if (isInstruction) {
                const auto instrDescIt = InstructionMappings.find(identToken.StringVal);
                if (instrDescIt == InstructionMappings.end())
                    return SetError(ParseResult::UsageOfUnknownInstruction, identToken, "Instruction does not exist.");
                const InstructionDescription& instrDesc = (*instrDescIt).second;
                if (instrDesc.MayFail)
                    PUSH_CODE_SYMBOL(settings.StoreDebugSymbols, func, identToken);
                func.Code.EmplaceBack(instrDesc.Code, arg0);
                break;
            } else if (isNative) {
                int64_t funcIdx = (int64_t)module.NativeBindings.Size()-1;
                for (; funcIdx >= 0 && module.NativeBindings[(size_t)funcIdx].Name != identToken.StringVal; funcIdx--);
                if (funcIdx < 0)
                    return SetError(ParseResult::UsageOfUndeclaredNativeFunction, identToken, "Native function not declared.");
                func.Code.EmplaceBack(InstructionCode::CallNative, funcIdx);
                break;
            } else if (identToken.StringVal == func.Name) {
                // Self-recursion
                func.Code.EmplaceBack(InstructionCode::Call, (int64_t)module.Functions.Size());
                break;
            }

            int64_t funcIdx = (int64_t)module.Functions.Size()-1;
            for (; funcIdx >= 0 && module.Functions[(size_t)funcIdx].Name != identToken.StringVal; funcIdx--);
            if (funcIdx < 0)
                return SetError(ParseResult::UsageOfUndeclaredFunction, identToken, "Function not declared.");
            func.Code.EmplaceBack(InstructionCode::Call, funcIdx);
        } break;
        case TokenType::KW_If: {
            PUSH_CODE_SYMBOL(settings.StoreDebugSymbols, func, curToken);
            auto res = ParseIfStatement(module, func, scopedLocals, skippableBlock, false, settings);
            if (res != ParseResult::OK)
                return res;
        } break;
        default:
            if (settings.AppendNotesToErrorMessage)
                return SetError(ParseResult::UnexpectedToken, curToken, "Expression expected.\nNote: You may have forgotten to return from function '" + func.Name + "' or end some block within it.");
            else return SetError(ParseResult::UnexpectedToken, curToken, "Expression expected.");
        }
    }
}

Pulsar::ParseResult Pulsar::Parser::ParseIfStatement(
    Module& module, FunctionDefinition& func,
    const LocalsBindings& locals, SkippableBlock* skippableBlock,
    bool isChained, const ParseSettings& settings)
{
    Token ifToken = m_Lexer->CurrentToken();
    Token comparisonToken(TokenType::None);
    InstructionCode jmpInstrCode = InstructionCode::JumpIfZero;
    // Whether the if condition is fully contained within the statement.
    bool isSelfContained = false;
    bool hasComparison = false;

    const Token& curToken = m_Lexer->NextToken();
    if (curToken.Type != TokenType::Colon) {
        hasComparison = true;
        switch (curToken.Type) {
        case TokenType::StringLiteral:
        case TokenType::IntegerLiteral:
        case TokenType::DoubleLiteral:
        case TokenType::Identifier: {
            isSelfContained = true;
            jmpInstrCode = InstructionCode::JumpIfNotZero;
            auto res = PushLValue(module, func, locals, curToken, settings);
            if (res != ParseResult::OK)
                return res;
            m_Lexer->NextToken();
        } break;
        default:
            break;
        }

        if (curToken.Type != TokenType::Colon) {
            switch (curToken.Type) {
            case TokenType::Equals:
                jmpInstrCode = InstructionCode::JumpIfNotZero;
                break;
            case TokenType::NotEquals:
                jmpInstrCode = InstructionCode::JumpIfZero;
                break;
            case TokenType::Less:
                jmpInstrCode = InstructionCode::JumpIfGreaterThanOrEqualToZero;
                break;
            case TokenType::LessOrEqual:
                jmpInstrCode = InstructionCode::JumpIfGreaterThanZero;
                break;
            case TokenType::More:
                jmpInstrCode = InstructionCode::JumpIfLessThanOrEqualToZero;
                break;
            case TokenType::MoreOrEqual:
                jmpInstrCode = InstructionCode::JumpIfLessThanZero;
                break;
            default:
                return SetError(ParseResult::UnexpectedToken, curToken, "Expected if body start ':' or comparison operator.");
            }

            comparisonToken = curToken;
            m_Lexer->NextToken();
            switch (curToken.Type) {
            case TokenType::StringLiteral:
            case TokenType::IntegerLiteral:
            case TokenType::DoubleLiteral:
            case TokenType::Identifier: {
                auto res = PushLValue(module, func, locals, curToken, settings);
                if (res != ParseResult::OK)
                    return res;
                m_Lexer->NextToken();
            } break;
            default:
                return SetError(ParseResult::UnexpectedToken, curToken, "Expected lvalue of type Integer, Double or Local after comparison operator.");
            }
        } else isSelfContained = false;
    }

    if (isChained && !isSelfContained)
        return SetError(ParseResult::Error, ifToken, "Chained if statement must have a self-contained condition.");

    if (curToken.Type != TokenType::Colon)
        return SetError(ParseResult::UnexpectedToken, curToken, "Expected ':' to begin if statement body.");
    else if (hasComparison) {
        PUSH_CODE_SYMBOL(settings.StoreDebugSymbols, func, ifToken);
        func.Code.EmplaceBack(InstructionCode::Compare);
    }

    PUSH_CODE_SYMBOL(settings.StoreDebugSymbols, func, ifToken);
    size_t ifIdx = func.Code.Size();
    func.Code.EmplaceBack(jmpInstrCode, 0);
    
    auto res = ParseFunctionBody(module, func, locals, skippableBlock, settings);
    func.Code[ifIdx].Arg0 = func.Code.Size() - ifIdx;
    if (res == ParseResult::UnexpectedToken) {
        switch (curToken.Type) {
        case TokenType::KW_End:
            ClearError();
            return ParseResult::OK;
        case TokenType::KW_Else:
            ClearError();
            break;
        default:
            return res;
        }
    } else if (res == ParseResult::OK) {
        if (!isSelfContained)
            return ParseResult::OK;
        m_Lexer->NextToken();
        switch (curToken.Type) {
        case TokenType::KW_End:
            return ParseResult::OK;
        case TokenType::KW_Else:
            break;
        default:
            return SetError(ParseResult::UnexpectedToken, curToken,
                "Expected 'end' to close or 'else' to create a new branch of a self-contained if statement.");
        }
    } else return res;

    size_t elseIdx = func.Code.Size();
    PUSH_CODE_SYMBOL(settings.StoreDebugSymbols, func, curToken);
    func.Code.EmplaceBack(InstructionCode::Jump, 0);
    func.Code[ifIdx].Arg0 = func.Code.Size() - ifIdx;
    m_Lexer->NextToken(); // Consume 'else' Token

    if (curToken.Type == TokenType::Colon) {
        res = ParseFunctionBody(module, func, locals, skippableBlock, settings);
        if (res == ParseResult::OK) {
            if (isSelfContained) {
                m_Lexer->NextToken();
                if (curToken.Type != TokenType::KW_End)
                    return SetError(ParseResult::UnexpectedToken, curToken, "Expected 'end' to close else branch.");
            }
        } else if (res != ParseResult::UnexpectedToken || curToken.Type != TokenType::KW_End)
            return res;

        ClearError();
        func.Code[elseIdx].Arg0 = func.Code.Size() - elseIdx;
        return ParseResult::OK;
    } else if (curToken.Type == TokenType::KW_If) {
        if (!isSelfContained)
            return SetError(ParseResult::UnexpectedToken, curToken, "Illegal 'else if' statement. Previous condition is not self-contained.");
        res = ParseIfStatement(module, func, locals, skippableBlock, true, settings);
        func.Code[elseIdx].Arg0 = func.Code.Size() - elseIdx;
        return res;
    }
    return SetError(ParseResult::UnexpectedToken, curToken, "Expected 'else' block start or 'else if' compound statement.");
}

Pulsar::ParseResult Pulsar::Parser::ParseWhileLoop(Module& module, FunctionDefinition& func, const LocalsBindings& locals, const ParseSettings& settings)
{
    const Token& curToken = m_Lexer->CurrentToken();
    if (curToken.Type != TokenType::KW_While)
        return SetError(ParseResult::UnexpectedToken, curToken, "Expected while loop");
    Token whileToken = curToken;
    Token comparisonToken(TokenType::None);
    InstructionCode jmpInstrCode = InstructionCode::JumpIfZero;
    bool hasComparison = false;
    bool whileTrue = false;

    size_t whileIdx = func.Code.Size();
    m_Lexer->NextToken();
    if (curToken.Type == TokenType::Colon) {
        whileTrue = true;
    } else {
        switch (curToken.Type) {
        case TokenType::StringLiteral:
        case TokenType::IntegerLiteral:
        case TokenType::DoubleLiteral:
        case TokenType::Identifier: {
            // jmpInstrCode = InstructionCode::JumpIfZero;
            auto res = PushLValue(module, func, locals, curToken, settings);
            if (res != ParseResult::OK)
                return res;
            m_Lexer->NextToken();
        } break;
        default:
            break;
        }

        if (curToken.Type != TokenType::Colon) {
            hasComparison = true;
            switch (curToken.Type) {
            case TokenType::Equals:
                jmpInstrCode = InstructionCode::JumpIfNotZero;
                break;
            case TokenType::NotEquals:
                jmpInstrCode = InstructionCode::JumpIfZero;
                break;
            case TokenType::Less:
                jmpInstrCode = InstructionCode::JumpIfGreaterThanOrEqualToZero;
                break;
            case TokenType::LessOrEqual:
                jmpInstrCode = InstructionCode::JumpIfGreaterThanZero;
                break;
            case TokenType::More:
                jmpInstrCode = InstructionCode::JumpIfLessThanOrEqualToZero;
                break;
            case TokenType::MoreOrEqual:
                jmpInstrCode = InstructionCode::JumpIfLessThanZero;
                break;
            default:
                return SetError(ParseResult::UnexpectedToken, curToken, "Expected while loop body start ':' or comparison operator.");
            }

            comparisonToken = curToken;
            m_Lexer->NextToken();
            switch (curToken.Type) {
            case TokenType::StringLiteral:
            case TokenType::IntegerLiteral:
            case TokenType::DoubleLiteral:
            case TokenType::Identifier: {
                auto res = PushLValue(module, func, locals, curToken, settings);
                if (res != ParseResult::OK)
                    return res;
                m_Lexer->NextToken();
            } break;
            default:
                return SetError(ParseResult::UnexpectedToken, curToken, "Expected lvalue of type String, Integer, Double or Local after comparison operator.");
            }
        }
    }

    if (curToken.Type != TokenType::Colon)
        return SetError(ParseResult::UnexpectedToken, curToken, "Expected ':' to begin while loop body.");
    else if (hasComparison) {
        PUSH_CODE_SYMBOL(settings.StoreDebugSymbols, func, comparisonToken);
        func.Code.EmplaceBack(InstructionCode::Compare);
    }

    SkippableBlock block{
        .AllowBreak = true,
        .AllowContinue = true,
    };

    if (!whileTrue) {
        block.BreakStatements.PushBack(func.Code.Size());
        PUSH_CODE_SYMBOL(settings.StoreDebugSymbols, func, whileToken);
        func.Code.EmplaceBack(jmpInstrCode, 0);
    }
    
    auto res = ParseFunctionBody(module, func, locals, &block, settings);
    if (res == ParseResult::OK) {
        m_Lexer->NextToken();
        if (curToken.Type != TokenType::KW_End)
            return SetError(ParseResult::UnexpectedToken, curToken, "Expected 'end' to close while loop body.");
    } else if (res != ParseResult::UnexpectedToken || curToken.Type != TokenType::KW_End)
        return res;
    ClearError();
    
    for (size_t i = 0; i < block.ContinueStatements.Size(); i++)
        func.Code[block.ContinueStatements[i]].Arg0 = (int64_t)(whileIdx-block.ContinueStatements[i]);

    size_t endIdx = func.Code.Size();
    func.Code.EmplaceBack(InstructionCode::Jump, (int64_t)(whileIdx-endIdx));

    size_t breakIdx = func.Code.Size();
    for (size_t i = 0; i < block.BreakStatements.Size(); i++)
        func.Code[block.BreakStatements[i]].Arg0 = (int64_t)(breakIdx-block.BreakStatements[i]);
    return ParseResult::OK;
}

Pulsar::ParseResult Pulsar::Parser::ParseDoBlock(Module& module, FunctionDefinition& func, const LocalsBindings& locals, const ParseSettings& settings)
{
    const Token& curToken = m_Lexer->CurrentToken();
    if (curToken.Type != TokenType::KW_Do)
        return SetError(ParseResult::UnexpectedToken, curToken, "Expected do block.");
    Token doToken = curToken;

    size_t doIdx = func.Code.Size();
    m_Lexer->NextToken();
    if (curToken.Type != TokenType::Colon)
        return SetError(ParseResult::UnexpectedToken, curToken, "Expected ':' to begin do block body.");

    SkippableBlock block{
        .AllowBreak = true,
        .AllowContinue = true,
    };

    auto res = ParseFunctionBody(module, func, locals, &block, settings);
    if (res == ParseResult::OK) {
        m_Lexer->NextToken();
        if (curToken.Type != TokenType::KW_End)
            return SetError(ParseResult::UnexpectedToken, curToken, "Expected 'end' to close do block body.");
    } else if (res != ParseResult::UnexpectedToken || curToken.Type != TokenType::KW_End)
        return res;
    ClearError();
    
    for (size_t i = 0; i < block.ContinueStatements.Size(); i++)
        func.Code[block.ContinueStatements[i]].Arg0 = (int64_t)(doIdx-block.ContinueStatements[i]);

    size_t breakIdx = func.Code.Size();
    for (size_t i = 0; i < block.BreakStatements.Size(); i++)
        func.Code[block.BreakStatements[i]].Arg0 = (int64_t)(breakIdx-block.BreakStatements[i]);
    return ParseResult::OK;
}

Pulsar::ParseResult Pulsar::Parser::PushLValue(Module& module, FunctionDefinition& func, const LocalsBindings& locals, const Token& lvalue, const ParseSettings& settings)
{
    switch (lvalue.Type) {
    case TokenType::IntegerLiteral:
        func.Code.EmplaceBack(InstructionCode::PushInt, lvalue.IntegerVal);
        break;
    case TokenType::DoubleLiteral: {
        static_assert(sizeof(double) == sizeof(int64_t));
        void* val = (void*)&lvalue.DoubleVal;
        int64_t arg0 = *(int64_t*)val;
        func.Code.EmplaceBack(InstructionCode::PushDbl, arg0);
        // Don't want to rely on std::bit_cast since my g++ does not have it.
    } break;
    case TokenType::Identifier: {
        PUSH_CODE_SYMBOL(settings.StoreDebugSymbols, func, lvalue);
        int64_t localIdx = (int64_t)locals.Size()-1;
        for (; localIdx >= 0 && locals[(size_t)localIdx] != lvalue.StringVal; localIdx--);
        if (localIdx < 0) {
            int64_t globalIdx = (int64_t)module.Globals.Size()-1;
            for (; globalIdx >= 0 && module.Globals[(size_t)globalIdx].Name != lvalue.StringVal; globalIdx--);
            if (globalIdx < 0)
                return SetError(ParseResult::UsageOfUndeclaredLocal, lvalue, "Local not declared.");
            func.Code.EmplaceBack(InstructionCode::PushGlobal, globalIdx);
            break;
        }
        func.Code.EmplaceBack(InstructionCode::PushLocal, localIdx);
    } break;
    case TokenType::StringLiteral: {
        PUSH_CODE_SYMBOL(settings.StoreDebugSymbols, func, lvalue);
        int64_t constIdx = (int64_t)module.Constants.Size()-1;
        Value constVal;
        constVal.SetString(lvalue.StringVal);
        for (; constIdx >= 0 && module.Constants[(size_t)constIdx] != constVal; constIdx--);
        if (constIdx < 0) {
            constIdx = module.Constants.Size();
            module.Constants.EmplaceBack(std::move(constVal));
        }
        func.Code.EmplaceBack(InstructionCode::PushConst, constIdx);
    } break;
    case TokenType::PushReference: {
        const Token& curToken = m_Lexer->NextToken();
        if (curToken.Type == TokenType::Identifier) {
            return SetError(ParseResult::UnexpectedToken, curToken, "Local reference is not supported, expected (function).");
        } else if (curToken.Type == TokenType::OpenParenth) {
            m_Lexer->NextToken();
            bool isNative = curToken.Type == TokenType::Star;
            if (isNative) m_Lexer->NextToken();
            if (curToken.Type != TokenType::Identifier)
                return SetError(ParseResult::UnexpectedToken, curToken, "Expected (function) name.");
            Token identToken = curToken;
            m_Lexer->NextToken();
            if (curToken.Type != TokenType::CloseParenth)
                return SetError(ParseResult::UnexpectedToken, curToken, "Expected ')' to close function reference.");
        
            if (isNative) {
                int64_t funcIdx = (int64_t)module.NativeBindings.Size()-1;
                for (; funcIdx >= 0 && module.NativeBindings[(size_t)funcIdx].Name != identToken.StringVal; funcIdx--);
                if (funcIdx < 0)
                    return SetError(ParseResult::UsageOfUndeclaredNativeFunction, identToken, "Native function not declared.");
                func.Code.EmplaceBack(InstructionCode::PushNativeFunctionReference, funcIdx);
                break;
            } else if (identToken.StringVal == func.Name) {
                func.Code.EmplaceBack(InstructionCode::PushFunctionReference, (int64_t)module.Functions.Size());
                break;
            }

            int64_t funcIdx = (int64_t)module.Functions.Size()-1;
            for (; funcIdx >= 0 && module.Functions[(size_t)funcIdx].Name != identToken.StringVal; funcIdx--);
            if (funcIdx < 0)
                return SetError(ParseResult::UsageOfUndeclaredFunction, identToken, "Function not declared.");
            func.Code.EmplaceBack(InstructionCode::PushFunctionReference, funcIdx);
        } else return SetError(ParseResult::UnexpectedToken, curToken, "Expected (function) or local to reference.");
    } break;
    case TokenType::LeftArrow: {
        PUSH_CODE_SYMBOL(settings.StoreDebugSymbols, func, lvalue);
        const Token& curToken = m_Lexer->NextToken();
        if (curToken.Type != TokenType::Identifier)
            return SetError(ParseResult::UnexpectedToken, curToken, "Expected local name.");

        int64_t localIdx = (int64_t)locals.Size()-1;
        for (; localIdx >= 0 && locals[(size_t)localIdx] != curToken.StringVal; localIdx--);
        if (localIdx < 0) {
            int64_t globalIdx = (int64_t)module.Globals.Size()-1;
            for (; globalIdx >= 0 && module.Globals[(size_t)globalIdx].Name != curToken.StringVal; globalIdx--);
            if (globalIdx >= 0) {
                if (module.Globals[(size_t)globalIdx].IsConstant)
                    return SetError(ParseResult::WritingToConstantGlobal, curToken, "Cannot move constant global.");
                func.Code.EmplaceBack(InstructionCode::MoveGlobal, globalIdx);
                break;
            }
            return SetError(ParseResult::UsageOfUndeclaredLocal, curToken, "Local not declared.");
        }
        func.Code.EmplaceBack(InstructionCode::MoveLocal, localIdx);
    } break;
    case TokenType::OpenBracket: {
        /**
         * Given the following list:
         *   [ 1, 2, 3, foo, bar, "baz", 4, 5, 6 ]
         * Split the parts which have constant
         *   numeric values from the other ones:
         *   [ 1, 2, 3 ], [ foo, bar, "baz", ], [ 4, 5, 6 ]
         * - Now the ones that only contain numbers
         *   can be cached in module.Constants and Pushed.
         * - For the other values we must push them
         *   individually and then Append those values.
         * - After a const list is pushed we must concat
         *   it with the previous one.
         * - For lists inside lists we can just call recursively.
         *   And expect a list value to be appended.
         * PUSH []
         * CONCAT
         * PUSH [ 1, 2, 3, ]
         * APPEND foo
         * APPEND bar
         * APPEND "baz"
         * PUSH [ 4, 5, 6, ]
         * CONCAT
         */
        Value constList;
        constList.SetList(ValueList());
        const Token& curToken = m_Lexer->NextToken();
        func.Code.EmplaceBack(InstructionCode::PushEmptyList);
        while (true) {
            switch (curToken.Type) {
            case TokenType::LeftArrow:
            case TokenType::PushReference:
            case TokenType::StringLiteral:
            case TokenType::Identifier:
            case TokenType::OpenBracket:
                // Do not create a new list for consecutive non-const values.
                if (!constList.AsList().Front())
                    break;
                [[fallthrough]];
            case TokenType::CloseBracket: {
                if (!constList.AsList().Front())
                    return ParseResult::OK; // Empty List
                // Check for an already existing list with the same values.
                int64_t constIdx = (int64_t)module.Constants.Size()-1;
                for (; constIdx >= 0 && module.Constants[(size_t)constIdx] != constList; constIdx--);
                if (constIdx < 0) {
                    constIdx = module.Constants.Size();
                    module.Constants.EmplaceBack(constList);
                }
                constList.AsList().Clear();
                PUSH_CODE_SYMBOL(settings.StoreDebugSymbols, func, curToken);
                func.Code.EmplaceBack(InstructionCode::PushConst, constIdx);
                func.Code.EmplaceBack(InstructionCode::Concat);
                if (curToken.Type == TokenType::CloseBracket)
                    return ParseResult::OK;
            } break;
            case TokenType::IntegerLiteral:
                constList.AsList().Append()->Value().SetInteger(curToken.IntegerVal);
                break;
            case TokenType::DoubleLiteral:
                constList.AsList().Append()->Value().SetDouble(curToken.DoubleVal);
                break;
            default:
                break;
            }

            switch (curToken.Type) {
            case TokenType::LeftArrow:
            case TokenType::PushReference:
            case TokenType::StringLiteral:
            case TokenType::Identifier:
            case TokenType::OpenBracket: {
                auto res = PushLValue(module, func, locals, curToken, settings);
                if (res != ParseResult::OK)
                    return res;
                PUSH_CODE_SYMBOL(settings.StoreDebugSymbols, func, curToken);
                func.Code.EmplaceBack(InstructionCode::Append);
            } break;
            case TokenType::IntegerLiteral:
            case TokenType::DoubleLiteral:
                break;
            default:
                return SetError(ParseResult::UnexpectedToken, curToken, "Expected lvalue.");
            }
            
            m_Lexer->NextToken();
            if (curToken.Type == TokenType::Comma) {
                m_Lexer->NextToken();
            } else if (curToken.Type != TokenType::CloseBracket)
                return SetError(ParseResult::UnexpectedToken, curToken, "Expected ',' to continue List literal or ']' to close it.");
        }
    } break;
    default:
        return SetError(ParseResult::UnexpectedToken, lvalue, "Expected lvalue.");
    }
    return ParseResult::OK;
}

const char* Pulsar::ParseResultToString(ParseResult presult)
{
    switch (presult) {
    case ParseResult::OK:
        return "OK";
    case ParseResult::Error:
        return "Error";
    case ParseResult::FileNotRead:
        return "FileNotRead";
    case ParseResult::UnexpectedToken:
        return "UnexpectedToken";
    case ParseResult::NegativeResultCount:
        return "NegativeResultCount";
    case ParseResult::UsageOfUndeclaredLocal:
        return "UsageOfUndeclaredLocal";
    case ParseResult::UsageOfUnknownInstruction:
        return "UsageOfUnknownInstruction";
    case ParseResult::UsageOfUndeclaredFunction:
        return "UsageOfUndeclaredFunction";
    case ParseResult::UsageOfUndeclaredNativeFunction:
        return "UsageOfUndeclaredNativeFunction";
    case ParseResult::WritingToConstantGlobal:
        return "WritingToConstantGlobal";
    case ParseResult::GlobalEvaluationError:
        return "GlobalEvaluationError";
    case ParseResult::IllegalDirective:
        return "IllegalDirective";
    }
    return "Unknown";
}
