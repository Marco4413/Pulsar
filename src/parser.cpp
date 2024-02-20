#include "pulsar/parser.h"

Pulsar::ParseResult Pulsar::Parser::ParseIntoModule(Module& module, bool debugSymbols)
{
    ParseResult result = ParseResult::OK;
    while (result == ParseResult::OK && !m_Lexer.IsEndOfFile())
        result = ParseFunctionDefinition(module, debugSymbols);
    module.NativeFunctions.resize(module.NativeBindings.size(), nullptr);
    return result;
}

Pulsar::ParseResult Pulsar::Parser::ParseFunctionDefinition(Module& module, bool debugSymbols)
{
    const Token& curToken = m_Lexer.NextToken();
    if (curToken.Type == TokenType::EndOfFile)
        return ParseResult::OK;
    else if (curToken.Type != TokenType::Star)
        return SetError(ParseResult::UnexpectedToken, curToken, "Expected '*' to begin function declaration.");

    m_Lexer.NextToken();
    if (curToken.Type != TokenType::OpenParenth)
        return SetError(ParseResult::UnexpectedToken, curToken, "Expected '(' to open function name and args declaration.");
    m_Lexer.NextToken();
    bool isNative = curToken.Type == TokenType::Star;
    if (isNative) m_Lexer.NextToken();

    if (curToken.Type != TokenType::Identifier)
        return SetError(ParseResult::UnexpectedToken, curToken, "Expected function identifier.");
    FunctionDefinition def = { curToken.StringVal, 0, 0 };
    if (debugSymbols) def.FunctionDebugSymbol = {curToken};

    m_Lexer.NextToken();
    LocalsBindings args;
    for (;;) {
        if (curToken.Type != TokenType::Identifier)
            break;
        args.push_back(std::move(curToken.StringVal));
        m_Lexer.NextToken();
    }
    def.Arity = args.size();
    def.LocalsCount = args.size();

    if (curToken.Type != TokenType::CloseParenth)
        return SetError(ParseResult::UnexpectedToken, curToken, "Expected ')' to close function name and args declaration.");
    m_Lexer.NextToken();
    if (curToken.Type == TokenType::RightArrow) {
        m_Lexer.NextToken();
        if (curToken.Type != TokenType::IntegerLiteral)
            return SetError(ParseResult::UnexpectedToken, curToken, "Expected return count.");
        else if (curToken.IntegerVal < 0)
            return SetError(ParseResult::NegativeResultCount, curToken, "Illegal return count. Return count must be >= 0");
        def.Returns = (size_t)curToken.IntegerVal;
        m_Lexer.NextToken();
    }

    if (isNative) {
        if (curToken.Type != TokenType::FullStop)
            return SetError(ParseResult::UnexpectedToken, curToken,
                "Expected '.' to confirm native function declaration. Native functions can't have a body.");
        module.NativeBindings.push_back(std::move(def));
    } else {
        if (curToken.Type != TokenType::Colon)
            return SetError(ParseResult::UnexpectedToken, curToken, "Expected '->' for return count declaration or ':' to begin function body.");
        ParseResult bodyParseResult = ParseFunctionBody(module, def, args, debugSymbols);
        if (bodyParseResult != ParseResult::OK)
            return bodyParseResult;
        module.Functions.push_back(std::move(def));
    }

    return ParseResult::OK;
}

#define PUSH_CODE_SYMBOL(cond, func, token) \
    if (cond) (func).CodeDebugSymbols.emplace_back((token), (func).Code.size())

Pulsar::ParseResult Pulsar::Parser::ParseFunctionBody(Module& module, FunctionDefinition& func, const LocalsBindings& locals, bool debugSymbols)
{
    LocalsBindings scopedLocals = locals;
    for (;;) {
        const Token& curToken = m_Lexer.NextToken();
        switch (curToken.Type) {
        case TokenType::FullStop:
            PUSH_CODE_SYMBOL(debugSymbols, func, curToken);
            func.Code.emplace_back(InstructionCode::Return);
            return ParseResult::OK;
        case TokenType::Plus:
            PUSH_CODE_SYMBOL(debugSymbols, func, curToken);
            func.Code.emplace_back(InstructionCode::DynSum);
            break;
        case TokenType::Minus:
            PUSH_CODE_SYMBOL(debugSymbols, func, curToken);
            func.Code.emplace_back(InstructionCode::DynSub);
            break;
        case TokenType::Star:
            PUSH_CODE_SYMBOL(debugSymbols, func, curToken);
            func.Code.emplace_back(InstructionCode::DynMul);
            break;
        case TokenType::Slash:
            PUSH_CODE_SYMBOL(debugSymbols, func, curToken);
            func.Code.emplace_back(InstructionCode::DynDiv);
            break;
        case TokenType::Modulus:
            PUSH_CODE_SYMBOL(debugSymbols, func, curToken);
            func.Code.emplace_back(InstructionCode::Mod);
            break;
        case TokenType::PushReference:
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
            m_Lexer.NextToken();
            bool forceBinding = curToken.Type == TokenType::Negate;
            if (forceBinding) m_Lexer.NextToken();
            if (curToken.Type != TokenType::Identifier)
                return SetError(ParseResult::UnexpectedToken, curToken, "Expected identifier to create local binding.");

            int64_t localIdx = 0;
            if (forceBinding) {
                localIdx = scopedLocals.size();
                scopedLocals.push_back(curToken.StringVal);
            } else {
                localIdx = (int64_t)scopedLocals.size()-1;
                for (; localIdx >= 0 && scopedLocals[localIdx] != curToken.StringVal; localIdx--);
                if (localIdx < 0) {
                    localIdx = scopedLocals.size();
                    scopedLocals.push_back(curToken.StringVal);
                }
            }
            if (scopedLocals.size() > func.LocalsCount)
                func.LocalsCount = scopedLocals.size();
            func.Code.emplace_back(copyIntoLocal ? InstructionCode::CopyIntoLocal : InstructionCode::PopIntoLocal, localIdx);
        } break;
        case TokenType::LeftArrow: {
            PUSH_CODE_SYMBOL(debugSymbols, func, curToken);
            m_Lexer.NextToken();
            if (curToken.Type != TokenType::Identifier)
                return SetError(ParseResult::UnexpectedToken, curToken, "Expected local name.");

            int64_t localIdx = (int64_t)scopedLocals.size()-1;
            for (; localIdx >= 0 && scopedLocals[localIdx] != curToken.StringVal; localIdx--);
            if (localIdx < 0)
                return SetError(ParseResult::UsageOfUndeclaredLocal, curToken, "Local not declared.");
            func.Code.emplace_back(InstructionCode::MoveLocal, localIdx);
        } break;
        case TokenType::OpenParenth: {
            m_Lexer.NextToken();
            bool isNative = curToken.Type == TokenType::Star;
            bool isInstruction = curToken.Type == TokenType::Negate;
            if (isNative || isInstruction) m_Lexer.NextToken();
            PUSH_CODE_SYMBOL(debugSymbols, func, curToken);
            Token identToken = curToken;
            int64_t arg0 = 0;
            if (identToken.Type != TokenType::Identifier)
                return SetError(ParseResult::UnexpectedToken, identToken, "Expected function name for function call.");
            m_Lexer.NextToken();
            if (isInstruction && curToken.Type == TokenType::IntegerLiteral) {
                arg0 = curToken.IntegerVal;
                m_Lexer.NextToken();
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
                func.Code.emplace_back(instrDesc.Code, arg0);
                break;
            } else if (isNative) {
                int64_t funcIdx = (int64_t)module.NativeBindings.size()-1;
                for (; funcIdx >= 0 && module.NativeBindings[funcIdx].Name != identToken.StringVal; funcIdx--);
                if (funcIdx < 0)
                    return SetError(ParseResult::UsageOfUndeclaredNativeFunction, identToken, "Native function not declared.");
                func.Code.emplace_back(InstructionCode::CallNative, funcIdx);
                break;
            } else if (identToken.StringVal == func.Name) {
                // Self-recursion
                func.Code.emplace_back(InstructionCode::Call, (int64_t)module.Functions.size());
                break;
            }

            int64_t funcIdx = (int64_t)module.Functions.size()-1;
            for (; funcIdx >= 0 && module.Functions[funcIdx].Name != identToken.StringVal; funcIdx--);
            if (funcIdx < 0)
                return SetError(ParseResult::UsageOfUndeclaredFunction, identToken, "Function not declared.");
            func.Code.emplace_back(InstructionCode::Call, funcIdx);
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
    Token ifToken = m_Lexer.CurrentToken();
    Token comparisonToken(TokenType::None);
    InstructionCode jmpInstrCode = InstructionCode::JumpIfZero;
    // Whether the if condition is fully contained within the statement.
    bool isSelfContained = false;
    bool hasComparison = false;

    const Token& curToken = m_Lexer.NextToken();
    if (curToken.Type != TokenType::Colon) {
        hasComparison = true;
        switch (curToken.Type) {
        case TokenType::IntegerLiteral:
        case TokenType::DoubleLiteral:
        case TokenType::Identifier: {
            isSelfContained = true;
            jmpInstrCode = InstructionCode::JumpIfNotZero;
            auto res = PushLValue(module, func, locals, curToken, debugSymbols);
            if (res != ParseResult::OK)
                return res;
            m_Lexer.NextToken();
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
            m_Lexer.NextToken();
            switch (curToken.Type) {
            case TokenType::IntegerLiteral:
            case TokenType::DoubleLiteral:
            case TokenType::Identifier: {
                auto res = PushLValue(module, func, locals, curToken, debugSymbols);
                if (res != ParseResult::OK)
                    return res;
                m_Lexer.NextToken();
            } break;
            default:
                return SetError(ParseResult::UnexpectedToken, curToken, "Expected lvalue after comparison operator.");
            }
        } else isSelfContained = false;
    }

    if (curToken.Type != TokenType::Colon)
        return SetError(ParseResult::UnexpectedToken, curToken, "Expected ':' to begin if statement body.");
    else if (hasComparison) {
        PUSH_CODE_SYMBOL(debugSymbols, func, ifToken);
        func.Code.emplace_back(InstructionCode::Compare);
    }

    PUSH_CODE_SYMBOL(debugSymbols, func, ifToken);
    size_t ifIdx = func.Code.size();
    func.Code.emplace_back(jmpInstrCode, 0);
    
    auto res = ParseFunctionBody(module, func, locals, debugSymbols);
    if (res != ParseResult::OK) {
        if (res != ParseResult::UnexpectedToken)
            return res;
        else if (curToken.Type == TokenType::KW_End) {
            func.Code[ifIdx].Arg0 = func.Code.size() - ifIdx;
            return ParseResult::OK;
        } else if (curToken.Type != TokenType::KW_Else)
            return SetError(ParseResult::UnexpectedToken, curToken, "'else' or 'end' expected.");
    } else {
        func.Code[ifIdx].Arg0 = func.Code.size() - ifIdx;
        return ParseResult::OK;
    }

    size_t elseIdx = func.Code.size();
    PUSH_CODE_SYMBOL(debugSymbols, func, curToken);
    func.Code.emplace_back(InstructionCode::Jump, 0);
    func.Code[ifIdx].Arg0 = func.Code.size() - ifIdx;
    m_Lexer.NextToken(); // Consume 'else' Token

    if (curToken.Type == TokenType::Colon) {
        res = ParseFunctionBody(module, func, locals, debugSymbols);
        if (res != ParseResult::OK) {
            if (res != ParseResult::UnexpectedToken)
                return res;
            const Token& curToken = m_Lexer.CurrentToken();
            if (curToken.Type != TokenType::KW_End)
                return SetError(ParseResult::UnexpectedToken, curToken, "Expected 'end'.");
        }

        func.Code[elseIdx].Arg0 = func.Code.size() - elseIdx;
        return ParseResult::OK;
    } else if (curToken.Type == TokenType::KW_If) {
        if (!isSelfContained)
            return SetError(ParseResult::UnexpectedToken, curToken, "Illegal 'else if' statement. Previous condition is not self-contained.");
        res = ParseIfStatement(module, func, locals, debugSymbols);
        func.Code[elseIdx].Arg0 = func.Code.size() - elseIdx;
        return res;
    }
    return SetError(ParseResult::UnexpectedToken, curToken, "Expected 'else' block start or 'else if' compound statement.");
}

Pulsar::ParseResult Pulsar::Parser::PushLValue(Module& module, FunctionDefinition& func, const LocalsBindings& locals, const Token& lvalue, bool debugSymbols)
{
    switch (lvalue.Type) {
    case TokenType::IntegerLiteral:
        func.Code.emplace_back(InstructionCode::PushInt, lvalue.IntegerVal);
        break;
    case TokenType::DoubleLiteral: {
        static_assert(sizeof(double) == sizeof(int64_t));
        void* val = (void*)&lvalue.DoubleVal;
        int64_t arg0 = *(int64_t*)val;
        func.Code.emplace_back(InstructionCode::PushDbl, arg0);
        // Don't want to rely on std::bit_cast since my g++ does not have it.
    } break;
    case TokenType::Identifier: {
        PUSH_CODE_SYMBOL(debugSymbols, func, lvalue);
        int64_t localIdx = (int64_t)locals.size()-1;
        for (; localIdx >= 0 && locals[localIdx] != lvalue.StringVal; localIdx--);
        if (localIdx < 0)
            return SetError(ParseResult::UsageOfUndeclaredLocal, lvalue, "Local not declared.");
        func.Code.emplace_back(InstructionCode::PushLocal, localIdx);
    } break;
    case TokenType::StringLiteral: {
        PUSH_CODE_SYMBOL(debugSymbols, func, lvalue);
        int64_t constIdx = (int64_t)module.Constants.size()-1;
        Value constVal;
        constVal.SetString(lvalue.StringVal);
        for (; constIdx >= 0 && module.Constants[constIdx] != constVal; constIdx--);
        if (constIdx < 0) {
            constIdx = module.Constants.size();
            module.Constants.emplace_back(std::move(constVal));
        }
        func.Code.emplace_back(InstructionCode::PushConst, constIdx);
    } break;
    case TokenType::PushReference: {
        const Token& curToken = m_Lexer.NextToken();
        if (curToken.Type == TokenType::Identifier) {
            return SetError(ParseResult::UnexpectedToken, curToken, "Local reference is not supported, expected (function).");
        } else if (curToken.Type == TokenType::OpenParenth) {
            m_Lexer.NextToken();
            bool isNative = curToken.Type == TokenType::Star;
            if (isNative) m_Lexer.NextToken();
            if (curToken.Type != TokenType::Identifier)
                return SetError(ParseResult::UnexpectedToken, curToken, "Expected (function) name.");
            Token identToken = curToken;
            m_Lexer.NextToken();
            if (curToken.Type != TokenType::CloseParenth)
                return SetError(ParseResult::UnexpectedToken, curToken, "Expected ')' to close function reference.");
        
            if (isNative) {
                int64_t funcIdx = (int64_t)module.NativeBindings.size()-1;
                for (; funcIdx >= 0 && module.NativeBindings[funcIdx].Name != identToken.StringVal; funcIdx--);
                if (funcIdx < 0)
                    return SetError(ParseResult::UsageOfUndeclaredNativeFunction, identToken, "Native function not declared.");
                func.Code.emplace_back(InstructionCode::PushNativeFunctionReference, funcIdx);
                break;
            } else if (identToken.StringVal == func.Name) {
                func.Code.emplace_back(InstructionCode::PushFunctionReference, (int64_t)module.Functions.size());
                break;
            }

            int64_t funcIdx = (int64_t)module.Functions.size()-1;
            for (; funcIdx >= 0 && module.Functions[funcIdx].Name != identToken.StringVal; funcIdx--);
            if (funcIdx < 0)
                return SetError(ParseResult::UsageOfUndeclaredFunction, identToken, "Function not declared.");
            func.Code.emplace_back(InstructionCode::PushFunctionReference, funcIdx);
        } else return SetError(ParseResult::UnexpectedToken, curToken, "Expected (function) or local to reference.");
    } break;
    default:
        // We should never get here
        return SetError(ParseResult::Error, lvalue, "Pulsar::Parser::PushLValue internal error.");
    }
    return ParseResult::OK;
}
