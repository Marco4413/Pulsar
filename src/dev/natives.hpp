#include <filesystem>
#include <fstream>

#include "pulsar/parser.h"

class ModuleNativeBindings
{
public:
    ModuleNativeBindings() = default;
    ~ModuleNativeBindings() = default;

    void BindToModule(Pulsar::Module& module)
    {
        uint64_t type = module.BindCustomType("ModuleHandle");
        module.BindNativeFunction({ "module/from-file", 1, 1 },
            [&, type](auto& ctx) { return Module_FromFile(ctx, type); });
        module.BindNativeFunction({ "module/run", 1, 2 },
            [&, type](auto& ctx) { return Module_Run(ctx, type); });
        module.BindNativeFunction({ "module/free!", 1, 0 },
            [&, type](auto& ctx) { return Module_Free(ctx, type); });
        module.BindNativeFunction({ "module/valid?", 1, 2 },
            [&, type](auto& ctx) { return Module_IsValid(ctx, type); });
    }

    Pulsar::RuntimeState Module_FromFile(Pulsar::ExecutionContext& eContext, uint64_t type)
    {
        Pulsar::Frame& frame = eContext.CallStack.CurrentFrame();
        Pulsar::Value& modulePath = frame.Locals[0];
        if (modulePath.Type() != Pulsar::ValueType::String)
            return Pulsar::RuntimeState::TypeError;

        Pulsar::Parser parser;
        auto result = parser.AddSourceFile(modulePath.AsString());
        if (result != Pulsar::ParseResult::OK) {
            frame.Stack.EmplaceBack()
                .SetCustom({ .Type=type, .Handle=0 });
            return Pulsar::RuntimeState::OK;
        }
        
        Pulsar::Module module;
        result = parser.ParseIntoModule(module, Pulsar::ParseSettings_Default);
        if (result != Pulsar::ParseResult::OK) {
            frame.Stack.EmplaceBack()
                .SetCustom({ .Type=type, .Handle=0 });
            return Pulsar::RuntimeState::OK;
        }
        
        int64_t handle = m_NextHandle++;
        frame.Stack.EmplaceBack()
            .SetCustom({ .Type=type, .Handle=handle });
        m_Modules[handle] = std::move(module);
        return Pulsar::RuntimeState::OK;
    }

    Pulsar::RuntimeState Module_Run(Pulsar::ExecutionContext& eContext, uint64_t type) const
    {
        Pulsar::Frame& frame = eContext.CallStack.CurrentFrame();
        Pulsar::Value& moduleHandle = frame.Locals[0];
        if (moduleHandle.Type() != Pulsar::ValueType::Custom
            || moduleHandle.AsCustom().Type != type)
            return Pulsar::RuntimeState::TypeError;

        const Pulsar::Module& module = (*m_Modules.find(moduleHandle.AsCustom().Handle)).second;
        Pulsar::ValueStack stack;
        Pulsar::ExecutionContext context = module.CreateExecutionContext();
        auto runtimeState = module.CallFunctionByName("main", stack, context);
        if (runtimeState != Pulsar::RuntimeState::OK)
            return Pulsar::RuntimeState::Error;

        frame.Stack.EmplaceBack(moduleHandle);
        Pulsar::ValueList retValues;
        for (size_t i = 0; i < stack.Size(); i++)
            retValues.Append()->Value() = std::move(stack[i]);
        frame.Stack.EmplaceBack()
            .SetList(std::move(retValues));
        return Pulsar::RuntimeState::OK;
    }

    Pulsar::RuntimeState Module_Free(Pulsar::ExecutionContext& eContext, uint64_t type)
    {
        Pulsar::Frame& frame = eContext.CallStack.CurrentFrame();
        Pulsar::Value& moduleHandle = frame.Locals[0];
        if (moduleHandle.Type() != Pulsar::ValueType::Custom
            || moduleHandle.AsCustom().Type != type)
            return Pulsar::RuntimeState::TypeError;

        m_Modules.erase(moduleHandle.AsCustom().Handle);
        return Pulsar::RuntimeState::OK;
    }

    Pulsar::RuntimeState Module_IsValid(Pulsar::ExecutionContext& eContext, uint64_t type)
    {
        Pulsar::Frame& frame = eContext.CallStack.CurrentFrame();
        Pulsar::Value& moduleHandle = frame.Locals[0];
        if (moduleHandle.Type() != Pulsar::ValueType::Custom
            || moduleHandle.AsCustom().Type != type)
            return Pulsar::RuntimeState::TypeError;
        frame.Stack.EmplaceBack(moduleHandle);
        frame.Stack.EmplaceBack()
            .SetInteger(moduleHandle.AsCustom().Handle);
        return Pulsar::RuntimeState::OK;
    }

private:
    int64_t m_NextHandle = 1;
    std::unordered_map<int64_t, Pulsar::Module> m_Modules;
};

class LexerNativeBindings
{
public:
    LexerNativeBindings() = default;
    ~LexerNativeBindings() = default;

    void BindToModule(Pulsar::Module& module)
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

    Pulsar::RuntimeState Lexer_FromFile(Pulsar::ExecutionContext& eContext, uint64_t type)
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
        m_Lexers.emplace(handle, std::move(source));
        return Pulsar::RuntimeState::OK;
    }

    Pulsar::RuntimeState Lexer_NextToken(Pulsar::ExecutionContext& eContext, uint64_t type)
    {
        Pulsar::Frame& frame = eContext.CallStack.CurrentFrame();
        Pulsar::Value& lexerHandle = frame.Locals[0];
        if (lexerHandle.Type() != Pulsar::ValueType::Custom
            || lexerHandle.AsCustom().Type != type)
            return Pulsar::RuntimeState::TypeError;

        Pulsar::Lexer& lexer = (*m_Lexers.find(lexerHandle.AsCustom().Handle)).second;
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

    Pulsar::RuntimeState Lexer_Free(Pulsar::ExecutionContext& eContext, uint64_t type)
    {
        Pulsar::Frame& frame = eContext.CallStack.CurrentFrame();
        Pulsar::Value& lexerHandle = frame.Locals[0];
        if (lexerHandle.Type() != Pulsar::ValueType::Custom
            || lexerHandle.AsCustom().Type != type)
            return Pulsar::RuntimeState::TypeError;
        m_Lexers.erase(lexerHandle.AsCustom().Handle);
        return Pulsar::RuntimeState::OK;
    }

    Pulsar::RuntimeState Lexer_IsValid(Pulsar::ExecutionContext& eContext, uint64_t type)
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

private:
    int64_t m_NextHandle = 1;
    std::unordered_map<int64_t, Pulsar::Lexer> m_Lexers;
};
