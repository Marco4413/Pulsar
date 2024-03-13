#include "pulsar-tools/bindings/lexer.h"

#include <filesystem>
#include <fstream>

#include "pulsar/parser.h"

void PulsarTools::LexerNativeBindings::BindToModule(Pulsar::Module& module)
{
    uint64_t type = module.BindCustomType("LexerHandle");
    module.BindNativeFunction({ "lexer/from-file", 1, 1 },
        [&, type](auto& ctx) { return Lexer_FromFile(ctx, type); });
    module.BindNativeFunction({ "lexer/next-token", 1, 2 },
        [&, type](auto& ctx) { return Lexer_NextToken(ctx, type); });
    module.BindNativeFunction({ "lexer/free!", 1, 0 },
        [&, type](auto& ctx) { return Lexer_Free(ctx, type); });
    module.BindNativeFunction({ "lexer/valid?", 1, 2 },
        [&, type](auto& ctx) { return Lexer_IsValid(ctx, type); });
}

Pulsar::RuntimeState PulsarTools::LexerNativeBindings::Lexer_FromFile(Pulsar::ExecutionContext& eContext, uint64_t type)
{
    Pulsar::Frame& frame = eContext.CallStack.CurrentFrame();
    Pulsar::Value& filePath = frame.Locals[0];
    if (filePath.Type() != Pulsar::ValueType::String)
        return Pulsar::RuntimeState::TypeError;

    if (!std::filesystem::exists(filePath.AsString().Data())) {
        frame.Stack.EmplaceBack()
            .SetCustom({ .Type=type, .Handle=0 });
        return Pulsar::RuntimeState::OK;
    }

    std::ifstream file(filePath.AsString().Data(), std::ios::binary);
    size_t fileSize = (size_t)std::filesystem::file_size(filePath.AsString().Data());

    Pulsar::String source;
    source.Resize(fileSize);
    if (!file.read((char*)source.Data(), fileSize)) {
        frame.Stack.EmplaceBack()
            .SetCustom({ .Type=type, .Handle=0 });
        return Pulsar::RuntimeState::OK;
    }

    int64_t handle = m_NextHandle++;
    frame.Stack.EmplaceBack()
        .SetCustom({ .Type=type, .Handle=handle });
    m_Lexers.Emplace(handle, std::move(source));
    return Pulsar::RuntimeState::OK;
}

Pulsar::RuntimeState PulsarTools::LexerNativeBindings::Lexer_NextToken(Pulsar::ExecutionContext& eContext, uint64_t type)
{
    Pulsar::Frame& frame = eContext.CallStack.CurrentFrame();
    Pulsar::Value& lexerHandle = frame.Locals[0];
    if (lexerHandle.Type() != Pulsar::ValueType::Custom
        || lexerHandle.AsCustom().Type != type)
        return Pulsar::RuntimeState::TypeError;

    auto handleLexerPair = m_Lexers.Find(lexerHandle.AsCustom().Handle);
    if (!handleLexerPair)
        return Pulsar::RuntimeState::Error;

    Pulsar::Lexer& lexer = *handleLexerPair.Value;
    Pulsar::Token token = lexer.NextToken();
    
    Pulsar::ValueList tokenAsList;
    tokenAsList.Append()->Value().SetInteger((int64_t)token.Type);
    tokenAsList.Append()->Value().SetString(Pulsar::TokenTypeToString(token.Type));
    switch (token.Type) {
    case Pulsar::TokenType::IntegerLiteral:
        tokenAsList.Append()->Value().SetInteger(token.IntegerVal);
        break;
    case Pulsar::TokenType::DoubleLiteral:
        tokenAsList.Append()->Value().SetDouble(token.DoubleVal);
        break;
    default:
        if (token.StringVal.Length() > 0)
            tokenAsList.Append()->Value().SetString(token.StringVal);
    }

    frame.Stack.EmplaceBack(lexerHandle);
    frame.Stack.EmplaceBack()
        .SetList(std::move(tokenAsList));

    return Pulsar::RuntimeState::OK;
}

Pulsar::RuntimeState PulsarTools::LexerNativeBindings::Lexer_Free(Pulsar::ExecutionContext& eContext, uint64_t type)
{
    Pulsar::Frame& frame = eContext.CallStack.CurrentFrame();
    Pulsar::Value& lexerHandle = frame.Locals[0];
    if (lexerHandle.Type() != Pulsar::ValueType::Custom
        || lexerHandle.AsCustom().Type != type)
        return Pulsar::RuntimeState::TypeError;
    m_Lexers.Remove(lexerHandle.AsCustom().Handle);
    return Pulsar::RuntimeState::OK;
}

Pulsar::RuntimeState PulsarTools::LexerNativeBindings::Lexer_IsValid(Pulsar::ExecutionContext& eContext, uint64_t type)
{
    Pulsar::Frame& frame = eContext.CallStack.CurrentFrame();
    Pulsar::Value& lexerHandle = frame.Locals[0];
    if (lexerHandle.Type() != Pulsar::ValueType::Custom
        || lexerHandle.AsCustom().Type != type)
        return Pulsar::RuntimeState::TypeError;
    frame.Stack.EmplaceBack(lexerHandle);
    frame.Stack.EmplaceBack()
        .SetInteger(lexerHandle.AsCustom().Handle);
    return Pulsar::RuntimeState::OK;
}
