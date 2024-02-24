#include "pulsar/parser.h"

#include <filesystem>
#include <fstream>

bool Pulsar::Parser::AddSource(const String& path, const String& src)
{
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

    String internalPath = relativePath.generic_string().c_str();
    if (!std::filesystem::exists(relativePath))
        return SetError(ParseResult::FileNotRead, token, "File '" + internalPath + "' does not exist.");
    
    std::ifstream file(relativePath, std::ios::binary);
    size_t fileSize = std::filesystem::file_size(relativePath);

    Pulsar::String source;
    source.Resize(fileSize);
    if (!file.read((char*)source.Data(), fileSize))
        return SetError(ParseResult::FileNotRead, token, "Could not read file '" + internalPath + "'.");

    AddSource(internalPath, std::move(source));
    return ParseResult::OK;
}

Pulsar::ParseResult Pulsar::Parser::ParseIntoModule(Module& module, bool debugSymbols)
{
    if (debugSymbols) {
        for (size_t i = 0; i < m_LexerPool.Size(); i++)
            module.SourceDebugSymbols.EmplaceBack(m_LexerPool[i].Path, m_LexerPool[i].Lexer.GetSource());
    }
    while (m_LexerPool.Size() > 0) {
        auto result = ParseModuleStatement(module, debugSymbols);
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

Pulsar::ParseResult Pulsar::Parser::ParseModuleStatement(Module& module, bool debugSymbols)
{
    const Token& curToken = m_Lexer->NextToken();
    switch (curToken.Type) {
    case TokenType::Star:
        return ParseFunctionDefinition(module, debugSymbols);
    case TokenType::CompilerDirective: {
        if (curToken.IntegerVal != TOKEN_CD_INCLUDE)
            return SetError(ParseResult::UnexpectedToken, curToken, "Unknown compiler directive.");
        m_Lexer->NextToken();
        if (curToken.Type != TokenType::StringLiteral)
            return SetError(ParseResult::UnexpectedToken, curToken, "Expected file path.");
        std::filesystem::path targetPath(curToken.StringVal.Data());
        std::filesystem::path workingPath(m_LexerPool.Back().Path.Data());
        std::filesystem::path filePath = workingPath.parent_path() / targetPath;
        auto result = AddSourceFile(filePath.generic_string().data());
        if (result != ParseResult::OK)
            return result;
        if (debugSymbols)
            module.SourceDebugSymbols.EmplaceBack(m_LexerPool.Back().Path, m_LexerPool.Back().Lexer.GetSource());
        return ParseResult::OK;
    }
    case TokenType::KW_Global:
        return ParseGlobalDefinition(module, debugSymbols);
    case TokenType::EndOfFile:
        return ParseResult::OK;
    default:
        return SetError(ParseResult::UnexpectedToken, curToken, "Expected function declaration or compiler directive.");
    }
}

Pulsar::ParseResult Pulsar::Parser::ParseGlobalDefinition(Module& module, bool debugSymbols)
{
    const Token& curToken = m_Lexer->CurrentToken();
    if (curToken.Type != TokenType::KW_Global)
        return SetError(ParseResult::UnexpectedToken, curToken, "Expected 'global' to begin global definition.");
    
    m_Lexer->NextToken();
    Token constToken = curToken;
    bool isConstant = constToken.Type == TokenType::KW_Const;
    if (isConstant) m_Lexer->NextToken();

    FunctionDefinition dummyFunc{"", 0, 1};
    auto result = PushLValue(module, dummyFunc, LocalsBindings(), curToken, false);
    if (result != ParseResult::OK)
        return result;
    m_Lexer->NextToken();
    
    if (curToken.Type != TokenType::RightArrow)
        return SetError(ParseResult::UnexpectedToken, curToken, "Expected '->' to assign global value.");
    m_Lexer->NextToken();
    if (curToken.Type != TokenType::Identifier)
        return SetError(ParseResult::UnexpectedToken, curToken, "Expected name for global.");
    dummyFunc.Name = curToken.StringVal;

    for (size_t i = 0; i < module.Globals.Size(); i++) {
        if (module.Globals[i].Name == curToken.StringVal) {
            if (module.Globals[i].IsConstant)
                return SetError(ParseResult::WritingToConstantGlobal, curToken, "Trying to reassign constant global.");
            else if (isConstant)
                return SetError(ParseResult::UnexpectedToken, constToken, "Redeclaring global as const.");
            break;
        }
    }

    auto stack = ValueStack();
    auto context = module.CreateExecutionContext();
    auto evalResult = module.ExecuteFunction(dummyFunc, stack, context);
    if (evalResult != RuntimeState::OK || stack.Size() == 0)
        return SetError(ParseResult::GlobalEvaluationError, curToken, "Error while evaluating value of global.");
    
    GlobalDefinition& globalDef = module.Globals.EmplaceBack(std::move(curToken.StringVal), std::move(stack.Back()), isConstant);
    if (debugSymbols) {
        const auto& lexSource = m_LexerPool.Back();
        size_t srcIdx = 0;
        for (; srcIdx < module.SourceDebugSymbols.Size() && module.SourceDebugSymbols[srcIdx].Path != lexSource.Path; srcIdx++);
        globalDef.DebugSymbol.Token = curToken;
        globalDef.DebugSymbol.SourceIdx = srcIdx;
    }

    return ParseResult::OK;
}

Pulsar::ParseResult Pulsar::Parser::ParseFunctionDefinition(Module& module, bool debugSymbols)
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
    if (debugSymbols) {
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
        module.NativeBindings.PushBack(std::move(def));
    } else {
        if (curToken.Type != TokenType::Colon)
            return SetError(ParseResult::UnexpectedToken, curToken, "Expected '->' for return count declaration or ':' to begin function body.");
        ParseResult bodyParseResult = ParseFunctionBody(module, def, args, debugSymbols);
        if (bodyParseResult != ParseResult::OK)
            return bodyParseResult;
        module.Functions.PushBack(std::move(def));
    }

    return ParseResult::OK;
}

#define PUSH_CODE_SYMBOL(cond, func, token) \
    if (cond) (func).CodeDebugSymbols.EmplaceBack((token), (func).Code.Size())

Pulsar::ParseResult Pulsar::Parser::ParseFunctionBody(Module& module, FunctionDefinition& func, const LocalsBindings& locals, bool debugSymbols)
{
    LocalsBindings scopedLocals = locals;
    for (;;) {
        const Token& curToken = m_Lexer->NextToken();
        switch (curToken.Type) {
        case TokenType::FullStop:
            PUSH_CODE_SYMBOL(debugSymbols, func, curToken);
            func.Code.EmplaceBack(InstructionCode::Return);
            return ParseResult::OK;
        case TokenType::Plus:
            PUSH_CODE_SYMBOL(debugSymbols, func, curToken);
            func.Code.EmplaceBack(InstructionCode::DynSum);
            break;
        case TokenType::Minus:
            PUSH_CODE_SYMBOL(debugSymbols, func, curToken);
            func.Code.EmplaceBack(InstructionCode::DynSub);
            break;
        case TokenType::Star:
            PUSH_CODE_SYMBOL(debugSymbols, func, curToken);
            func.Code.EmplaceBack(InstructionCode::DynMul);
            break;
        case TokenType::Slash:
            PUSH_CODE_SYMBOL(debugSymbols, func, curToken);
            func.Code.EmplaceBack(InstructionCode::DynDiv);
            break;
        case TokenType::Modulus:
            PUSH_CODE_SYMBOL(debugSymbols, func, curToken);
            func.Code.EmplaceBack(InstructionCode::Mod);
            break;
        case TokenType::PushReference:
        case TokenType::OpenBracket:
        case TokenType::StringLiteral:
        case TokenType::IntegerLiteral:
        case TokenType::DoubleLiteral:
        case TokenType::Identifier: {
            auto res = PushLValue(module, func, scopedLocals, curToken, debugSymbols);
            if (res != ParseResult::OK)
                return res;
        } break;
        case TokenType::RightArrow:
        case TokenType::BothArrows: {
            bool copyIntoLocal = curToken.Type == TokenType::BothArrows;
            PUSH_CODE_SYMBOL(debugSymbols, func, curToken);
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
                for (; localIdx >= 0 && scopedLocals[localIdx] != curToken.StringVal; localIdx--);
                if (localIdx < 0) {
                    int64_t globalIdx = (int64_t)module.Globals.Size()-1;
                    for (; globalIdx >= 0 && module.Globals[globalIdx].Name != curToken.StringVal; globalIdx--);
                    if (globalIdx >= 0) {
                        if (module.Globals[globalIdx].IsConstant)
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
        case TokenType::LeftArrow: {
            PUSH_CODE_SYMBOL(debugSymbols, func, curToken);
            m_Lexer->NextToken();
            if (curToken.Type != TokenType::Identifier)
                return SetError(ParseResult::UnexpectedToken, curToken, "Expected local name.");

            int64_t localIdx = (int64_t)scopedLocals.Size()-1;
            for (; localIdx >= 0 && scopedLocals[localIdx] != curToken.StringVal; localIdx--);
            if (localIdx < 0) {
                int64_t globalIdx = (int64_t)module.Globals.Size()-1;
                for (; globalIdx >= 0 && module.Globals[globalIdx].Name != curToken.StringVal; globalIdx--);
                if (globalIdx >= 0) {
                    if (module.Globals[globalIdx].IsConstant)
                        return SetError(ParseResult::WritingToConstantGlobal, curToken, "Cannot move constant global.");
                    func.Code.EmplaceBack(InstructionCode::MoveGlobal, globalIdx);
                    break;
                }
                return SetError(ParseResult::UsageOfUndeclaredLocal, curToken, "Local not declared.");
            }
            func.Code.EmplaceBack(InstructionCode::MoveLocal, localIdx);
        } break;
        case TokenType::OpenParenth: {
            m_Lexer->NextToken();
            bool isNative = curToken.Type == TokenType::Star;
            bool isInstruction = curToken.Type == TokenType::Negate;
            if (isNative || isInstruction) m_Lexer->NextToken();
            PUSH_CODE_SYMBOL(debugSymbols, func, curToken);
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
                    PUSH_CODE_SYMBOL(debugSymbols, func, identToken);
                func.Code.EmplaceBack(instrDesc.Code, arg0);
                break;
            } else if (isNative) {
                int64_t funcIdx = (int64_t)module.NativeBindings.Size()-1;
                for (; funcIdx >= 0 && module.NativeBindings[funcIdx].Name != identToken.StringVal; funcIdx--);
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
            for (; funcIdx >= 0 && module.Functions[funcIdx].Name != identToken.StringVal; funcIdx--);
            if (funcIdx < 0)
                return SetError(ParseResult::UsageOfUndeclaredFunction, identToken, "Function not declared.");
            func.Code.EmplaceBack(InstructionCode::Call, funcIdx);
        } break;
        case TokenType::KW_If: {
            PUSH_CODE_SYMBOL(debugSymbols, func, curToken);
            auto res = ParseIfStatement(module, func, scopedLocals, debugSymbols);
            if (res != ParseResult::OK)
                return res;
        } break;
        default:
            return SetError(ParseResult::UnexpectedToken, curToken, "Expression expected.");
        }
    }
}

Pulsar::ParseResult Pulsar::Parser::ParseIfStatement(Module& module, FunctionDefinition& func, const LocalsBindings& locals, bool debugSymbols)
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
            auto res = PushLValue(module, func, locals, curToken, debugSymbols);
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
            case TokenType::IntegerLiteral:
            case TokenType::DoubleLiteral:
            case TokenType::Identifier: {
                auto res = PushLValue(module, func, locals, curToken, debugSymbols);
                if (res != ParseResult::OK)
                    return res;
                m_Lexer->NextToken();
            } break;
            default:
                return SetError(ParseResult::UnexpectedToken, curToken, "Expected lvalue of type Integer, Double or Local after comparison operator.");
            }
        } else isSelfContained = false;
    }

    if (curToken.Type != TokenType::Colon)
        return SetError(ParseResult::UnexpectedToken, curToken, "Expected ':' to begin if statement body.");
    else if (hasComparison) {
        PUSH_CODE_SYMBOL(debugSymbols, func, ifToken);
        func.Code.EmplaceBack(InstructionCode::Compare);
    }

    PUSH_CODE_SYMBOL(debugSymbols, func, ifToken);
    size_t ifIdx = func.Code.Size();
    func.Code.EmplaceBack(jmpInstrCode, 0);
    
    auto res = ParseFunctionBody(module, func, locals, debugSymbols);
    if (res != ParseResult::OK) {
        if (res != ParseResult::UnexpectedToken)
            return res;
        else if (curToken.Type == TokenType::KW_End) {
            func.Code[ifIdx].Arg0 = func.Code.Size() - ifIdx;
            return ParseResult::OK;
        } else if (curToken.Type != TokenType::KW_Else)
            return SetError(ParseResult::UnexpectedToken, curToken, "'else' or 'end' expected.");
    } else {
        func.Code[ifIdx].Arg0 = func.Code.Size() - ifIdx;
        return ParseResult::OK;
    }

    size_t elseIdx = func.Code.Size();
    PUSH_CODE_SYMBOL(debugSymbols, func, curToken);
    func.Code.EmplaceBack(InstructionCode::Jump, 0);
    func.Code[ifIdx].Arg0 = func.Code.Size() - ifIdx;
    m_Lexer->NextToken(); // Consume 'else' Token

    if (curToken.Type == TokenType::Colon) {
        res = ParseFunctionBody(module, func, locals, debugSymbols);
        if (res != ParseResult::OK) {
            if (res != ParseResult::UnexpectedToken)
                return res;
            const Token& curToken = m_Lexer->CurrentToken();
            if (curToken.Type != TokenType::KW_End)
                return SetError(ParseResult::UnexpectedToken, curToken, "Expected 'end'.");
        }

        func.Code[elseIdx].Arg0 = func.Code.Size() - elseIdx;
        return ParseResult::OK;
    } else if (curToken.Type == TokenType::KW_If) {
        if (!isSelfContained)
            return SetError(ParseResult::UnexpectedToken, curToken, "Illegal 'else if' statement. Previous condition is not self-contained.");
        res = ParseIfStatement(module, func, locals, debugSymbols);
        func.Code[elseIdx].Arg0 = func.Code.Size() - elseIdx;
        return res;
    }
    return SetError(ParseResult::UnexpectedToken, curToken, "Expected 'else' block start or 'else if' compound statement.");
}

Pulsar::ParseResult Pulsar::Parser::PushLValue(Module& module, FunctionDefinition& func, const LocalsBindings& locals, const Token& lvalue, bool debugSymbols)
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
        PUSH_CODE_SYMBOL(debugSymbols, func, lvalue);
        int64_t localIdx = (int64_t)locals.Size()-1;
        for (; localIdx >= 0 && locals[localIdx] != lvalue.StringVal; localIdx--);
        if (localIdx < 0) {
            int64_t globalIdx = (int64_t)module.Globals.Size()-1;
            for (; globalIdx >= 0 && module.Globals[globalIdx].Name != lvalue.StringVal; globalIdx--);
            if (globalIdx < 0)
                return SetError(ParseResult::UsageOfUndeclaredLocal, lvalue, "Local not declared.");
            func.Code.EmplaceBack(InstructionCode::PushGlobal, globalIdx);
            break;
        }
        func.Code.EmplaceBack(InstructionCode::PushLocal, localIdx);
    } break;
    case TokenType::StringLiteral: {
        PUSH_CODE_SYMBOL(debugSymbols, func, lvalue);
        int64_t constIdx = (int64_t)module.Constants.Size()-1;
        Value constVal;
        constVal.SetString(lvalue.StringVal);
        for (; constIdx >= 0 && module.Constants[constIdx] != constVal; constIdx--);
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
                for (; funcIdx >= 0 && module.NativeBindings[funcIdx].Name != identToken.StringVal; funcIdx--);
                if (funcIdx < 0)
                    return SetError(ParseResult::UsageOfUndeclaredNativeFunction, identToken, "Native function not declared.");
                func.Code.EmplaceBack(InstructionCode::PushNativeFunctionReference, funcIdx);
                break;
            } else if (identToken.StringVal == func.Name) {
                func.Code.EmplaceBack(InstructionCode::PushFunctionReference, (int64_t)module.Functions.Size());
                break;
            }

            int64_t funcIdx = (int64_t)module.Functions.Size()-1;
            for (; funcIdx >= 0 && module.Functions[funcIdx].Name != identToken.StringVal; funcIdx--);
            if (funcIdx < 0)
                return SetError(ParseResult::UsageOfUndeclaredFunction, identToken, "Function not declared.");
            func.Code.EmplaceBack(InstructionCode::PushFunctionReference, funcIdx);
        } else return SetError(ParseResult::UnexpectedToken, curToken, "Expected (function) or local to reference.");
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
                for (; constIdx >= 0 && module.Constants[constIdx] != constList; constIdx--);
                if (constIdx < 0) {
                    constIdx = module.Constants.Size();
                    module.Constants.EmplaceBack(constList);
                }
                constList.AsList().Clear();
                PUSH_CODE_SYMBOL(debugSymbols, func, curToken);
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
            case TokenType::PushReference:
            case TokenType::StringLiteral:
            case TokenType::Identifier:
            case TokenType::OpenBracket: {
                auto res = PushLValue(module, func, locals, curToken, debugSymbols);
                if (res != ParseResult::OK)
                    return res;
                PUSH_CODE_SYMBOL(debugSymbols, func, curToken);
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
