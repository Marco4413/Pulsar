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

Pulsar::ParseResult Pulsar::Parser::SetError(ParseResult errorType, const Token& token, const String& errorMsg)
{
    m_ErrorSource = CurrentSource();
    m_ErrorPath = CurrentPath();
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
    m_ParsedSources.Clear();
    m_LexerPool.Clear();
}

bool Pulsar::Parser::AddSource(const String& path, const String& src)
{
    ClearError();
    if (path.Length() > 0) {
        if (m_ParsedSources.Find(path))
            return false;
        m_ParsedSources.Emplace(path);
    }

    LexerSource& lexSource = m_LexerPool.EmplaceBack(path, src, Lexer(""));
    lexSource.Lexer = Lexer(lexSource.Source);

    m_Lexer = &m_LexerPool.Back().Lexer;
    m_Lexer->SkipShaBang();
    return true;
}

bool Pulsar::Parser::AddSource(const String& path, String&& src)
{
    ClearError();
    if (path.Length() > 0) {
        if (m_ParsedSources.Find(path))
            return false;
        m_ParsedSources.Emplace(path);
    }

    LexerSource& lexSource = m_LexerPool.EmplaceBack(path, std::move(src), Lexer(""));
    lexSource.Lexer = Lexer(lexSource.Source);

    m_Lexer = &m_LexerPool.Back().Lexer;
    m_Lexer->SkipShaBang();
    return true;
}

Pulsar::ParseResult Pulsar::Parser::AddSourceFile(const String& path)
{
    Token token = CurrentToken();
#ifdef PULSAR_NO_FILESYSTEM
    return SetError(ParseResult::FileSystemNotAvailable, token, "Could not read '" + path + "' because filesystem was disabled.");
#else // PULSAR_NO_FILESYSTEM
    auto rawPath = std::filesystem::path(path.CString());

    std::error_code error;
    auto normalizedPath = std::filesystem::relative(rawPath, error);

    if (error || normalizedPath.empty()) {
        // If path is empty it may be located on a different drive on Windows.
        // In which case we should compute the canonical path.
        normalizedPath = std::filesystem::canonical(rawPath, error);
    }

    if (error) {
        return SetError(ParseResult::FileNotRead, token, "Could not normalize path '" + path + "'.");
    } else if (normalizedPath.empty()) {
        return SetError(ParseResult::FileNotRead, token, "Could not resolve file '" + path + "'.");
    }

    String internalPath = normalizedPath.generic_string().c_str();
    if (!std::filesystem::exists(normalizedPath))
        return SetError(ParseResult::FileNotRead, token, "File '" + internalPath + "' does not exist.");
    
    std::ifstream file(normalizedPath, std::ios::binary);
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
        for (size_t i = 0; i < m_LexerPool.Size(); i++) {
            globalScope.SourceDebugSymbols.Emplace(m_LexerPool[i].Path, module.SourceDebugSymbols.Size());
            module.SourceDebugSymbols.EmplaceBack(m_LexerPool[i].Path, m_LexerPool[i].Source);
        }
    }
    for (size_t i = 0; i < module.Functions.Size(); i++)
        globalScope.Functions.Insert(module.Functions[i].Name, i);
    for (size_t i = 0; i < module.NativeBindings.Size(); i++)
        globalScope.NativeFunctions.Insert(module.NativeBindings[i].Name, i);
    for (size_t i = 0; i < module.Globals.Size(); i++)
        globalScope.Globals.Insert(module.Globals[i].Name, i);

    while (m_LexerPool.Size() > 0) {
        auto result = ParseModuleStatement(module, globalScope, settings);
        if (result != ParseResult::OK)
            return result;
        else if (IsEndOfFile()) {
            // No need to call ClearError because Result was OK
            m_LexerPool.PopBack();
            m_Lexer = m_LexerPool.IsEmpty() ? nullptr : &m_LexerPool.Back().Lexer;
        }
    }
    module.NativeFunctions.Resize(module.NativeBindings.Size(), nullptr);
    return ParseResult::OK;
}

Pulsar::ParseResult Pulsar::Parser::ParseModuleStatement(Module& module, GlobalScope& globalScope, const ParseSettings& settings)
{
    const Token& curToken = NextToken();
    switch (curToken.Type) {
    case TokenType::Star:
        return ParseFunctionDefinition(module, globalScope, settings);
    case TokenType::CompilerDirective: {
        if (curToken.IntegerVal != TOKEN_CD_INCLUDE)
            return SetError(ParseResult::UnexpectedToken, curToken, "Unknown compiler directive.");
        else if (!settings.AllowIncludeDirective)
            return SetError(ParseResult::IllegalDirective, curToken, "Include compiler directive was disabled.");
        NextToken();
        if (curToken.Type != TokenType::StringLiteral) {
            return SetError(ParseResult::UnexpectedToken, curToken, "Expected file path.");
        } else if (settings.IncludeResolver) {
            const String* cwf = CurrentPath();
            PULSAR_ASSERT(cwf != nullptr, "CWF should not be nullptr.");
            auto res = settings.IncludeResolver(*this, *cwf, curToken);
            if (res != ParseResult::OK)
                return res;
        } else {
#ifdef PULSAR_NO_FILESYSTEM
            return SetError(ParseResult::FileSystemNotAvailable, curToken, "No custom include resolver provided.");
#else // PULSAR_NO_FILESYSTEM
            const String* cwf = CurrentPath();
            PULSAR_ASSERT(cwf != nullptr, "CWF should not be nullptr.");
            std::filesystem::path targetPath(curToken.StringVal.CString());
            std::filesystem::path workingPath(cwf->CString());
            std::filesystem::path filePath = workingPath.parent_path() / targetPath;
            auto result = AddSourceFile(filePath.generic_string().data());
            if (result != ParseResult::OK)
                return result;
#endif // PULSAR_NO_FILESYSTEM
        }
        if (settings.StoreDebugSymbols) {
            const String* path = CurrentPath();
            PULSAR_ASSERT(path != nullptr, "Path should not be nullptr.");
            const String* source = CurrentSource();
            PULSAR_ASSERT(source != nullptr, "Source should not be nullptr.");
            globalScope.SourceDebugSymbols.Emplace(*path, module.SourceDebugSymbols.Size());
            module.SourceDebugSymbols.EmplaceBack(*path, *source);
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
    
    NextToken();
    Token constToken = curToken;
    bool isConstant = constToken.Type == TokenType::KW_Const;
    if (isConstant) NextToken();

    FunctionDefinition dummyFunc{"", 0, 1};
    bool isProducer = curToken.Type == TokenType::RightArrow;

    if (!isProducer) {
        ParseSettings subSettings = settings;
        // We want to know where, within the body of the global, the runtime failed.
        subSettings.StoreDebugSymbols = true;
        LocalScope localScope{
            .Global = globalScope,
            .Function = nullptr,
        };
        auto result = PushLValue(module, dummyFunc, localScope, curToken, subSettings);
        if (result != ParseResult::OK)
            return result;
        NextToken();
    }
    
    if (curToken.Type != TokenType::RightArrow)
        return SetError(ParseResult::UnexpectedToken, curToken, "Expected '->' to assign global value.");
    NextToken();
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

    if (isProducer) {
        NextToken();
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
        auto result = ParseFunctionBody(module, dummyFunc, localScope, nullptr, false, subSettings);
        if (result != ParseResult::OK)
            return result;
        result = BackPatchFunctionLabels(dummyFunc, functionScope);
        if (result != ParseResult::OK)
            return result;
    }

    // Assign name after ParseFunctionBody to prevent self-recursion
    dummyFunc.Name = "{g";
    dummyFunc.Name += identToken.StringVal;
    dummyFunc.Name += '}';

    ExecutionContext context(module);
    ValueStack& stack = context.GetStack();
    auto evalResult = RuntimeState::OK;

    if (isProducer && settings.MapGlobalProducersToVoid) {
        stack.EmplaceBack().SetVoid();
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
        if (settings.AppendNotesToErrorMessage
            && evalResult == RuntimeState::NativeFunctionBindingsMismatch)
            errorMsg += "\nNote: Native functions may not be bound during parsing.";
        return SetError(
            ParseResult::GlobalEvaluationError,
            dummyFunc.CodeDebugSymbols[symbolIdx].Token,
            std::move(errorMsg));
    }

    PULSAR_ASSERT(stack.Size() > 0, "Global producer did not match return count.");
    
    GlobalDefinition* globalDef;
    if (!globalNameIdxPair) {
        globalScope.Globals.Emplace(identToken.StringVal, module.Globals.Size());
        globalDef = &module.Globals.EmplaceBack(std::move(identToken.StringVal), std::move(stack.Back()), isConstant);
    } else {
        globalDef = &module.Globals[globalNameIdxPair->Value()];
        globalDef->InitialValue = std::move(stack.Back());
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

    NextToken();
    if (curToken.Type != TokenType::OpenParenth)
        return SetError(ParseResult::UnexpectedToken, curToken, "Expected '(' to open function name and args declaration.");
    NextToken();
    bool isNative = curToken.Type == TokenType::Star;
    if (isNative) NextToken();

    if (curToken.Type != TokenType::Identifier)
        return SetError(ParseResult::UnexpectedToken, curToken, "Expected function identifier.");
    Token identToken = curToken;
    FunctionDefinition def = { identToken.StringVal, 0, 0 };
    if (settings.StoreDebugSymbols) {
        const String* path = CurrentPath();
        PULSAR_ASSERT(path != nullptr, "Path should not be nullptr.");
        auto sourcePathIdxPair = globalScope.SourceDebugSymbols.Find(*path);
        def.DebugSymbol.Token = identToken;
        def.DebugSymbol.SourceIdx = sourcePathIdxPair ? sourcePathIdxPair->Value() : ~(size_t)0;
    }

    NextToken();
    if (curToken.Type == TokenType::IntegerLiteral) {
        def.StackArity = (size_t)curToken.IntegerVal;
        NextToken();
    }

    List<LocalScope::LocalVar> args;
    for (;;) {
        if (curToken.Type != TokenType::Identifier)
            break;
        args.EmplaceBack(std::move(curToken.StringVal), curToken.SourcePos);
        NextToken();
    }
    def.Arity = args.Size();
    def.LocalsCount = args.Size();

    if (curToken.Type != TokenType::CloseParenth)
        return SetError(ParseResult::UnexpectedToken, curToken, "Expected ')' to close function name and args declaration.");
    NextToken();
    if (curToken.Type == TokenType::RightArrow) {
        NextToken();
        if (curToken.Type != TokenType::IntegerLiteral)
            return SetError(ParseResult::UnexpectedToken, curToken, "Expected return count.");
        else if (curToken.IntegerVal < 0)
            return SetError(ParseResult::NegativeResultCount, curToken, "Illegal return count. Return count must be >= 0");
        def.Returns = (size_t)curToken.IntegerVal;
        NextToken();
    }

    if (isNative) {
        if (curToken.Type != TokenType::FullStop)
            return SetError(ParseResult::UnexpectedToken, curToken,
                "Expected '.' to complete native function declaration. Native functions can't have a body.");
        // If the native already exists push symbols (the function may have been defined outside the Parser)
        auto nameIdxPair = globalScope.NativeFunctions.Find(def.Name);
        if (!nameIdxPair) {
            NOTIFY_FUNCTION_DEFINITION(false, true, module.NativeBindings.Size(), def, identToken, args, settings);
            globalScope.NativeFunctions.Emplace(def.Name, module.NativeBindings.Size());
            module.NativeBindings.EmplaceBack(std::move(def));
        } else {
            size_t nativeIdx = (int64_t)nameIdxPair->Value();
            FunctionDefinition& nativeFunc = module.NativeBindings[nativeIdx];
            if (!nativeFunc.MatchesDeclaration(def))
                return SetError(ParseResult::NativeFunctionRedeclaration, identToken, "Redeclaration of Native Function with different signature.");
            nativeFunc.DebugSymbol = def.DebugSymbol;
            NOTIFY_FUNCTION_DEFINITION(true, true, nativeIdx, nativeFunc, identToken, args, settings);
        }
    } else {
        if (curToken.Type != TokenType::Colon)
            return SetError(ParseResult::UnexpectedToken, curToken, "Expected '->' for return count declaration or ':' to begin function body.");
        NOTIFY_FUNCTION_DEFINITION(false, false, module.Functions.Size(), def, identToken, args, settings);
        globalScope.Functions.Emplace(def.Name, module.Functions.Size());
        FunctionScope functionScope;
        LocalScope localScope{
            .Global = globalScope,
            .Function = &functionScope,
            .Locals = std::move(args),
        };
        ParseResult parseResult = ParseFunctionBody(module, def, localScope, nullptr, false, settings);
        if (parseResult != ParseResult::OK)
            return parseResult;
        parseResult = BackPatchFunctionLabels(def, functionScope);
        if (parseResult != ParseResult::OK)
            return parseResult;
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
    bool allowEndKeyword,
    const ParseSettings& settings)
{
    NOTIFY_SEND_BLOCK_NOTIFICATION(ParserNotifications::BlockNotificationType::BlockStart, func, localScope, settings);
    LocalScope scope = localScope;
    for (;;) {
        const Token& curToken = NextToken();
        switch (curToken.Type) {
        case TokenType::FullStop:
            PUSH_CODE_SYMBOL(settings.StoreDebugSymbols, func, curToken);
            func.Code.EmplaceBack(InstructionCode::Return);
            NOTIFY_SEND_BLOCK_NOTIFICATION(ParserNotifications::BlockNotificationType::BlockEnd, func, scope, settings);
            return ParseResult::OK;
        case TokenType::KW_Break:
            if (!skippableBlock || !skippableBlock->AllowBreak)
                return SetError(ParseResult::UnexpectedToken, curToken, "Trying to break out of an un-breakable block.");
            PUSH_CODE_SYMBOL(settings.StoreDebugSymbols, func, curToken);
            skippableBlock->BreakStatements.PushBack(func.Code.Size());
            func.Code.EmplaceBack(InstructionCode::J, 0);
            NOTIFY_SEND_BLOCK_NOTIFICATION(ParserNotifications::BlockNotificationType::BlockEnd, func, scope, settings);
            return ParseResult::OK;
        case TokenType::KW_Continue:
            if (!skippableBlock || !skippableBlock->AllowContinue)
                return SetError(ParseResult::UnexpectedToken, curToken, "Trying to repeat an un-repeatable block.");
            PUSH_CODE_SYMBOL(settings.StoreDebugSymbols, func, curToken);
            skippableBlock->ContinueStatements.PushBack(func.Code.Size());
            func.Code.EmplaceBack(InstructionCode::J, 0);
            NOTIFY_SEND_BLOCK_NOTIFICATION(ParserNotifications::BlockNotificationType::BlockEnd, func, scope, settings);
            return ParseResult::OK;
        case TokenType::KW_End:
            if (!allowEndKeyword)
                return SetError(ParseResult::UnexpectedToken, curToken, "Cannot use the 'end' keyword to close the current block.");
            NOTIFY_SEND_BLOCK_NOTIFICATION(ParserNotifications::BlockNotificationType::BlockEnd, func, scope, settings);
            return ParseResult::OK;
        case TokenType::KW_Do: {
            auto res = ParseDoBlock(module, func, scope, settings);
            if (res != ParseResult::OK)
                return res;
        } break;
        case TokenType::KW_While: {
            PUSH_CODE_SYMBOL(settings.StoreDebugSymbols, func, curToken);
            auto res = ParseWhileLoop(module, func, scope, settings);
            if (res != ParseResult::OK)
                return res;
        } break;
        case TokenType::KW_Local: {
            PUSH_CODE_SYMBOL(settings.StoreDebugSymbols, func, curToken);
            auto res = ParseLocalBlock(module, func, scope, skippableBlock, settings);
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
        case TokenType::BitShiftLeft:
            PUSH_CODE_SYMBOL(settings.StoreDebugSymbols, func, curToken);
            func.Code.EmplaceBack(InstructionCode::BitShiftLeft);
            break;
        case TokenType::BitShiftRight:
            PUSH_CODE_SYMBOL(settings.StoreDebugSymbols, func, curToken);
            func.Code.EmplaceBack(InstructionCode::BitShiftRight);
            break;
        case TokenType::LeftArrow:
        case TokenType::PushReference:
        case TokenType::OpenBracket:
        case TokenType::StringLiteral:
        case TokenType::IntegerLiteral:
        case TokenType::DoubleLiteral:
        case TokenType::Identifier: {
            auto res = PushLValue(module, func, scope, curToken, settings);
            if (res != ParseResult::OK)
                return res;
        } break;
        case TokenType::Label: {
            if (!settings.AllowLabels)
                return SetError(ParseResult::LabelNotAllowedInContext, curToken, "Labels were disabled.");
            else if (!localScope.Function)
                return SetError(ParseResult::LabelNotAllowedInContext, curToken, "Labels are not allowed within this context.");
            else if (localScope.Function->Labels.Find(curToken.StringVal))
                return SetError(ParseResult::RedeclarationOfLabel, curToken, "Redeclaration of labels is not allowed.");
            localScope.Function->Labels.Emplace(curToken.StringVal, curToken, func.Code.Size());
        } break;
        case TokenType::RightArrow:
        case TokenType::BothArrows: {
            bool copyIntoLocal = curToken.Type == TokenType::BothArrows;
            PUSH_CODE_SYMBOL(settings.StoreDebugSymbols, func, curToken);
            NextToken();
            bool forceBinding = curToken.Type == TokenType::Negate;
            if (forceBinding) NextToken();
            if (curToken.Type != TokenType::Identifier)
                return SetError(ParseResult::UnexpectedToken, curToken, "Expected identifier to create local binding.");

            int64_t localIdx = 0;
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
                if (localIdx < 0) {
                    auto globalNameIdxPair = scope.Global.Globals.Find(curToken.StringVal);
                    if (globalNameIdxPair) {
                        int64_t globalIdx = (int64_t)globalNameIdxPair->Value();
                        if (module.Globals[(size_t)globalIdx].IsConstant)
                            return SetError(ParseResult::UnexpectedToken, curToken, "Trying to assign to constant global.");
                        NOTIFY_IDENTIFIER_USAGE(ParserNotifications::IdentifierUsageType::Global, globalIdx, func, curToken, scope, settings);
                        func.Code.EmplaceBack(
                            copyIntoLocal
                                ? InstructionCode::CopyIntoGlobal
                                : InstructionCode::PopIntoGlobal,
                            globalIdx);
                        break;
                    }
                    localIdx = (int64_t)scope.Locals.Size();
                    scope.Locals.PushBack({
                        .Name = curToken.StringVal,
                        .DeclaredAt = curToken.SourcePos
                    });
                    NOTIFY_SEND_BLOCK_NOTIFICATION(ParserNotifications::BlockNotificationType::LocalScopeChanged, func, scope, settings);
                }
            }
            if (scope.Locals.Size() > func.LocalsCount)
                func.LocalsCount = scope.Locals.Size();
            NOTIFY_IDENTIFIER_USAGE(ParserNotifications::IdentifierUsageType::Local, localIdx, func, curToken, scope, settings);
            func.Code.EmplaceBack(
                copyIntoLocal
                    ? InstructionCode::CopyIntoLocal
                    : InstructionCode::PopIntoLocal,
                localIdx);
        } break;
        case TokenType::OpenParenth: {
            NextToken();
            bool isNative = curToken.Type == TokenType::Star;
            bool isInstruction = curToken.Type == TokenType::Negate;
            if (isNative || isInstruction) NextToken();

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
            NextToken();

            Token argToken(TokenType::None);
            if (isInstruction && (
                curToken.Type == TokenType::IntegerLiteral ||
                curToken.Type == TokenType::Label
            )) {
                argToken = curToken;
                NextToken();
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
        } break;
        case TokenType::KW_If: {
            PUSH_CODE_SYMBOL(settings.StoreDebugSymbols, func, curToken);
            auto res = ParseIfStatement(module, func, scope, skippableBlock, false, settings);
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
    const LocalScope& localScope, SkippableBlock* skippableBlock,
    bool isChained, const ParseSettings& settings)
{
    Token ifToken = CurrentToken();
    Token comparisonToken(TokenType::None);
    InstructionCode jmpInstrCode = InstructionCode::JZ;
    InstructionCode compInstrCode = InstructionCode::Equals;
    // Whether the if condition is fully contained within the statement.
    bool isSelfContained = false;
    bool hasComparison = false;
    bool invertedJump = false;

    const Token& curToken = NextToken();
    if (curToken.Type == TokenType::KW_Not) {
        NextToken();
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
            auto res = PushLValue(module, func, localScope, curToken, settings);
            if (res != ParseResult::OK)
                return res;
            NextToken();
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
            NextToken();
            switch (curToken.Type) {
            case TokenType::StringLiteral:
            case TokenType::IntegerLiteral:
            case TokenType::DoubleLiteral:
            case TokenType::Identifier: {
                auto res = PushLValue(module, func, localScope, curToken, settings);
                if (res != ParseResult::OK)
                    return res;
                NextToken();
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
    else if (hasComparison) {
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
    
    auto res = ParseFunctionBody(module, func, localScope, skippableBlock, true, settings);
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
        if (curToken.Type == TokenType::KW_End)
            return ParseResult::OK;
        // if ... <cmp> ...: ... (continue|break|.)
        NextToken();
        // We either want an else or an end
        if (curToken.Type == TokenType::KW_End) {
            return ParseResult::OK;
        } else if (curToken.Type != TokenType::KW_Else) {
            return SetError(ParseResult::UnexpectedToken, curToken,
                "Expected 'end' to close or 'else' to create a new branch of a self-contained if statement.");
        }
    }

    // If we get here, there's an else branch
    size_t elseIdx = func.Code.Size();
    PUSH_CODE_SYMBOL(settings.StoreDebugSymbols, func, curToken);
    func.Code.EmplaceBack(InstructionCode::J, 0);
    func.Code[ifIdx].Arg0 = func.Code.Size() - ifIdx;
    NextToken(); // Consume 'else' Token

    if (curToken.Type == TokenType::Colon) {
        // else: ...
        res = ParseFunctionBody(module, func, localScope, skippableBlock, true, settings);
        if (res != ParseResult::OK)
            return res;

        if (isSelfContained && curToken.Type != TokenType::KW_End) {
            // ... if ... <cmp> ...:
            //     ...
            // else:
            //     ...
            //     (continue|break|.)
            NextToken();
            if (curToken.Type != TokenType::KW_End)
                return SetError(ParseResult::UnexpectedToken, curToken, "Expected 'end' to close else branch.");
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
    while (NextToken().Type != TokenType::Colon) {
        if (curToken.Type != TokenType::Identifier)
            return SetError(ParseResult::UnexpectedToken, curToken, "Expected name of local.");
        if (!IsDummyIdentifier(curToken.StringVal))
            localNameToIdx.Insert(curToken.StringVal, localNames.Size());
        localNames.EmplaceBack(curToken);
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

    auto res = ParseFunctionBody(module, func, localScope ? *localScope : parentScope, skippableBlock, true, settings);
    if (res != ParseResult::OK)
        return res;

    if (curToken.Type != TokenType::KW_End) {
        NextToken();
        if (curToken.Type != TokenType::KW_End)
            return SetError(ParseResult::UnexpectedToken, curToken, "Expected 'end' to close local block.");
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
    NextToken();
    if (curToken.Type == TokenType::KW_Not) {
        NextToken();
        invertedJump = true;
    }

    if (curToken.Type == TokenType::Colon) {
        if (invertedJump)
            jmpInstrCode = InstructionCode::J;
        else whileTrue = true;
    } else {
        switch (curToken.Type) {
        case TokenType::StringLiteral:
        case TokenType::IntegerLiteral:
        case TokenType::DoubleLiteral:
        case TokenType::Identifier: {
            // jmpInstrCode = InstructionCode::JZ;
            auto res = PushLValue(module, func, localScope, curToken, settings);
            if (res != ParseResult::OK)
                return res;
            NextToken();
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
            NextToken();
            switch (curToken.Type) {
            case TokenType::StringLiteral:
            case TokenType::IntegerLiteral:
            case TokenType::DoubleLiteral:
            case TokenType::Identifier: {
                auto res = PushLValue(module, func, localScope, curToken, settings);
                if (res != ParseResult::OK)
                    return res;
                NextToken();
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
    
    auto res = ParseFunctionBody(module, func, localScope, &block, true, settings);
    if (res != ParseResult::OK)
        return res;

    if (curToken.Type != TokenType::KW_End) {
        NextToken();
        if (curToken.Type != TokenType::KW_End)
            return SetError(ParseResult::UnexpectedToken, curToken, "Expected 'end' to close while loop body.");
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
    NextToken();
    if (curToken.Type != TokenType::Colon)
        return SetError(ParseResult::UnexpectedToken, curToken, "Expected ':' to begin do block body.");

    SkippableBlock block{
        .AllowBreak = true,
        .AllowContinue = true,
    };

    auto res = ParseFunctionBody(module, func, localScope, &block, true, settings);
    if (res != ParseResult::OK)
        return res;

    if (curToken.Type != TokenType::KW_End) {
        NextToken();
        if (curToken.Type != TokenType::KW_End)
            return SetError(ParseResult::UnexpectedToken, curToken, "Expected 'end' to close do block body.");
    }

    for (size_t i = 0; i < block.ContinueStatements.Size(); i++)
        func.Code[block.ContinueStatements[i]].Arg0 = (int64_t)(doIdx-block.ContinueStatements[i]);

    size_t breakIdx = func.Code.Size();
    for (size_t i = 0; i < block.BreakStatements.Size(); i++)
        func.Code[block.BreakStatements[i]].Arg0 = (int64_t)(breakIdx-block.BreakStatements[i]);
    return ParseResult::OK;
}

Pulsar::ParseResult Pulsar::Parser::PushLValue(Module& module, FunctionDefinition& func, const LocalScope& localScope, const Token& lvalue, const ParseSettings& settings)
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
        int64_t localIdx = (int64_t)localScope.Locals.Size()-1;
        for (; localIdx >= 0 && localScope.Locals[(size_t)localIdx].Name != lvalue.StringVal; localIdx--);
        if (localIdx < 0) {
            auto globalNameIdxPair = localScope.Global.Globals.Find(lvalue.StringVal);
            if (!globalNameIdxPair)
                return SetError(ParseResult::UsageOfUndeclaredLocal, lvalue, "Local not declared.");
            int64_t globalIdx = (int64_t)globalNameIdxPair->Value();
            NOTIFY_IDENTIFIER_USAGE(ParserNotifications::IdentifierUsageType::Global, globalIdx, func, lvalue, localScope, settings);
            func.Code.EmplaceBack(InstructionCode::PushGlobal, globalIdx);
            break;
        }
        NOTIFY_IDENTIFIER_USAGE(ParserNotifications::IdentifierUsageType::Local, localIdx, func, lvalue, localScope, settings);
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
        const Token& curToken = NextToken();
        if (curToken.Type == TokenType::Identifier) {
            return SetError(ParseResult::UnexpectedToken, curToken, "Local reference is not supported, expected (function).");
        } else if (curToken.Type == TokenType::OpenParenth) {
            NextToken();
            bool isNative = curToken.Type == TokenType::Star;
            if (isNative) NextToken();
            if (curToken.Type != TokenType::Identifier)
                return SetError(ParseResult::UnexpectedToken, curToken, "Expected (function) name.");
            Token identToken = curToken;
            NextToken();
            if (curToken.Type != TokenType::CloseParenth)
                return SetError(ParseResult::UnexpectedToken, curToken, "Expected ')' to close function reference.");
        
            if (isNative) {
                auto nativeNameIdxPair = localScope.Global.NativeFunctions.Find(identToken.StringVal);
                if (!nativeNameIdxPair)
                    return SetError(ParseResult::UsageOfUndeclaredNativeFunction, identToken, "Native function not declared.");
                int64_t funcIdx = (int64_t)nativeNameIdxPair->Value();
                NOTIFY_IDENTIFIER_USAGE(ParserNotifications::IdentifierUsageType::NativeFunction, funcIdx, func, identToken, localScope, settings);
                func.Code.EmplaceBack(InstructionCode::PushNativeFunctionReference, funcIdx);
                break;
            }

            auto funcNameIdxPair = localScope.Global.Functions.Find(identToken.StringVal);
            if (!funcNameIdxPair)
                return SetError(ParseResult::UsageOfUndeclaredFunction, identToken, "Function not declared.");
            int64_t funcIdx = (int64_t)funcNameIdxPair->Value();
            NOTIFY_IDENTIFIER_USAGE(ParserNotifications::IdentifierUsageType::Function, funcIdx, func, identToken, localScope, settings);
            func.Code.EmplaceBack(InstructionCode::PushFunctionReference, funcIdx);
        } else return SetError(ParseResult::UnexpectedToken, curToken, "Expected (function) to reference.");
    } break;
    case TokenType::LeftArrow: {
        PUSH_CODE_SYMBOL(settings.StoreDebugSymbols, func, lvalue);
        const Token& curToken = NextToken();
        if (curToken.Type != TokenType::Identifier)
            return SetError(ParseResult::UnexpectedToken, curToken, "Expected local name.");

        int64_t localIdx = (int64_t)localScope.Locals.Size()-1;
        for (; localIdx >= 0 && localScope.Locals[(size_t)localIdx].Name != curToken.StringVal; localIdx--);
        if (localIdx < 0) {
            auto globalNameIdxPair = localScope.Global.Globals.Find(lvalue.StringVal);
            if (!globalNameIdxPair)
                return SetError(ParseResult::UsageOfUndeclaredLocal, lvalue, "Local not declared.");
            if (module.Globals[globalNameIdxPair->Value()].IsConstant)
                return SetError(ParseResult::WritingToConstantGlobal, curToken, "Cannot move constant global.");
            int64_t globalIdx = (int64_t)globalNameIdxPair->Value();
            NOTIFY_IDENTIFIER_USAGE(ParserNotifications::IdentifierUsageType::Global, globalIdx, func, lvalue, localScope, settings);
            func.Code.EmplaceBack(InstructionCode::MoveGlobal, globalIdx);
            break;
        }
        NOTIFY_IDENTIFIER_USAGE(ParserNotifications::IdentifierUsageType::Local, localIdx, func, lvalue, localScope, settings);
        func.Code.EmplaceBack(InstructionCode::MoveLocal, localIdx);
    } break;
    case TokenType::OpenBracket: {
        int64_t listSize = 0;
        const Token& curToken = NextToken();
        while (true) {
            switch (curToken.Type) {
            case TokenType::IntegerLiteral:
            case TokenType::DoubleLiteral:
            case TokenType::LeftArrow:
            case TokenType::PushReference:
            case TokenType::StringLiteral:
            case TokenType::Identifier:
            case TokenType::OpenBracket: {
                auto res = PushLValue(module, func, localScope, curToken, settings);
                if (res != ParseResult::OK)
                    return res;
                ++listSize;
            } break;
            case TokenType::CloseBracket:
                if (listSize <= 0) {
                    // Empty List
                    func.Code.EmplaceBack(InstructionCode::PushEmptyList);
                    return ParseResult::OK;
                }

                PUSH_CODE_SYMBOL(settings.StoreDebugSymbols, func, curToken);
                func.Code.EmplaceBack(InstructionCode::Pack, listSize);
                return ParseResult::OK;
            default:
                return SetError(ParseResult::UnexpectedToken, curToken, "Expected lvalue.");
            }
            
            NextToken();
            if (curToken.Type == TokenType::Comma) {
                NextToken();
            } else if (curToken.Type != TokenType::CloseBracket)
                return SetError(ParseResult::UnexpectedToken, curToken, "Expected ',' to continue List literal or ']' to close it.");
        }
    } break;
    default:
        return SetError(ParseResult::UnexpectedToken, lvalue, "Expected lvalue.");
    }
    return ParseResult::OK;
}

const Pulsar::Token& Pulsar::Parser::NextToken()
{
    return m_CurrentToken = m_Lexer->NextToken();
}

const Pulsar::Token& Pulsar::Parser::CurrentToken() const
{
    return m_CurrentToken;
}

bool Pulsar::Parser::IsEndOfFile() const
{
    return m_Lexer->IsEndOfFile();
}

const Pulsar::String* Pulsar::Parser::CurrentPath() const
{
    if (m_LexerPool.Size() > 0)
        return &m_LexerPool.Back().Path;
    return nullptr;
}

const Pulsar::String* Pulsar::Parser::CurrentSource() const
{
    if (m_LexerPool.Size() > 0)
        return &m_LexerPool.Back().Source;
    return nullptr;
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
    case ParseResult::NativeFunctionRedeclaration:
        return "NativeFunctionRedeclaration";
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
