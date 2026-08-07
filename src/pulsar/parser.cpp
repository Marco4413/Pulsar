#include "pulsar/parser.h"

#ifndef PULSAR_NO_FILESYSTEM
#include <filesystem>
#include <fstream>
#endif // PULSAR_NO_FILESYSTEM

#define NOTIFY_SEND_BLOCK_NOTIFICATION(notificationType, fnDef, localScope, settings) \
    do {                                                                              \
        const Pulsar::String* filePath = this->CurrentPath();                         \
        const auto& callback = (settings).Notifications.OnBlockNotification;          \
        if (filePath && callback) {                                                   \
            if (callback({(notificationType), CurrentToken().SourcePos,               \
                         *filePath, (fnDef), (localScope)})) {                        \
                return SetError(                                                      \
                    Pulsar::ParseResult::TerminatedByNotification,                    \
                    CurrentToken(), "OnBlockNotification requested termination.");    \
        }}                                                                            \
    } while (0)

#define NOTIFY_IDENTIFIER_USAGE(usageType, boundIdx, fnDef, token, localScope, settings) \
    do {                                                                                 \
        const Pulsar::String* filePath = this->CurrentPath();                            \
        const auto& callback = (settings).Notifications.OnIdentifierUsage;               \
        if (filePath && callback) {                                                      \
            if (callback({(usageType), (size_t)(boundIdx),                               \
                         *filePath, (fnDef), (token), (localScope)})) {                  \
                return SetError(                                                         \
                    Pulsar::ParseResult::TerminatedByNotification,                       \
                    CurrentToken(), "OnIdentifierUsage requested termination.");         \
        }}                                                                               \
    } while (0)

#define NOTIFY_FUNCTION_DEFINITION(isRedecl, isNative, idx, fnDef, ident, args, settings) \
    do {                                                                                  \
        const Pulsar::String* filePath = this->CurrentPath();                             \
        const auto& callback = (settings).Notifications.OnFunctionDefinition;             \
        if (filePath && callback) {                                                       \
            if (callback({(isRedecl), (isNative), (size_t)(idx),                          \
                         *filePath, (fnDef), (ident), (args)})) {                         \
                return SetError(                                                          \
                    Pulsar::ParseResult::TerminatedByNotification,                        \
                    CurrentToken(), "OnFunctionDefinition requested termination.");       \
        }}                                                                                \
    } while (0)

void Pulsar::Parser::EmitWarning(ParseWarning reason, const Token& token, const String& message)
{
    WarningMessage warning;
    warning.SourceIndex = CurrentSourceIndex();
    warning.Token   = token;
    warning.Message = message;
    warning.Reason  = reason;
    m_WarningMessages.EmplaceBack(std::move(warning));
}

Pulsar::ParseResult Pulsar::Parser::SetError(ParseResult reason, const Token& token, const String& message)
{
    m_ErrorMessage.SourceIndex = CurrentSourceIndex();
    m_ErrorMessage.Token   = token;
    m_ErrorMessage.Message = message;
    m_ErrorMessage.Reason  = reason;
    return reason;
}

void Pulsar::Parser::ClearError()
{
    m_ErrorMessage.SourceIndex = INVALID_INDEX;
    m_ErrorMessage.Token  = Token(TokenType::None);
    m_ErrorMessage.Message.Resize(0);
    m_ErrorMessage.Reason = ParseResult::OK;
}

bool Pulsar::Parser::AddSource(const String& path, const String& src)
{
    return AddSource(path, String(src));
}

bool Pulsar::Parser::AddSource(const String& path, String&& src)
{
    ClearError();
    if (path.Length() > 0) {
        if (m_ParsedSources.Find(path))
            return false;
        m_ParsedSources.Emplace(path);
    }

    size_t sourceIndex = m_SourceDebugSymbols.Size();
    m_SourceDebugSymbols.EmplaceBack(path, std::move(src));
    // HACK: Reallocs to m_SourceDebugSymbols keep the lexer valid because
    //       it stores a reference to the const char* of the source.
    m_Lexers.EmplaceBack(sourceIndex, Lexer(m_SourceDebugSymbols[sourceIndex].Source), Token(TokenType::None));

    m_Lexers.Back().Lexer.SkipShaBang();
    ConsumeToken(); // Start Lexing
    return true;
}

Pulsar::ParseResult Pulsar::Parser::AddSourceFile(const String& path)
{
    Token token = CurrentToken();
#ifdef PULSAR_NO_FILESYSTEM
    return SetError(ParseResult::FileSystemNotAvailable, token, "Could not read '" + path + "' because filesystem was disabled.");
#else // PULSAR_NO_FILESYSTEM
    auto rawPath = std::filesystem::path(path.CString());

    String internalPath;
    // Having a method which normalizes the path removes duplicate code.
    // However, normalizedPath must be created again which is wasteful.
    if (!PathToNormalizedFileSystemPath(path, internalPath)) {
        return SetError(ParseResult::FileNotRead, token, "Could not resolve path '" + path + "'.");
    }

    std::filesystem::path normalizedPath(internalPath.CString());

    if (!std::filesystem::exists(normalizedPath))
        return SetError(ParseResult::FileNotRead, token, "File '" + internalPath + "' does not exist.");

    std::ifstream file(normalizedPath, std::ios::binary);

    std::error_code error;
    size_t fileSize = (size_t)std::filesystem::file_size(normalizedPath, error);
    if (error) {
        return SetError(ParseResult::FileNotRead, token, "Could not get size of file '" + internalPath + "'.");
    }

    Pulsar::String source;
    source.Resize(fileSize);
    if (!file.read(source.Data(), fileSize))
        return SetError(ParseResult::FileNotRead, token, "Could not read file '" + internalPath + "'.");

    AddSource(internalPath, std::move(source));
    return ParseResult::OK;
#endif // PULSAR_NO_FILESYSTEM
}

Pulsar::ParseResult Pulsar::Parser::ParseIntoModule(Module& module, const ParseSettings& settings)
{
    ClearError();
    GlobalScope globalScope;
    if (settings.StoreDebugSymbols) {
        for (size_t i = 0; i < m_SourceDebugSymbols.Size(); i++) {
            globalScope.SourceDebugSymbols.Emplace(m_SourceDebugSymbols[i].Path, i);
        }
    }
    for (size_t i = 0; i < module.Functions.Size(); i++)
        globalScope.Functions.Insert(module.Functions[i].Name, i);
    // This was commented out so that the file MUST declare used natives itself
    // for (size_t i = 0; i < module.NativeBindings.Size(); i++)
    //     globalScope.NativeFunctions.Insert(module.NativeBindings[i].Name, i);
    for (size_t i = 0; i < module.Globals.Size(); i++)
        globalScope.Globals.Insert(module.Globals[i].Name, i);

    while (m_Lexers.Size() > 0) {
        auto res = ParseModuleStatement(module, globalScope, settings);
        if (res != ParseResult::OK) return res;
        if (CurrentToken().Type == TokenType::EndOfFile)
            m_Lexers.PopBack();
    }

    module.NativeFunctions.Resize(module.NativeBindings.Size(), nullptr);

    if (settings.StoreDebugSymbols) {
        if (HasMessages()) {
            module.SourceDebugSymbols = m_SourceDebugSymbols;
        } else {
            module.SourceDebugSymbols = std::move(m_SourceDebugSymbols);
        }
    }

    StripUnusedSources();

    return ParseResult::OK;
}

Pulsar::ParseResult Pulsar::Parser::ParseModuleStatement(Module& module, GlobalScope& globalScope, const ParseSettings& settings)
{
    const Token& curToken = CurrentToken();
    switch (curToken.Type) {
    case TokenType::Star:
        return ParseFunctionDefinition(module, globalScope, settings);
    case TokenType::CompilerDirective: {
        if (curToken.IntegerVal != TOKEN_CD_INCLUDE)
            return SetError(ParseResult::UnexpectedToken, curToken, "Unknown compiler directive.");
        else if (!settings.AllowIncludeDirective)
            return SetError(ParseResult::IllegalDirective, curToken, "Include compiler directive was disabled.");

        ConsumeToken(); // CompilerDirective
        if (curToken.Type != TokenType::StringLiteral)
            return SetError(ParseResult::UnexpectedToken, curToken, "Expected file path.");

        Token pathToken;
        if (auto res = ParseStringLiteral(pathToken); res != ParseResult::OK)
            return res;

        if (settings.IncludeResolver) {
            const String* cwf = CurrentPath();
            PULSAR_ASSERT(cwf != nullptr, "CWF should not be nullptr.");
            auto res = settings.IncludeResolver(*this, *cwf, pathToken);
            if (res != ParseResult::OK)
                return res;
        } else {
#ifdef PULSAR_NO_FILESYSTEM
            return SetError(ParseResult::FileSystemNotAvailable, pathToken, "No custom include resolver provided.");
#else // PULSAR_NO_FILESYSTEM
            const String* cwf = CurrentPath();
            PULSAR_ASSERT(cwf != nullptr, "CWF should not be nullptr.");
            std::filesystem::path targetPath(pathToken.StringVal.CString());
            std::filesystem::path workingPath(cwf->CString());
            std::filesystem::path filePath = workingPath.parent_path() / targetPath;
            auto result = AddSourceFile(filePath.generic_string().data());
            if (result != ParseResult::OK)
                return result;
#endif // PULSAR_NO_FILESYSTEM
        }
        if (settings.StoreDebugSymbols) {
            // No error, a source was added
            globalScope.SourceDebugSymbols.Emplace(
                    m_SourceDebugSymbols.Back().Path,
                    m_SourceDebugSymbols.Size()-1);
        }
        return ParseResult::OK;
    }
    case TokenType::KW_Global:
        return ParseGlobalDefinition(module, globalScope, settings);
    case TokenType::EndOfFile:
        return ParseResult::OK;
    default:
        return SetError(ParseResult::UnexpectedToken, curToken, "Expected function declaration or compiler directive.");
    }
}

Pulsar::ParseResult Pulsar::Parser::ParseGlobalDefinition(Module& module, GlobalScope& globalScope, const ParseSettings& settings)
{
    const Token& curToken = CurrentToken();
    if (curToken.Type != TokenType::KW_Global)
        return SetError(ParseResult::UnexpectedToken, curToken, "Expected 'global' to begin global definition.");

    ConsumeToken(); // KW_Global
    Token constToken = curToken;
    bool isConstant = constToken.Type == TokenType::KW_Const;
    if (isConstant) ConsumeToken(); // KW_Const

    FunctionDefinition dummyFunc{
        .Name        = "",
        .Arity       = 0,
        .Returns     = 1,
        .StackArity  = 0,
        .LocalsCount = 0,
    };

    bool isProducer = curToken.Type == TokenType::RightArrow;

    if (!isProducer) {
        ParseSettings subSettings = settings;
        // We want to know where, within the body of the global, the runtime failed.
        subSettings.StoreDebugSymbols = true;
        LocalScope localScope{
            .Global = globalScope,
            .Function = nullptr,
        };
        auto res = ParseLValue(module, dummyFunc, localScope, subSettings);
        if (res != ParseResult::OK) return res;
    }

    if (curToken.Type != TokenType::RightArrow)
        return SetError(ParseResult::UnexpectedToken, curToken, "Expected '->' to assign global value.");

    ConsumeToken(); // RightArrow
    if (curToken.Type != TokenType::Identifier)
        return SetError(ParseResult::UnexpectedToken, curToken, "Expected name for global.");
    Token identToken = curToken;

    auto globalNameIdxPair = globalScope.Globals.Find(identToken.StringVal);
    if (globalNameIdxPair) {
        if (module.Globals[globalNameIdxPair->Value()].IsConstant)
            return SetError(ParseResult::WritingToConstantGlobal, identToken, "Trying to reassign constant global.");
        else if (isConstant)
            return SetError(ParseResult::UnexpectedToken, constToken, "Redeclaring global as const.");
    }

    ConsumeToken(); // Identifier
    if (isProducer) {
        if (curToken.Type != TokenType::Colon)
            return SetError(ParseResult::UnexpectedToken, curToken, "Expected ':' to begin global producer body.");
        ParseSettings subSettings = settings;
        // We want to know where, within the body of the global, the runtime failed.
        subSettings.StoreDebugSymbols = true;
        FunctionScope functionScope;
        LocalScope localScope{
            .Global = globalScope,
            .Function = &functionScope,
        };

        ConsumeToken(); // Colon

        Token endToken;
        auto result = ParseFunctionBody(module, dummyFunc, localScope, nullptr, &endToken, subSettings);
        if (result != ParseResult::OK)
            return result;
        if (endToken.Type != TokenType::FullStop)
            return SetError(ParseResult::UnexpectedToken, endToken, "You can only use the return operator to close a global producer.");
        result = BackPatchFunctionLabels(dummyFunc, functionScope);
        if (result != ParseResult::OK)
            return result;
    }

    // Assign name after ParseFunctionBody to prevent self-recursion
    dummyFunc.Name  = "{global/";
    dummyFunc.Name += identToken.StringVal;
    dummyFunc.Name += '}';

    ExecutionContext context(module);
    Stack& stack = context.GetStack();
    auto evalResult = RuntimeState::OK;

    if (isProducer && settings.MapGlobalProducersToVoid) {
        stack.Emplace();
    } else {
        context.CallFunction(dummyFunc);
        evalResult = context.Run();
    }

    if (evalResult != RuntimeState::OK) {
        size_t instrIdx = context.GetCallStack()[0].InstructionIndex;
        size_t symbolIdx = 0;
        for (size_t i = 0; i < dummyFunc.CodeDebugSymbols.Size(); i++) {
            if (dummyFunc.CodeDebugSymbols[i].StartIdx >= instrIdx)
                break;
            symbolIdx = i;
        }

        String errorMsg = String("Error while evaluating value of global (") + RuntimeStateToString(evalResult) + ").";
        if (settings.AppendStackTraceToErrorMessage)
            errorMsg += "\n" + context.GetStackTrace(settings.StackTraceMaxDepth);
        return SetError(
            ParseResult::GlobalEvaluationError,
            dummyFunc.CodeDebugSymbols[symbolIdx].Token,
            std::move(errorMsg));
    }

    PULSAR_ASSERT(stack.Size() > 0, "Global producer did not match return count.");

    GlobalDefinition* globalDef;
    if (!globalNameIdxPair) {
        globalScope.Globals.Emplace(identToken.StringVal, module.Globals.Size());
        globalDef = &module.Globals.EmplaceBack(std::move(identToken.StringVal), std::move(stack.Top()), isConstant);
    } else {
        globalDef = &module.Globals[globalNameIdxPair->Value()];
        globalDef->InitialValue = std::move(stack.Top());
    }

    if (settings.StoreDebugSymbols) {
        const String* path = CurrentPath();
        PULSAR_ASSERT(path != nullptr, "Path should not be nullptr.");
        auto sourcePathIdxPair = globalScope.SourceDebugSymbols.Find(*path);
        globalDef->DebugSymbol.Token = identToken;
        globalDef->DebugSymbol.SourceIdx = sourcePathIdxPair ? sourcePathIdxPair->Value() : ~(size_t)0;
    }

    return ParseResult::OK;
}

Pulsar::ParseResult Pulsar::Parser::ParseFunctionDefinition(Module& module, GlobalScope& globalScope, const ParseSettings& settings)
{
    const Token& curToken = CurrentToken();
    if (curToken.Type != TokenType::Star)
        return SetError(ParseResult::UnexpectedToken, curToken, "Expected '*' to begin function declaration.");

    ConsumeToken(); // Star
    if (curToken.Type != TokenType::OpenParenth)
        return SetError(ParseResult::UnexpectedToken, curToken, "Expected '(' to open function name and args declaration.");

    ConsumeToken(); // OpenParenth
    bool isNative = curToken.Type == TokenType::Star;
    if (isNative) ConsumeToken(); // Star

    if (curToken.Type != TokenType::Identifier)
        return SetError(ParseResult::UnexpectedToken, curToken, "Expected function identifier.");
    Token identToken = curToken;
    FunctionDefinition def{
        .Name        = identToken.StringVal,
        .Arity       = 0,
        .Returns     = 0,
        .StackArity  = 0,
        .LocalsCount = 0,
    };
    if (settings.StoreDebugSymbols) {
        const String* path = CurrentPath();
        PULSAR_ASSERT(path != nullptr, "Path should not be nullptr.");
        auto sourcePathIdxPair = globalScope.SourceDebugSymbols.Find(*path);
        def.DebugSymbol.Token = identToken;
        def.DebugSymbol.SourceIdx = sourcePathIdxPair ? sourcePathIdxPair->Value() : ~(size_t)0;
    }

    ConsumeToken(); // Identifier
    if (curToken.Type == TokenType::IntegerLiteral) {
        def.StackArity = (size_t)curToken.IntegerVal;
        ConsumeToken(); // IntegerLiteral
    }

    List<LocalScope::LocalVar> args;
    while (true) {
        if (curToken.Type != TokenType::Identifier) break;
        args.EmplaceBack(std::move(curToken.StringVal), curToken.SourcePos);
        ConsumeToken(); // Identifier
    }
    def.Arity = args.Size();
    def.LocalsCount = args.Size();

    if (curToken.Type != TokenType::CloseParenth)
        return SetError(ParseResult::UnexpectedToken, curToken, "Expected ')' to close function name and args declaration.");

    ConsumeToken(); // CloseParenth
    if (curToken.Type == TokenType::RightArrow) {
        ConsumeToken(); // RightArrow
        if (curToken.Type != TokenType::IntegerLiteral)
            return SetError(ParseResult::UnexpectedToken, curToken, "Expected return count.");
        if (curToken.IntegerVal < 0)
            return SetError(ParseResult::NegativeResultCount, curToken, "Illegal return count. Return count must be >= 0");
        def.Returns = (size_t)curToken.IntegerVal;
        ConsumeToken(); // IntegerLiteral
    }

    if (isNative) {
        if (curToken.Type != TokenType::FullStop)
            return SetError(ParseResult::UnexpectedToken, curToken,
                "Expected '.' to complete native function declaration. Native functions can't have a body.");

        bool isRedeclaration = false;
        size_t nativeIdx = Module::INVALID_INDEX;
        if (auto nameIdxPair = globalScope.NativeFunctions.Find(def.Name); nameIdxPair) {
            isRedeclaration = true;
            nativeIdx = nameIdxPair->Value();
        } else {
            isRedeclaration = false;
            nativeIdx = module.FindNativeByName(def.Name);
            if (nativeIdx == Module::INVALID_INDEX) {
                nativeIdx = module.DeclareNativeFunction(def);
            }
        }

        // If the native already exists push symbols (the function may have been defined outside the Parser)
        const FunctionDefinition& binding = module.NativeBindings[nativeIdx];
        if (!binding.DeclarationMatches(def)) {
            // TODO: EmitError() would be cool so a message could point to the previous declaration
            if (isRedeclaration) {
                return SetError(ParseResult::NativeFunctionRedeclaration, identToken, "Redeclaration of Native Function with different signature.");
            } else {
                return SetError(ParseResult::NativeFunctionDeclarationMismatch, identToken, "Declaration of Native Function with different signature.");
            }
        }

        globalScope.NativeFunctions.Emplace(std::move(def.Name), nativeIdx);
        NOTIFY_FUNCTION_DEFINITION(isRedeclaration, true, nativeIdx, binding, identToken, args, settings);
        ConsumeToken(); // FullStop
    } else {
        if (curToken.Type != TokenType::Colon)
            return SetError(ParseResult::UnexpectedToken, curToken, "Expected '->' for return count declaration or ':' to begin function body.");
        NOTIFY_FUNCTION_DEFINITION(false, false, module.Functions.Size(), def, identToken, args, settings);
        auto nameIdxPair = globalScope.Functions.Find(def.Name);
        if (nameIdxPair) {
            nameIdxPair->Value() = module.Functions.Size();
            if (settings.Warnings.DuplicateFunctionNames && def.Name != "main")
                EmitWarning(ParseWarning::DuplicateFunctionNames, identToken, "Function definition has duplicate name.");
        } else {
            globalScope.Functions.Emplace(def.Name, module.Functions.Size());
        }
        FunctionScope functionScope;
        LocalScope localScope{
            .Global = globalScope,
            .Function = &functionScope,
            .Locals = std::move(args),
        };

        ConsumeToken(); // Colon

        Token endToken;
        auto res = ParseFunctionBody(module, def, localScope, nullptr, &endToken, settings);
        if (res != ParseResult::OK) return res;

        if (endToken.Type != TokenType::FullStop)
            return SetError(ParseResult::UnexpectedToken, endToken, "You can only use the return operator to close a function definition.");

        res = BackPatchFunctionLabels(def, functionScope);
        if (res != ParseResult::OK) return res;

        module.Functions.EmplaceBack(std::move(def));
    }

    return ParseResult::OK;
}

Pulsar::ParseResult Pulsar::Parser::BackPatchFunctionLabels(FunctionDefinition& func, const FunctionScope& funcScope)
{
    for (size_t i = 0; i < funcScope.LabelUsages.Size(); i++) {
        const FunctionScope::LabelBackPatch& toBackPatch = funcScope.LabelUsages[i];
        auto nameLabelPair = funcScope.Labels.Find(toBackPatch.Label.StringVal);
        if (!nameLabelPair)
            return SetError(ParseResult::UsageOfUndeclaredLabel, toBackPatch.Label, "Usage of undeclared label.");
        size_t relJump = nameLabelPair->Value().CodeDstIdx - toBackPatch.CodeIdx;
        Instruction& instr = func.Code[toBackPatch.CodeIdx];
        if (!IsJump(instr.Code))
            return SetError(ParseResult::IllegalUsageOfLabel, toBackPatch.Label, "Labels can only be used by jump instructions.");
        instr.Arg0 = (int64_t)relJump;
    }
    return ParseResult::OK;
}

#define PUSH_CODE_SYMBOL(cond, func, token) \
    if (cond) (func).CodeDebugSymbols.EmplaceBack((token), (func).Code.Size())

Pulsar::ParseResult Pulsar::Parser::ParseFunctionBody(
    Module& module, FunctionDefinition& func,
    const LocalScope& localScope, SkippableBlock* skippableBlock,
    Token* closingToken,
    const ParseSettings& settings)
{
    if (closingToken) *closingToken = Token(TokenType::None);
    NOTIFY_SEND_BLOCK_NOTIFICATION(ParserNotifications::BlockNotificationType::BlockStart, func, localScope, settings);
    LocalScope scope = localScope;
    const Token& curToken = CurrentToken();
    while (true) {
        switch (curToken.Type) {
        case TokenType::FullStop:
            PUSH_CODE_SYMBOL(settings.StoreDebugSymbols, func, curToken);
            func.Code.EmplaceBack(InstructionCode::Return);
            NOTIFY_SEND_BLOCK_NOTIFICATION(ParserNotifications::BlockNotificationType::BlockEnd, func, scope, settings);
            if (closingToken) *closingToken = curToken;
            ConsumeToken(); // FullStop
            return ParseResult::OK;
        case TokenType::KW_Break:
            if (!skippableBlock || !skippableBlock->AllowBreak)
                return SetError(ParseResult::UnexpectedToken, curToken, "Trying to break out of an un-breakable block.");
            PUSH_CODE_SYMBOL(settings.StoreDebugSymbols, func, curToken);
            skippableBlock->BreakStatements.PushBack(func.Code.Size());
            func.Code.EmplaceBack(InstructionCode::J, 0);
            NOTIFY_SEND_BLOCK_NOTIFICATION(ParserNotifications::BlockNotificationType::BlockEnd, func, scope, settings);
            if (closingToken) *closingToken = curToken;
            ConsumeToken(); // KW_Break
            return ParseResult::OK;
        case TokenType::KW_Continue:
            if (!skippableBlock || !skippableBlock->AllowContinue)
                return SetError(ParseResult::UnexpectedToken, curToken, "Trying to repeat an un-repeatable block.");
            PUSH_CODE_SYMBOL(settings.StoreDebugSymbols, func, curToken);
            skippableBlock->ContinueStatements.PushBack(func.Code.Size());
            func.Code.EmplaceBack(InstructionCode::J, 0);
            NOTIFY_SEND_BLOCK_NOTIFICATION(ParserNotifications::BlockNotificationType::BlockEnd, func, scope, settings);
            if (closingToken) *closingToken = curToken;
            ConsumeToken(); // KW_Continue
            return ParseResult::OK;
        case TokenType::KW_End:
            // if (!allowEndKeyword)
            //     return SetError(ParseResult::UnexpectedToken, curToken, "Cannot use the 'end' keyword to close the current block.");
            NOTIFY_SEND_BLOCK_NOTIFICATION(ParserNotifications::BlockNotificationType::BlockEnd, func, scope, settings);
            if (closingToken) *closingToken = curToken;
            ConsumeToken(); // KW_End
            return ParseResult::OK;
        case TokenType::KW_Do: {
            auto res = ParseDoBlock(module, func, scope, settings);
            if (res != ParseResult::OK) return res;
        } break;
        case TokenType::KW_While: {
            PUSH_CODE_SYMBOL(settings.StoreDebugSymbols, func, curToken);
            auto res = ParseWhileLoop(module, func, scope, settings);
            if (res != ParseResult::OK) return res;
        } break;
        case TokenType::KW_Local: {
            PUSH_CODE_SYMBOL(settings.StoreDebugSymbols, func, curToken);
            auto res = ParseLocalBlock(module, func, scope, skippableBlock, settings);
            if (res != ParseResult::OK) return res;
        } break;
        case TokenType::Plus:
            PUSH_CODE_SYMBOL(settings.StoreDebugSymbols, func, curToken);
            func.Code.EmplaceBack(InstructionCode::DynSum);
            ConsumeToken(); // Plus
            break;
        case TokenType::Minus:
            PUSH_CODE_SYMBOL(settings.StoreDebugSymbols, func, curToken);
            func.Code.EmplaceBack(InstructionCode::DynSub);
            ConsumeToken(); // Minus
            break;
        case TokenType::Star:
            PUSH_CODE_SYMBOL(settings.StoreDebugSymbols, func, curToken);
            func.Code.EmplaceBack(InstructionCode::DynMul);
            ConsumeToken(); // Star
            break;
        case TokenType::Slash:
            PUSH_CODE_SYMBOL(settings.StoreDebugSymbols, func, curToken);
            func.Code.EmplaceBack(InstructionCode::DynDiv);
            ConsumeToken(); // Slash
            break;
        case TokenType::Modulus:
            PUSH_CODE_SYMBOL(settings.StoreDebugSymbols, func, curToken);
            func.Code.EmplaceBack(InstructionCode::Mod);
            ConsumeToken(); // Modulus
            break;
        case TokenType::BitAnd:
            PUSH_CODE_SYMBOL(settings.StoreDebugSymbols, func, curToken);
            func.Code.EmplaceBack(InstructionCode::BitAnd);
            ConsumeToken(); // BitAnd
            break;
        case TokenType::BitOr:
            PUSH_CODE_SYMBOL(settings.StoreDebugSymbols, func, curToken);
            func.Code.EmplaceBack(InstructionCode::BitOr);
            ConsumeToken(); // BitOr
            break;
        case TokenType::BitNot:
            PUSH_CODE_SYMBOL(settings.StoreDebugSymbols, func, curToken);
            func.Code.EmplaceBack(InstructionCode::BitNot);
            ConsumeToken(); // BitNot
            break;
        case TokenType::BitXor:
            PUSH_CODE_SYMBOL(settings.StoreDebugSymbols, func, curToken);
            func.Code.EmplaceBack(InstructionCode::BitXor);
            ConsumeToken(); // BitXor
            break;
        case TokenType::BitShiftLeft:
            PUSH_CODE_SYMBOL(settings.StoreDebugSymbols, func, curToken);
            func.Code.EmplaceBack(InstructionCode::BitShiftLeft);
            ConsumeToken(); // BitShiftLeft
            break;
        case TokenType::BitShiftRight:
            PUSH_CODE_SYMBOL(settings.StoreDebugSymbols, func, curToken);
            func.Code.EmplaceBack(InstructionCode::BitShiftRight);
            ConsumeToken(); // BitShiftRight
            break;
        case TokenType::LeftArrow:
        case TokenType::PushReference:
        case TokenType::OpenBracket:
        case TokenType::StringLiteral:
        case TokenType::IntegerLiteral:
        case TokenType::DoubleLiteral:
        case TokenType::Identifier: {
            auto res = ParseLValue(module, func, scope, settings);
            if (res != ParseResult::OK) return res;
        } break;
        case TokenType::Label: {
            if (!settings.AllowLabels)
                return SetError(ParseResult::LabelNotAllowedInContext, curToken, "Labels were disabled.");
            else if (!localScope.Function)
                return SetError(ParseResult::LabelNotAllowedInContext, curToken, "Labels are not allowed within this context.");
            else if (localScope.Function->Labels.Find(curToken.StringVal))
                return SetError(ParseResult::RedeclarationOfLabel, curToken, "Redeclaration of labels is not allowed.");
            localScope.Function->Labels.Emplace(curToken.StringVal, curToken, func.Code.Size());
            ConsumeToken(); // Label
        } break;
        case TokenType::RightArrow:
        case TokenType::BothArrows: {
            bool copyIntoLocal = curToken.Type == TokenType::BothArrows;
            PUSH_CODE_SYMBOL(settings.StoreDebugSymbols, func, curToken);
            ConsumeToken(); // RightArrow or BothArrows
            bool forceBinding = curToken.Type == TokenType::Negate;
            if (forceBinding) ConsumeToken(); // Negate
            if (curToken.Type != TokenType::Identifier)
                return SetError(ParseResult::UnexpectedToken, curToken, "Expected identifier to create local binding.");

            int64_t localIdx = -1;
            if (forceBinding) {
                localIdx = (int64_t)scope.Locals.Size();
                scope.Locals.PushBack({
                    .Name = curToken.StringVal,
                    .DeclaredAt = curToken.SourcePos
                });
                NOTIFY_SEND_BLOCK_NOTIFICATION(ParserNotifications::BlockNotificationType::LocalScopeChanged, func, scope, settings);
            } else {
                localIdx = (int64_t)scope.Locals.Size()-1;
                for (; localIdx >= 0 && scope.Locals[(size_t)localIdx].Name != curToken.StringVal; localIdx--);
            }

            if (localIdx < 0) {
                if (auto globalNameIdxPair = scope.Global.Globals.Find(curToken.StringVal); globalNameIdxPair) {
                    // Accessing global
                    int64_t globalIdx = (int64_t)globalNameIdxPair->Value();
                    if (module.Globals[(size_t)globalIdx].IsConstant)
                        return SetError(ParseResult::UnexpectedToken, curToken, "Trying to assign to constant global.");
                    NOTIFY_IDENTIFIER_USAGE(ParserNotifications::IdentifierUsageType::Global, globalIdx, func, curToken, scope, settings);
                    func.Code.EmplaceBack(
                        copyIntoLocal
                            ? InstructionCode::CopyIntoGlobal
                            : InstructionCode::PopIntoGlobal,
                        globalIdx);
                } else {
                    // Create local, global not found
                    localIdx = (int64_t)scope.Locals.Size();
                    scope.Locals.PushBack({
                        .Name = curToken.StringVal,
                        .DeclaredAt = curToken.SourcePos
                    });
                    NOTIFY_SEND_BLOCK_NOTIFICATION(ParserNotifications::BlockNotificationType::LocalScopeChanged, func, scope, settings);
                }
            }

            if (localIdx >= 0) {
                // Did not access global
                if (scope.Locals.Size() > func.LocalsCount)
                    func.LocalsCount = scope.Locals.Size();
                NOTIFY_IDENTIFIER_USAGE(ParserNotifications::IdentifierUsageType::Local, localIdx, func, curToken, scope, settings);
                func.Code.EmplaceBack(
                    copyIntoLocal
                        ? InstructionCode::CopyIntoLocal
                        : InstructionCode::PopIntoLocal,
                    localIdx);
            }

            ConsumeToken(); // Identifier
        } break;
        case TokenType::OpenParenth: {
            ConsumeToken(); // OpenParenth
            bool isNative = curToken.Type == TokenType::Star;
            bool isInstruction = curToken.Type == TokenType::Negate;
            if (isNative || isInstruction) ConsumeToken(); // Star or Negate

            PUSH_CODE_SYMBOL(settings.StoreDebugSymbols, func, curToken);
            Token identToken = curToken;
            if (identToken.Type != TokenType::Identifier) {
                if (isInstruction) {
                    return SetError(
                        ParseResult::UnexpectedToken, identToken,
                        "Expected instruction name for instruction mapping.");
                } else {
                    return SetError(
                        ParseResult::UnexpectedToken, identToken,
                        "Expected function name for function call.");
                }
            }
            ConsumeToken(); // Identifier

            Token argToken(TokenType::None);
            if (isInstruction && (
                curToken.Type == TokenType::IntegerLiteral ||
                curToken.Type == TokenType::Label
            )) {
                argToken = curToken;
                ConsumeToken(); // IntegerLiteral or Label
            }

            // We want to do this check here to get back to the user asap
            // Without trying to resolve the name of the function/instruction
            if (curToken.Type != TokenType::CloseParenth)
                return SetError(ParseResult::UnexpectedToken, curToken, "Expected ')' to close function call.");

            if (isInstruction) {
                auto instrNameDescPair = InstructionMappings.Find(identToken.StringVal);
                if (!instrNameDescPair)
                    return SetError(ParseResult::UsageOfUnknownInstruction, identToken, "Instruction does not exist.");

                const InstructionDescription& instrDesc = instrNameDescPair->Value();
                if (instrDesc.MayFail)
                    PUSH_CODE_SYMBOL(settings.StoreDebugSymbols, func, identToken);

                int64_t arg0 = 0;
                if (IsJump(instrDesc.Code)) {
                    // Allow only Labels in jump instructions
                    if (argToken.Type == TokenType::None)
                        return SetError(ParseResult::UnexpectedToken, curToken, "Expected label for jump instruction.");
                    else if (argToken.Type != TokenType::Label)
                        return SetError(ParseResult::UnexpectedToken, argToken, "Jump instructions only accept labels as arguments.");
                    else if (!settings.AllowLabels)
                        return SetError(ParseResult::LabelNotAllowedInContext, argToken, "Labels were disabled.");
                    else if (!localScope.Function)
                        return SetError(ParseResult::LabelNotAllowedInContext, argToken, "Labels are not allowed within this context.");
                    localScope.Function->LabelUsages.EmplaceBack(argToken, func.Code.Size());
                } else if (argToken.Type != TokenType::None) {
                    // Not a jump and an argument was provided
                    if (argToken.Type != TokenType::IntegerLiteral)
                        return SetError(ParseResult::UnexpectedToken, argToken, "Non-jump instructions only accept integer literals as arguments.");
                    arg0 = argToken.IntegerVal;
                }

                func.Code.EmplaceBack(instrDesc.Code, arg0);
            } else if (isNative) {
                auto nativeNameIdxPair = scope.Global.NativeFunctions.Find(identToken.StringVal);
                if (!nativeNameIdxPair) {
                    return SetError(ParseResult::UsageOfUndeclaredNativeFunction, identToken, "Native function not declared.");
                }
                int64_t funcIdx = (int64_t)nativeNameIdxPair->Value();
                NOTIFY_IDENTIFIER_USAGE(ParserNotifications::IdentifierUsageType::NativeFunction, funcIdx, func, identToken, scope, settings);
                func.Code.EmplaceBack(InstructionCode::CallNative, funcIdx);
            } else {
                auto funcNameIdxPair = scope.Global.Functions.Find(identToken.StringVal);
                if (!funcNameIdxPair)
                    return SetError(ParseResult::UsageOfUndeclaredFunction, identToken, "Function not declared.");
                int64_t funcIdx = (int64_t)funcNameIdxPair->Value();
                NOTIFY_IDENTIFIER_USAGE(ParserNotifications::IdentifierUsageType::Function, funcIdx, func, identToken, scope, settings);
                func.Code.EmplaceBack(InstructionCode::Call, funcIdx);
            }

            ConsumeToken(); // CloseParenth
        } break;
        case TokenType::KW_If: {
            PUSH_CODE_SYMBOL(settings.StoreDebugSymbols, func, curToken);
            auto res = ParseIfStatement(module, func, scope, skippableBlock, false, settings);
            if (res != ParseResult::OK) return res;
        } break;
        default: {
            String errorMessage = "Expression expected.";
            if (settings.AppendNotesToErrorMessage) {
                errorMessage += "\nNote: You may have forgotten to return from function '";
                errorMessage += func.Name;
                errorMessage += "' or end some block within it.";
            }
            return SetError(ParseResult::UnexpectedToken, curToken, errorMessage);
        } break;
        }
    }
}

Pulsar::ParseResult Pulsar::Parser::ParseIfStatement(
    Module& module, FunctionDefinition& func,
    const LocalScope& localScope, SkippableBlock* skippableBlock,
    bool isChained, const ParseSettings& settings)
{
    Token ifToken = CurrentToken();
    if (ifToken.Type != TokenType::KW_If)
        return SetError(ParseResult::UnexpectedToken, ifToken, "Expected if statement.");
    Token comparisonToken(TokenType::None);
    InstructionCode jmpInstrCode = InstructionCode::JZ;
    InstructionCode compInstrCode = InstructionCode::Equals;
    // Whether the if condition is fully contained within the statement.
    bool isSelfContained = false;
    bool hasComparison = false;
    bool invertedJump = false;

    ConsumeToken(); // KW_If
    const Token& curToken = CurrentToken();
    if (curToken.Type == TokenType::KW_Not) {
        ConsumeToken(); // KW_Not
        invertedJump = true;
    }

    if (curToken.Type != TokenType::Colon) {
        hasComparison = true;
        switch (curToken.Type) {
        case TokenType::StringLiteral:
        case TokenType::IntegerLiteral:
        case TokenType::DoubleLiteral:
        case TokenType::Identifier: {
            isSelfContained = true;
            // compInstrCode = InstructionCode::Equals;
            jmpInstrCode = InstructionCode::JZ;
            auto res = ParseLValue(module, func, localScope, settings);
            if (res != ParseResult::OK) return res;
        } break;
        default:
            break;
        }

        if (curToken.Type == TokenType::Colon) {
            isSelfContained = false;
        } else if (curToken.Type != TokenType::Colon) {
            switch (curToken.Type) {
            case TokenType::Equals:
                // compInstrCode = InstructionCode::Equals;
                jmpInstrCode = InstructionCode::JZ;
                break;
            case TokenType::NotEquals:
                // compInstrCode = InstructionCode::Equals;
                jmpInstrCode = InstructionCode::JNZ;
                break;
            case TokenType::Less:
                compInstrCode = InstructionCode::Compare;
                jmpInstrCode = InstructionCode::JGEZ;
                break;
            case TokenType::LessOrEqual:
                compInstrCode = InstructionCode::Compare;
                jmpInstrCode = InstructionCode::JGZ;
                break;
            case TokenType::More:
                compInstrCode = InstructionCode::Compare;
                jmpInstrCode = InstructionCode::JLEZ;
                break;
            case TokenType::MoreOrEqual:
                compInstrCode = InstructionCode::Compare;
                jmpInstrCode = InstructionCode::JLZ;
                break;
            default:
                return SetError(ParseResult::UnexpectedToken, curToken, "Expected if body start ':' or comparison operator.");
            }

            comparisonToken = curToken;
            ConsumeToken(); // Equals, NotEquals, Less, LessOrEqual, More or MoreOrEqual
            switch (curToken.Type) {
            case TokenType::StringLiteral:
            case TokenType::IntegerLiteral:
            case TokenType::DoubleLiteral:
            case TokenType::Identifier: {
                auto res = ParseLValue(module, func, localScope, settings);
                if (res != ParseResult::OK) return res;
            } break;
            default:
                return SetError(ParseResult::UnexpectedToken, curToken, "Expected lvalue of type Integer, Double or Local after comparison operator.");
            }
        }
    }

    if (isChained && !isSelfContained)
        return SetError(ParseResult::UnsafeChainedIfStatement, ifToken, "Chained if statement must have a self-contained condition.");

    if (curToken.Type != TokenType::Colon)
        return SetError(ParseResult::UnexpectedToken, curToken, "Expected ':' to begin if statement body.");

    if (hasComparison) {
        if (comparisonToken.Type != TokenType::None) {
            PUSH_CODE_SYMBOL(settings.StoreDebugSymbols, func, comparisonToken);
        } else PUSH_CODE_SYMBOL(settings.StoreDebugSymbols, func, ifToken);
        func.Code.EmplaceBack(compInstrCode);
    }

    PUSH_CODE_SYMBOL(settings.StoreDebugSymbols, func, ifToken);
    size_t ifIdx = func.Code.Size();
    if (invertedJump)
        jmpInstrCode = InvertJump(jmpInstrCode);
    func.Code.EmplaceBack(jmpInstrCode, 0);

    ConsumeToken(); // Colon

    Token endToken;
    auto res = ParseFunctionBody(module, func, localScope, skippableBlock, &endToken, settings);
    func.Code[ifIdx].Arg0 = func.Code.Size() - ifIdx;
    if (res == ParseResult::UnexpectedToken) {
        if (curToken.Type != TokenType::KW_Else)
            return res;
        ClearError();
        // FIXME: localScope is not the correct scope...
        NOTIFY_SEND_BLOCK_NOTIFICATION(ParserNotifications::BlockNotificationType::BlockEnd, func, localScope, settings);
    } else if (res != ParseResult::OK) {
        return res;
    } else {
        // ... if <cmp> ...: ... (end|continue|break|.)
        if (!isSelfContained)
            return ParseResult::OK;
        // if ... <cmp> ...: ... end
        if (endToken.Type == TokenType::KW_End)
            return ParseResult::OK;
        // if ... <cmp> ...: ... (continue|break|.)
        // We either want an else or an end
        if (curToken.Type == TokenType::KW_End)
            return ParseResult::OK;

        if (curToken.Type != TokenType::KW_Else) {
            return SetError(ParseResult::UnexpectedToken, curToken,
                "Expected 'end' to close or 'else' to create a new branch of a self-contained if statement.");
        }
    }

    // If we get here, there's an else branch
    size_t elseIdx = func.Code.Size();
    PUSH_CODE_SYMBOL(settings.StoreDebugSymbols, func, curToken);
    func.Code.EmplaceBack(InstructionCode::J, 0);
    func.Code[ifIdx].Arg0 = func.Code.Size() - ifIdx;

    ConsumeToken(); // KW_Else
    if (curToken.Type == TokenType::Colon) {
        // else: ...
        ConsumeToken(); // Colon
        res = ParseFunctionBody(module, func, localScope, skippableBlock, &endToken, settings);
        if (res != ParseResult::OK)
            return res;

        if (isSelfContained && endToken.Type != TokenType::KW_End) {
            // ... if ... <cmp> ...:
            //     ...
            // else:
            //     ...
            //     (continue|break|.)
            if (curToken.Type != TokenType::KW_End)
                return SetError(ParseResult::UnexpectedToken, curToken, "Expected 'end' to close else branch.");
            ConsumeToken(); // KW_End
        }

        func.Code[elseIdx].Arg0 = func.Code.Size() - elseIdx;
        return ParseResult::OK;
    } else if (curToken.Type == TokenType::KW_If) {
        if (!isSelfContained)
            return SetError(ParseResult::UnexpectedToken, curToken, "Illegal 'else if' statement. Previous condition is not self-contained.");
        res = ParseIfStatement(module, func, localScope, skippableBlock, true, settings);
        func.Code[elseIdx].Arg0 = func.Code.Size() - elseIdx;
        return res;
    }

    return SetError(ParseResult::UnexpectedToken, curToken, "Expected 'else' block start or 'else if' compound statement.");
}

inline bool IsDummyIdentifier(const Pulsar::String& id)
{
    return id.Length() == 1 && id[0] == '_';
}

Pulsar::ParseResult Pulsar::Parser::ParseLocalBlock(Module& module, FunctionDefinition& func, const LocalScope& parentScope, SkippableBlock* skippableBlock, const ParseSettings& settings)
{
    const Token& curToken = CurrentToken();
    if (curToken.Type != TokenType::KW_Local)
        return SetError(ParseResult::UnexpectedToken, curToken, "Expected local block.");

    List<Token> localNames;
    // Map that holds the index within localNames for the last definition of a local with a specific name.
    HashMap<String, size_t> localNameToIdx;
    ConsumeToken(); // KW_Local
    while (curToken.Type != TokenType::Colon) {
        if (curToken.Type != TokenType::Identifier)
            return SetError(ParseResult::UnexpectedToken, curToken, "Expected name of local.");
        if (!IsDummyIdentifier(curToken.StringVal))
            localNameToIdx.Insert(curToken.StringVal, localNames.Size());
        localNames.EmplaceBack(curToken);
        ConsumeToken(); // Identifier
    }

    // localScope will point to _localScope only if some locals were bound.
    LocalScope* localScope = nullptr;
    LocalScope _localScope{
        .Global = parentScope.Global,
        .Function = parentScope.Function,
    };

    for (size_t i = 0; i < localNames.Size(); i++) {
        size_t localNameIdx = localNames.Size()-i-1;
        const Token& localName = localNames[localNameIdx];
        // Because ids from localName are always put into localNameToIdx, it is safe to dereference.
        // Except for "Dummy Identifiers"
        if (IsDummyIdentifier(localName.StringVal) || localNameToIdx.Find(localName.StringVal)->Value() != localNameIdx) {
            if (settings.StoreDebugSymbols) {
                PUSH_CODE_SYMBOL(true, func, localName);
                func.Code.EmplaceBack(InstructionCode::Pop);
            } else {
                // Optimize for sequences of '_'
                // Only if debug is not enabled
                int64_t count = 1;
                for (size_t j = 0; j < localNameIdx; j++) {
                    const Token& prevLocalName = localNames[localNameIdx-j-1];
                    if (!(IsDummyIdentifier(prevLocalName.StringVal) || localNameToIdx.Find(localName.StringVal)->Value() != localNameIdx))
                        break;
                    count++;
                    i++;
                }
                func.Code.EmplaceBack(InstructionCode::Pop, count);
            }
        } else {
            if (!localScope) {
                // Populate localScope only if needed.
                // This speeds-up the parsing of `local: ... end`
                // Because parentScope won't be copied.
                localScope = &_localScope;
                _localScope.Locals = parentScope.Locals;
            }

            int64_t localIdx = (int64_t)_localScope.Locals.Size();
            _localScope.Locals.PushBack({
                .Name = localName.StringVal,
                .DeclaredAt = localName.SourcePos
            });
            if (_localScope.Locals.Size() > func.LocalsCount)
                func.LocalsCount = _localScope.Locals.Size();
            PUSH_CODE_SYMBOL(settings.StoreDebugSymbols, func, localName);
            func.Code.EmplaceBack(InstructionCode::PopIntoLocal, localIdx);
        }
    }

    ConsumeToken(); // Colon
    Token endToken;
    auto res = ParseFunctionBody(module, func, localScope ? *localScope : parentScope, skippableBlock, &endToken, settings);
    if (res != ParseResult::OK)
        return res;

    if (endToken.Type != TokenType::KW_End) {
        if (curToken.Type != TokenType::KW_End)
            return SetError(ParseResult::UnexpectedToken, curToken, "Expected 'end' to close local block.");
        ConsumeToken(); // KW_End
    }

    return ParseResult::OK;
}

Pulsar::ParseResult Pulsar::Parser::ParseWhileLoop(Module& module, FunctionDefinition& func, const LocalScope& localScope, const ParseSettings& settings)
{
    const Token& curToken = CurrentToken();
    if (curToken.Type != TokenType::KW_While)
        return SetError(ParseResult::UnexpectedToken, curToken, "Expected while loop");
    Token whileToken = curToken;
    Token comparisonToken(TokenType::None);
    InstructionCode jmpInstrCode = InstructionCode::JZ;
    InstructionCode compInstrCode = InstructionCode::Equals;
    bool hasComparison = false;
    bool whileTrue = false;
    bool invertedJump = false;

    size_t whileIdx = func.Code.Size();
    ConsumeToken(); // KW_While
    if (curToken.Type == TokenType::KW_Not) {
        ConsumeToken(); // KW_Not
        invertedJump = true;
    }

    if (curToken.Type == TokenType::Colon) {
        if (invertedJump) {
            jmpInstrCode = InstructionCode::J;
        } else {
            whileTrue = true;
        }
    } else {
        switch (curToken.Type) {
        case TokenType::StringLiteral:
        case TokenType::IntegerLiteral:
        case TokenType::DoubleLiteral:
        case TokenType::Identifier: {
            // jmpInstrCode = InstructionCode::JZ;
            auto res = ParseLValue(module, func, localScope, settings);
            if (res != ParseResult::OK) return res;
        } break;
        default:
            break;
        }

        if (curToken.Type != TokenType::Colon) {
            hasComparison = true;
            switch (curToken.Type) {
            case TokenType::Equals:
                // compInstrCode = InstructionCode::Equals;
                jmpInstrCode = InstructionCode::JZ;
                break;
            case TokenType::NotEquals:
                // compInstrCode = InstructionCode::Equals;
                jmpInstrCode = InstructionCode::JNZ;
                break;
            case TokenType::Less:
                compInstrCode = InstructionCode::Compare;
                jmpInstrCode = InstructionCode::JGEZ;
                break;
            case TokenType::LessOrEqual:
                compInstrCode = InstructionCode::Compare;
                jmpInstrCode = InstructionCode::JGZ;
                break;
            case TokenType::More:
                compInstrCode = InstructionCode::Compare;
                jmpInstrCode = InstructionCode::JLEZ;
                break;
            case TokenType::MoreOrEqual:
                compInstrCode = InstructionCode::Compare;
                jmpInstrCode = InstructionCode::JLZ;
                break;
            default:
                return SetError(ParseResult::UnexpectedToken, curToken, "Expected while loop body start ':' or comparison operator.");
            }

            comparisonToken = curToken;
            ConsumeToken(); // Equals, NotEquals, Less, LessOrEqual, More or MoreOrEqual
            switch (curToken.Type) {
            case TokenType::StringLiteral:
            case TokenType::IntegerLiteral:
            case TokenType::DoubleLiteral:
            case TokenType::Identifier: {
                auto res = ParseLValue(module, func, localScope, settings);
                if (res != ParseResult::OK) return res;
            } break;
            default:
                return SetError(ParseResult::UnexpectedToken, curToken, "Expected lvalue of type String, Integer, Double or Local after comparison operator.");
            }
        }
    }

    if (curToken.Type != TokenType::Colon)
        return SetError(ParseResult::UnexpectedToken, curToken, "Expected ':' to begin while loop body.");

    if (hasComparison) {
        PUSH_CODE_SYMBOL(settings.StoreDebugSymbols, func, comparisonToken);
        func.Code.EmplaceBack(compInstrCode);
    }

    SkippableBlock block{
        .AllowBreak = true,
        .AllowContinue = true,
    };

    if (!whileTrue) {
        block.BreakStatements.PushBack(func.Code.Size());
        PUSH_CODE_SYMBOL(settings.StoreDebugSymbols, func, whileToken);
        if (invertedJump)
            jmpInstrCode = InvertJump(jmpInstrCode);
        func.Code.EmplaceBack(jmpInstrCode, 0);
    }

    ConsumeToken(); // Colon

    Token endToken;
    auto res = ParseFunctionBody(module, func, localScope, &block, &endToken, settings);
    if (res != ParseResult::OK)
        return res;

    if (endToken.Type != TokenType::KW_End) {
        if (curToken.Type != TokenType::KW_End)
            return SetError(ParseResult::UnexpectedToken, curToken, "Expected 'end' to close while loop body.");
        ConsumeToken(); // TokenType::KW_End
    }

    for (size_t i = 0; i < block.ContinueStatements.Size(); i++)
        func.Code[block.ContinueStatements[i]].Arg0 = (int64_t)(whileIdx-block.ContinueStatements[i]);

    size_t endIdx = func.Code.Size();
    PUSH_CODE_SYMBOL(settings.StoreDebugSymbols, func, whileToken);
    func.Code.EmplaceBack(InstructionCode::J, (int64_t)(whileIdx-endIdx));

    size_t breakIdx = func.Code.Size();
    for (size_t i = 0; i < block.BreakStatements.Size(); i++)
        func.Code[block.BreakStatements[i]].Arg0 = (int64_t)(breakIdx-block.BreakStatements[i]);
    return ParseResult::OK;
}

Pulsar::ParseResult Pulsar::Parser::ParseDoBlock(Module& module, FunctionDefinition& func, const LocalScope& localScope, const ParseSettings& settings)
{
    const Token& curToken = CurrentToken();
    if (curToken.Type != TokenType::KW_Do)
        return SetError(ParseResult::UnexpectedToken, curToken, "Expected do block.");
    Token doToken = curToken;

    size_t doIdx = func.Code.Size();
    ConsumeToken(); // KW_Do
    if (curToken.Type != TokenType::Colon)
        return SetError(ParseResult::UnexpectedToken, curToken, "Expected ':' to begin do block body.");

    SkippableBlock block{
        .AllowBreak = true,
        .AllowContinue = true,
    };

    ConsumeToken(); // Colon

    Token endToken;
    auto res = ParseFunctionBody(module, func, localScope, &block, &endToken, settings);
    if (res != ParseResult::OK)
        return res;

    if (endToken.Type != TokenType::KW_End) {
        if (curToken.Type != TokenType::KW_End)
            return SetError(ParseResult::UnexpectedToken, curToken, "Expected 'end' to close do block body.");
        ConsumeToken(); // KW_End
    }

    for (size_t i = 0; i < block.ContinueStatements.Size(); i++)
        func.Code[block.ContinueStatements[i]].Arg0 = (int64_t)(doIdx-block.ContinueStatements[i]);

    size_t breakIdx = func.Code.Size();
    for (size_t i = 0; i < block.BreakStatements.Size(); i++)
        func.Code[block.BreakStatements[i]].Arg0 = (int64_t)(breakIdx-block.BreakStatements[i]);
    return ParseResult::OK;
}

// TODO: Rename
Pulsar::ParseResult Pulsar::Parser::ParseLValue(Module& module, FunctionDefinition& func, const LocalScope& localScope, const ParseSettings& settings)
{
    const Token& lvalue = CurrentToken();
    switch (lvalue.Type) {
    case TokenType::IntegerLiteral: {
        func.Code.EmplaceBack(InstructionCode::PushInt, lvalue.IntegerVal);
        ConsumeToken(); // IntegerLiteral
    } break;
    case TokenType::DoubleLiteral: {
        static_assert(sizeof(double) == sizeof(int64_t));
        void* val = (void*)&lvalue.DoubleVal;
        // Don't want to rely on std::bit_cast since my g++ does not have it.
        int64_t arg0 = *(int64_t*)val;
        func.Code.EmplaceBack(InstructionCode::PushDbl, arg0);
        ConsumeToken(); // DoubleLiteral
    } break;
    case TokenType::Identifier: {
        PUSH_CODE_SYMBOL(settings.StoreDebugSymbols, func, lvalue);
        int64_t localIdx = (int64_t)localScope.Locals.Size()-1;
        for (; localIdx >= 0 && localScope.Locals[(size_t)localIdx].Name != lvalue.StringVal; localIdx--);
        if (localIdx < 0) {
            auto globalNameIdxPair = localScope.Global.Globals.Find(lvalue.StringVal);
            if (!globalNameIdxPair)
                return SetError(ParseResult::UsageOfUndeclaredLocal, lvalue, "Local not declared.");
            int64_t globalIdx = (int64_t)globalNameIdxPair->Value();
            NOTIFY_IDENTIFIER_USAGE(ParserNotifications::IdentifierUsageType::Global, globalIdx, func, lvalue, localScope, settings);
            func.Code.EmplaceBack(InstructionCode::PushGlobal, globalIdx);
        } else {
            NOTIFY_IDENTIFIER_USAGE(ParserNotifications::IdentifierUsageType::Local, localIdx, func, lvalue, localScope, settings);
            func.Code.EmplaceBack(InstructionCode::PushLocal, localIdx);
        }
        ConsumeToken(); // Identifier
    } break;
    case TokenType::StringLiteral: {
        Token stringToken;
        auto res = ParseStringLiteral(stringToken);
        if (res != ParseResult::OK) return res;

        PUSH_CODE_SYMBOL(settings.StoreDebugSymbols, func, stringToken);
        int64_t constIdx = (int64_t)module.Constants.Size()-1;
        Value constVal;
        constVal.SetString(stringToken.StringVal);
        for (; constIdx >= 0 && module.Constants[(size_t)constIdx] != constVal; constIdx--);
        if (constIdx < 0) {
            constIdx = module.Constants.Size();
            module.Constants.EmplaceBack(std::move(constVal));
        }
        func.Code.EmplaceBack(InstructionCode::PushConst, constIdx);
    } break;
    case TokenType::PushReference: {
        ConsumeToken(); // PushReference
        if (lvalue.Type == TokenType::Identifier) {
            return SetError(ParseResult::UnexpectedToken, lvalue, "Local reference is not supported, expected (function).");
        } else if (lvalue.Type == TokenType::OpenParenth) {
            ConsumeToken(); // OpenParenth

            bool isNative = lvalue.Type == TokenType::Star;
            if (isNative) ConsumeToken(); // Star

            if (lvalue.Type != TokenType::Identifier)
                return SetError(ParseResult::UnexpectedToken, lvalue, "Expected (function) name.");
            Token identToken = lvalue;

            ConsumeToken(); // Identifier
            if (lvalue.Type != TokenType::CloseParenth)
                return SetError(ParseResult::UnexpectedToken, lvalue, "Expected ')' to close function reference.");

            if (isNative) {
                auto nativeNameIdxPair = localScope.Global.NativeFunctions.Find(identToken.StringVal);
                if (!nativeNameIdxPair)
                    return SetError(ParseResult::UsageOfUndeclaredNativeFunction, identToken, "Native function not declared.");
                int64_t funcIdx = (int64_t)nativeNameIdxPair->Value();
                NOTIFY_IDENTIFIER_USAGE(ParserNotifications::IdentifierUsageType::NativeFunction, funcIdx, func, identToken, localScope, settings);
                func.Code.EmplaceBack(InstructionCode::PushNativeFunctionReference, funcIdx);
            } else {
                auto funcNameIdxPair = localScope.Global.Functions.Find(identToken.StringVal);
                if (!funcNameIdxPair)
                    return SetError(ParseResult::UsageOfUndeclaredFunction, identToken, "Function not declared.");
                int64_t funcIdx = (int64_t)funcNameIdxPair->Value();
                NOTIFY_IDENTIFIER_USAGE(ParserNotifications::IdentifierUsageType::Function, funcIdx, func, identToken, localScope, settings);
                func.Code.EmplaceBack(InstructionCode::PushFunctionReference, funcIdx);
            }

            ConsumeToken(); // CloseParenth
        } else {
            return SetError(ParseResult::UnexpectedToken, lvalue, "Expected (function) to reference.");
        }
    } break;
    case TokenType::LeftArrow: {
        PUSH_CODE_SYMBOL(settings.StoreDebugSymbols, func, lvalue);

        ConsumeToken(); // LeftArrow
        if (lvalue.Type != TokenType::Identifier)
            return SetError(ParseResult::UnexpectedToken, lvalue, "Expected local name.");

        int64_t localIdx = (int64_t)localScope.Locals.Size()-1;
        for (; localIdx >= 0 && localScope.Locals[(size_t)localIdx].Name != lvalue.StringVal; localIdx--);
        if (localIdx < 0) {
            auto globalNameIdxPair = localScope.Global.Globals.Find(lvalue.StringVal);
            if (!globalNameIdxPair)
                return SetError(ParseResult::UsageOfUndeclaredLocal, lvalue, "Local not declared.");
            if (module.Globals[globalNameIdxPair->Value()].IsConstant)
                return SetError(ParseResult::WritingToConstantGlobal, lvalue, "Cannot move constant global.");
            int64_t globalIdx = (int64_t)globalNameIdxPair->Value();
            NOTIFY_IDENTIFIER_USAGE(ParserNotifications::IdentifierUsageType::Global, globalIdx, func, lvalue, localScope, settings);
            func.Code.EmplaceBack(InstructionCode::MoveGlobal, globalIdx);
        } else {
            NOTIFY_IDENTIFIER_USAGE(ParserNotifications::IdentifierUsageType::Local, localIdx, func, lvalue, localScope, settings);
            func.Code.EmplaceBack(InstructionCode::MoveLocal, localIdx);
        }

        ConsumeToken(); // Identifier
    } break;
    case TokenType::OpenBracket: {
        ConsumeToken(); // OpenBracket

        int64_t listSize = 0;
        while (true) {
            switch (lvalue.Type) {
            case TokenType::IntegerLiteral:
            case TokenType::DoubleLiteral:
            case TokenType::LeftArrow:
            case TokenType::PushReference:
            case TokenType::StringLiteral:
            case TokenType::Identifier:
            case TokenType::OpenBracket: {
                auto res = ParseLValue(module, func, localScope, settings);
                if (res != ParseResult::OK)
                    return res;
                ++listSize;
            } break;
            case TokenType::CloseBracket: break;
            default:
                return SetError(ParseResult::UnexpectedToken, lvalue, "Expected lvalue.");
            }

            if (lvalue.Type == TokenType::CloseBracket) {
                if (listSize <= 0) {
                    // Empty List
                    func.Code.EmplaceBack(InstructionCode::PushEmptyList);
                } else {
                    PUSH_CODE_SYMBOL(settings.StoreDebugSymbols, func, lvalue);
                    func.Code.EmplaceBack(InstructionCode::Pack, listSize);
                }

                ConsumeToken(); // CloseBracket
                break; // while(true)
            }

            if (lvalue.Type == TokenType::Comma) {
                ConsumeToken(); // Comma
            } else if (lvalue.Type != TokenType::CloseBracket) {
                return SetError(ParseResult::UnexpectedToken, lvalue, "Expected ',' to continue List literal or ']' to close it.");
            }
        }
    } break;
    default:
        return SetError(ParseResult::UnexpectedToken, lvalue, "Expected lvalue.");
    }
    return ParseResult::OK;
}

Pulsar::ParseResult Pulsar::Parser::ParseStringLiteral(Token& token)
{
    const Token& curToken = CurrentToken();
    if (curToken.Type != TokenType::StringLiteral)
        return SetError(ParseResult::UnexpectedToken, curToken, "Expected String literal.");

    token = curToken;
    while (true) {
        ConsumeToken(); // StringLiteral
        if (curToken.Type != TokenType::StringLiteralJoin)
            break;

        char joinChar = curToken.CharVal;

        ConsumeToken(); // StringLiteralJoin
        if (curToken.Type != TokenType::StringLiteral)
            return SetError(ParseResult::UnexpectedToken, curToken, "Expected String literal to join.");

        if (joinChar != '\0')
            token.StringVal += joinChar;
        token.StringVal += curToken.StringVal;

        if (token.SourcePos.Line == curToken.SourcePos.Line) {
            // Same line: Extend CharSpan to include the whole String
            // "Foo" \ "Bar"
            // ^       ^
            // |       curToken.Char
            // token.Char
            // ~~~~~~~~ = curToken.Char - token.Char
            //         ~~~~~ = curToken.CharSpan
            token.SourcePos.CharSpan = (curToken.SourcePos.Char - token.SourcePos.Char) + curToken.SourcePos.CharSpan;
        } else {
            // If it's not the same line take the last SourcePos
            token.SourcePos = curToken.SourcePos;
        }
    }

    return ParseResult::OK;
}

void Pulsar::Parser::ConsumeToken()
{
    if (m_Lexers.Size() <= 0) return;
    auto& lexer = m_Lexers.Back();
    lexer.CurrentToken = lexer.Lexer.NextToken();
}

const Pulsar::Token& Pulsar::Parser::CurrentToken() const
{
    static const Token NONE_TOKEN(TokenType::None);
    return m_Lexers.Size() > 0 ? m_Lexers.Back().CurrentToken : NONE_TOKEN;
}

const Pulsar::String* Pulsar::Parser::GetSourceFromIndex(size_t sourceIndex) const
{
    if (sourceIndex == INVALID_INDEX || sourceIndex >= m_SourceDebugSymbols.Size())
        return nullptr;
    return &m_SourceDebugSymbols[sourceIndex].Source;
}

const Pulsar::String* Pulsar::Parser::GetPathFromIndex(size_t sourceIndex) const
{
    if (sourceIndex == INVALID_INDEX || sourceIndex >= m_SourceDebugSymbols.Size())
        return nullptr;
    return &m_SourceDebugSymbols[sourceIndex].Path;
}

size_t Pulsar::Parser::CurrentSourceIndex() const
{
    return m_Lexers.Size() > 0
        ? m_Lexers.Back().SourceIndex
        : INVALID_INDEX;
}

const Pulsar::String* Pulsar::Parser::CurrentPath() const
{
    auto sourceIndex = CurrentSourceIndex();
    if (sourceIndex == INVALID_INDEX) return nullptr;
    return &m_SourceDebugSymbols[sourceIndex].Path;
}

const Pulsar::String* Pulsar::Parser::CurrentSource() const
{
    auto sourceIndex = CurrentSourceIndex();
    if (sourceIndex == INVALID_INDEX) return nullptr;
    return &m_SourceDebugSymbols[sourceIndex].Source;
}

bool Pulsar::Parser::HasMessages() const
{
    return m_ErrorMessage.SourceIndex != INVALID_INDEX
        || !m_WarningMessages.IsEmpty();
}

void Pulsar::Parser::StripUnusedSources()
{
    if (m_SourceDebugSymbols.IsEmpty()) return;

    List<bool> usedSources(m_SourceDebugSymbols.Size());
    usedSources.Resize(m_SourceDebugSymbols.Size(), false);

    bool isAtLeastOneSourceUsed = false;

    if (m_ErrorMessage.SourceIndex != INVALID_INDEX && m_ErrorMessage.SourceIndex < usedSources.Size()) {
        usedSources[m_ErrorMessage.SourceIndex] = true;
        isAtLeastOneSourceUsed = true;
    }

    for (size_t i = 0; i < m_WarningMessages.Size(); ++i) {
        const Message& message = m_WarningMessages[i];
        if (message.SourceIndex != INVALID_INDEX && message.SourceIndex < usedSources.Size()) {
            usedSources[message.SourceIndex] = true;
            isAtLeastOneSourceUsed = true;
        }
    }

    if (isAtLeastOneSourceUsed) {
        for (size_t i = 0; i < usedSources.Size(); ++i) {
            if (!usedSources[i]) {
                auto& symbol = m_SourceDebugSymbols[i];
                symbol.Path   = String();
                symbol.Source = String();
            }
        }
    } else {
        m_SourceDebugSymbols.Clear();
    }
}

bool Pulsar::Parser::PathToNormalizedFileSystemPath(const String& path, String& outNormalized)
{
#ifdef PULSAR_NO_FILESYSTEM
    PULSAR_UNUSED(path, outNormalized);
    return false;
#else // PULSAR_NO_FILESYSTEM
    auto rawPath = std::filesystem::path(path.CString());

    std::error_code error;
    auto normalizedPath = std::filesystem::relative(rawPath, error);

    if (error || normalizedPath.empty()) {
        // If path is empty it may be located on a different drive on Windows.
        // In which case we should compute the canonical path.
        normalizedPath = std::filesystem::canonical(rawPath, error);
    }

    if (error || normalizedPath.empty()) {
        return false;
    }

    outNormalized = normalizedPath.generic_string().c_str();
    return true;
#endif // PULSAR_NO_FILESYSTEM
}

Pulsar::ParseSettings::IncludeResolverFn Pulsar::ParseSettings::CreateFileSystemIncludeResolver(IncludePaths&& includePaths, bool showPathsInErrorMessage)
{
#ifdef PULSAR_NO_FILESYSTEM
    PULSAR_UNUSED(includePaths, showPathsInErrorMessage);
    return nullptr;
#else // PULSAR_NO_FILESYSTEM
    return [includePaths = std::move(includePaths), showPathsInErrorMessage](Pulsar::Parser& parser, Pulsar::String cwf, Pulsar::Token token) {
        // Populated only if showPathsInErrorMessage is true
        List<String> triedPaths;

        std::filesystem::path targetPath(token.StringVal.CString());

        ParseResult result;
        { // Try relative path first
            std::filesystem::path workingPath(cwf.CString());
            workingPath = workingPath.parent_path();
            std::filesystem::path filePath = workingPath / targetPath;
            String pulsarPath = filePath.generic_string().c_str();
            result = parser.AddSourceFile(pulsarPath);
            if (result == ParseResult::OK) return result;
            if (showPathsInErrorMessage) triedPaths.EmplaceBack(std::move(pulsarPath));
        }

        if (!targetPath.is_absolute()) {
            for (size_t i = includePaths.Size(); i > 0; --i) {
                std::filesystem::path workingPath(includePaths[i-1].CString());
                std::filesystem::path filePath = workingPath / targetPath;
                String pulsarPath = filePath.generic_string().c_str();
                result = parser.AddSourceFile(pulsarPath);
                if (result == ParseResult::OK) return result;
                if (showPathsInErrorMessage) triedPaths.EmplaceBack(std::move(pulsarPath));
            }
        }

        // Here result is always != ParseResult::OK
        if (showPathsInErrorMessage) {
            // Relative and include paths failed
            String errorMsg = "Could not read file ";

            // triedPaths must at least contain the relative path
            Parser::PathToNormalizedFileSystemPath(triedPaths[0], triedPaths[0]);
            errorMsg += '\'';
            errorMsg += triedPaths[0];
            errorMsg += '\'';

            for (size_t i = 1; i < triedPaths.Size(); ++i) {
                Parser::PathToNormalizedFileSystemPath(triedPaths[i], triedPaths[i]);
                errorMsg += ", '";
                errorMsg += triedPaths[i];
                errorMsg += '\'';
            }

            errorMsg += '.';
            return parser.SetError(Pulsar::ParseResult::FileNotRead, token, errorMsg);
        }

        return result;
    };
#endif // PULSAR_NO_FILESYSTEM
}

const char* Pulsar::ParseResultToString(ParseResult result)
{
    switch (result) {
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
    case ParseResult::NativeFunctionRedeclaration:
        return "NativeFunctionRedeclaration";
    case ParseResult::NativeFunctionDeclarationMismatch:
        return "NativeFunctionDeclarationMismatch";
    case ParseResult::UnsafeChainedIfStatement:
        return "UnsafeChainedIfStatement";
    case ParseResult::FileSystemNotAvailable:
        return "FileSystemNotAvailable";
    case ParseResult::UsageOfUndeclaredLabel:
        return "UsageOfUndeclaredLabel";
    case ParseResult::IllegalUsageOfLabel:
        return "IllegalUsageOfLabel";
    case ParseResult::LabelNotAllowedInContext:
        return "LabelNotAllowedInContext";
    case ParseResult::RedeclarationOfLabel:
        return "RedeclarationOfLabel";
    case ParseResult::TerminatedByNotification:
        return "TerminatedByNotification";
    }
    return "Unknown";
}

const char* Pulsar::ParseWarningToString(ParseWarning warning)
{
    switch (warning) {
    case ParseWarning::None:
        return "None";
    case ParseWarning::DuplicateFunctionNames:
        return "DuplicateFunctionNames";
    }
    return "Unknown";
}
