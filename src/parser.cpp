#include "pulsar/parser.h"

#define EXPECT_TOKEN_TYPE(token, type) \
    do {                                         \
        if ((token).Type != type)                \
            return SetError(                     \
                ParseResult::UnexpectedToken,    \
                (token),                         \
                "Unexpected token.");            \
    } while (false)

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
    Token starToken = m_Lexer.NextToken();
    if (starToken.Type == TokenType::EndOfFile)
        return ParseResult::OK;
    else EXPECT_TOKEN_TYPE(starToken, TokenType::Star);

    EXPECT_TOKEN_TYPE(m_Lexer.NextToken(), TokenType::OpenParenth);
    Token identToken = m_Lexer.NextToken();
    bool isNative = identToken.Type == TokenType::Star;
    if (isNative) identToken = m_Lexer.NextToken();
    EXPECT_TOKEN_TYPE(identToken, TokenType::Identifier);

    Token argToken = m_Lexer.NextToken();
    LocalsBindings args;
    for (;;) {
        if (argToken.Type != TokenType::Identifier)
            break;
        args.push_back(std::move(argToken.StringVal));
        argToken = m_Lexer.NextToken();
    }

    EXPECT_TOKEN_TYPE(argToken, TokenType::CloseParenth);
    size_t returnCount = 0;
    Token bodyStartToken = m_Lexer.NextToken();
    if (bodyStartToken.Type == TokenType::RightArrow) {
        Token returnCountToken = m_Lexer.NextToken();
        EXPECT_TOKEN_TYPE(returnCountToken, TokenType::IntegerLiteral);
        if (returnCountToken.IntegerVal < 0)
            return SetError(ParseResult::NegativeResultCount, returnCountToken, "Illegal return count. Return count must be >= 0");
        returnCount = (size_t)returnCountToken.IntegerVal;
        bodyStartToken = m_Lexer.NextToken();
    }

    FunctionDefinition def = { "", args.size(), returnCount };
    if (debugSymbols) def.FunctionDebugSymbol = {identToken};
    def.Name = std::move(identToken.StringVal);

    if (isNative) {
        EXPECT_TOKEN_TYPE(bodyStartToken, TokenType::FullStop);
        module.NativeBindings.push_back(std::move(def));
    } else {
        EXPECT_TOKEN_TYPE(bodyStartToken, TokenType::Colon);
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
            if (scopedLocals.size() > func.LocalsCount)
                func.LocalsCount = scopedLocals.size();
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
        case TokenType::IntegerLiteral:
        case TokenType::DoubleLiteral:
        case TokenType::Identifier: {
            auto res = PushLValue(module, func, scopedLocals, curToken, debugSymbols);
            if (res != ParseResult::OK)
                return res;
        } break;
        case TokenType::RightArrow: {
            PUSH_CODE_SYMBOL(debugSymbols, func, curToken);
            m_Lexer.NextToken();
            bool forceBinding = curToken.Type == TokenType::Negate;
            if (forceBinding) m_Lexer.NextToken();
            EXPECT_TOKEN_TYPE(curToken, TokenType::Identifier);

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
            func.Code.emplace_back(InstructionCode::PopIntoLocal, localIdx);
        } break;
        case TokenType::OpenParenth: {
            m_Lexer.NextToken();
            bool isNative = curToken.Type == TokenType::Star;
            if (isNative) m_Lexer.NextToken();
            PUSH_CODE_SYMBOL(debugSymbols, func, curToken);
            Token identToken = curToken;
            EXPECT_TOKEN_TYPE(identToken, TokenType::Identifier);
            EXPECT_TOKEN_TYPE(m_Lexer.NextToken(), TokenType::CloseParenth);
            
            if (isNative) {
                int64_t funcIdx = (int64_t)module.NativeBindings.size()-1;
                for (; funcIdx >= 0 && module.NativeBindings[funcIdx].Name != identToken.StringVal; funcIdx--);
                if (funcIdx < 0)
                    return SetError(ParseResult::UsageOfUndeclaredNativeFunction, identToken, "Native function not declared.");
                func.Code.emplace_back(InstructionCode::CallNative, funcIdx);
                break;
            } else if (identToken.StringVal == func.Name) {
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

    const Token& curToken = m_Lexer.NextToken();
    if (curToken.Type != TokenType::Colon) {
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
                return SetError(ParseResult::UnexpectedToken, curToken, "Expected block start or comparison operator.");
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

    EXPECT_TOKEN_TYPE(curToken, TokenType::Colon);
    if (comparisonToken.Type != TokenType::None) {
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
                return SetError(ParseResult::UnexpectedToken, curToken, "'end' expected.");
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
    (void) module;
    switch (lvalue.Type) {
    case TokenType::IntegerLiteral:
        func.Code.emplace_back(InstructionCode::PushInt, lvalue.IntegerVal);
        break;
    case TokenType::DoubleLiteral:
        func.Code.emplace_back(InstructionCode::PushDbl, *(int64_t*)&lvalue.DoubleVal);
        break;
    case TokenType::Identifier: {
        PUSH_CODE_SYMBOL(debugSymbols, func, lvalue);
        int64_t localIdx = (int64_t)locals.size()-1;
        for (; localIdx >= 0 && locals[localIdx] != lvalue.StringVal; localIdx--);
        if (localIdx < 0)
            return SetError(ParseResult::UsageOfUndeclaredLocal, lvalue, "Local not declared.");
        func.Code.emplace_back(InstructionCode::PushLocal, localIdx);
    } break;
    default:
        // We should never get here
        return ParseResult::UnexpectedToken;
    }
    return ParseResult::OK;
}
