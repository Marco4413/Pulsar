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
        Token exprToken = m_Lexer.NextToken();
        switch (exprToken.Type) {
        case TokenType::FullStop:
            PUSH_CODE_SYMBOL(debugSymbols, func, exprToken);
            func.Code.emplace_back(InstructionCode::Return);
            if (scopedLocals.size() > func.LocalsCount)
                func.LocalsCount = scopedLocals.size();
            return ParseResult::OK;
        case TokenType::Plus:
            PUSH_CODE_SYMBOL(debugSymbols, func, exprToken);
            func.Code.emplace_back(InstructionCode::DynSum);
            break;
        case TokenType::Minus:
            PUSH_CODE_SYMBOL(debugSymbols, func, exprToken);
            func.Code.emplace_back(InstructionCode::DynSub);
            break;
        case TokenType::Star:
            PUSH_CODE_SYMBOL(debugSymbols, func, exprToken);
            func.Code.emplace_back(InstructionCode::DynMul);
            break;
        case TokenType::IntegerLiteral:
            func.Code.emplace_back(InstructionCode::PushInt, exprToken.IntegerVal);
            break;
        case TokenType::DoubleLiteral:
            func.Code.emplace_back(InstructionCode::PushDbl, *(int64_t*)&exprToken.DoubleVal);
            break;
        case TokenType::RightArrow: {
            PUSH_CODE_SYMBOL(debugSymbols, func, exprToken);
            Token identToken = m_Lexer.NextToken();
            bool forceBinding = identToken.Type == TokenType::Negate;
            if (forceBinding) identToken = m_Lexer.NextToken();
            EXPECT_TOKEN_TYPE(identToken, TokenType::Identifier);

            int64_t localIdx = 0;
            if (forceBinding) {
                localIdx = scopedLocals.size();
                scopedLocals.push_back(std::move(identToken.StringVal));
            } else {
                localIdx = (int64_t)scopedLocals.size()-1;
                for (; localIdx >= 0 && scopedLocals[localIdx] != identToken.StringVal; localIdx--);
                if (localIdx < 0) {
                    localIdx = scopedLocals.size();
                    scopedLocals.push_back(std::move(identToken.StringVal));
                }
            }
            func.Code.emplace_back(InstructionCode::PopIntoLocal, localIdx);
        } break;
        case TokenType::Identifier: {
            PUSH_CODE_SYMBOL(debugSymbols, func, exprToken);
            int64_t localIdx = (int64_t)scopedLocals.size()-1;
            for (; localIdx >= 0 && scopedLocals[localIdx] != exprToken.StringVal; localIdx--);
            if (localIdx < 0)
                return SetError(ParseResult::UsageOfUndeclaredLocal, exprToken, "Local not declared.");
            func.Code.emplace_back(InstructionCode::PushLocal, localIdx);
        } break;
        case TokenType::OpenParenth: {
            Token identToken = m_Lexer.NextToken();
            PUSH_CODE_SYMBOL(debugSymbols, func, identToken);

            bool isNative = identToken.Type == TokenType::Star;
            if (isNative) identToken = m_Lexer.NextToken();
            EXPECT_TOKEN_TYPE(identToken, TokenType::Identifier);
            EXPECT_TOKEN_TYPE(m_Lexer.NextToken(), TokenType::CloseParenth);
            
            if (isNative) {
                int64_t funcIdx = (int64_t)module.NativeBindings.size()-1;
                for (; funcIdx >= 0 && module.NativeBindings[funcIdx].Name != identToken.StringVal; funcIdx--);
                if (funcIdx < 0)
                    return SetError(ParseResult::UsageOfUndeclaredNativeFunction, exprToken, "Native function not declared.");
                func.Code.emplace_back(InstructionCode::CallNative, funcIdx);
                break;
            } else if (identToken.StringVal == func.Name) {
                func.Code.emplace_back(InstructionCode::Call, (int64_t)module.Functions.size());
                break;
            }

            int64_t funcIdx = (int64_t)module.Functions.size()-1;
            for (; funcIdx >= 0 && module.Functions[funcIdx].Name != identToken.StringVal; funcIdx--);
            if (funcIdx < 0)
                return SetError(ParseResult::UsageOfUndeclaredFunction, exprToken, "Function not declared.");
            func.Code.emplace_back(InstructionCode::Call, funcIdx);
        } break;
        case TokenType::KW_If: {
            PUSH_CODE_SYMBOL(debugSymbols, func, exprToken);
            auto res = ParseIfStatement(module, func, scopedLocals, debugSymbols);
            if (res != ParseResult::OK)
                return res;
        } break;
        default:
            return SetError(ParseResult::UnexpectedToken, exprToken, "Expression expected.");
        }
    }
}

Pulsar::ParseResult Pulsar::Parser::ParseIfStatement(Module& module, FunctionDefinition& func, const LocalsBindings& locals, bool debugSymbols)
{
    Token ifToken = m_Lexer.CurrentToken();

    InstructionCode jmpInstrCode = InstructionCode::JumpIfZero;
    Token conditionToken(TokenType::None);
    Token bodyStartToken = m_Lexer.NextToken();

    switch (bodyStartToken.Type) {
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
    case TokenType::Equals:
    case TokenType::Identifier:
    case TokenType::DoubleLiteral:
    case TokenType::IntegerLiteral:
        jmpInstrCode = InstructionCode::JumpIfNotZero;
        break;
    default:
        break;
    }

    switch (bodyStartToken.Type) {
    case TokenType::NotEquals:
    case TokenType::Equals:
    case TokenType::Less:
    case TokenType::LessOrEqual:
    case TokenType::More:
    case TokenType::MoreOrEqual:
        conditionToken = m_Lexer.NextToken();
        bodyStartToken = m_Lexer.NextToken();
        break;
    case TokenType::Identifier:
    case TokenType::DoubleLiteral:
    case TokenType::IntegerLiteral:
        conditionToken = bodyStartToken;
        bodyStartToken = m_Lexer.NextToken();
        break;
    default:
        break;
    }

    switch (conditionToken.Type) {
    case TokenType::Identifier: {
        PUSH_CODE_SYMBOL(debugSymbols, func, conditionToken);
        int64_t localIdx = (int64_t)locals.size()-1;
        for (; localIdx >= 0 && locals[localIdx] != conditionToken.StringVal; localIdx--);
        if (localIdx < 0)
            return SetError(ParseResult::UsageOfUndeclaredLocal, conditionToken, "Local not declared.");
        func.Code.emplace_back(InstructionCode::PushLocal, localIdx);
    } break;
    case TokenType::DoubleLiteral:
        func.Code.emplace_back(InstructionCode::PushDbl, *(int64_t*)&conditionToken.DoubleVal);
        break;
    case TokenType::IntegerLiteral:
        func.Code.emplace_back(InstructionCode::PushInt, conditionToken.IntegerVal);
        break;
    case TokenType::None:
        break;
    default:
        return SetError(ParseResult::UnexpectedToken, conditionToken, "Identifier or numeric literal expected.");
    }

    if (conditionToken.Type != TokenType::None) {
        PUSH_CODE_SYMBOL(debugSymbols, func, ifToken);
        func.Code.emplace_back(InstructionCode::Compare);
    }
    EXPECT_TOKEN_TYPE(bodyStartToken, TokenType::Colon);

    PUSH_CODE_SYMBOL(debugSymbols, func, ifToken);
    size_t ifIdx = func.Code.size();
    func.Code.emplace_back(jmpInstrCode, 0);
    
    auto res = ParseFunctionBody(module, func, locals, debugSymbols);
    if (res != ParseResult::OK) {
        if (res != ParseResult::UnexpectedToken)
            return res;
        const Token& curToken = m_Lexer.CurrentToken();
        if (curToken.Type == TokenType::KW_End) {
            func.Code[ifIdx].Arg0 = func.Code.size() - ifIdx;
            return ParseResult::OK;
        } else if (curToken.Type != TokenType::KW_Else)
            return SetError(ParseResult::UnexpectedToken, curToken, "'else' or 'end' expected.");
    } else {
        func.Code[ifIdx].Arg0 = func.Code.size() - ifIdx;
        return ParseResult::OK;
    }

    PUSH_CODE_SYMBOL(debugSymbols, func, m_Lexer.CurrentToken());
    size_t elseIdx = func.Code.size();
    func.Code.emplace_back(InstructionCode::Jump, 0);
    func.Code[ifIdx].Arg0 = func.Code.size() - ifIdx;

    EXPECT_TOKEN_TYPE(m_Lexer.NextToken(), TokenType::Colon);
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
}
