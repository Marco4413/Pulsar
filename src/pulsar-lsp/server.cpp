#include "pulsar-lsp/server.h"

#include <filesystem>
#include <fstream>

#include <lsp/messages.h>

const char* ValueTypeToString(Pulsar::ValueType type)
{
    switch (type) {
    case Pulsar::ValueType::Void:
        return "Void";
    case Pulsar::ValueType::Integer:
        return "Integer";
    case Pulsar::ValueType::Double:
        return "Double";
    case Pulsar::ValueType::FunctionReference:
        return "FunctionReference";
    case Pulsar::ValueType::NativeFunctionReference:
        return "NativeFunctionReference";
    case Pulsar::ValueType::List:
        return "List";
    case Pulsar::ValueType::String:
        return "String";
    case Pulsar::ValueType::Custom:
        return "Custom";
    default:
        return "Unknown";
    }
}

lsp::Range SourcePositionToRange(Pulsar::SourcePosition pos)
{
    return lsp::Range{
        .start = {
            .line = (lsp::uint)pos.Line,
            .character = (lsp::uint)pos.Char
        },
        .end = {
            .line = (lsp::uint)pos.Line,
            .character = (lsp::uint)(pos.Char+pos.CharSpan)
        }
    };
}

bool IsPositionAfter(lsp::Position pos, Pulsar::SourcePosition marker)
{
    return (pos.line == marker.Line && pos.character >= marker.Char)
        || pos.line > marker.Line;
}

bool IsPositionBefore(lsp::Position pos, Pulsar::SourcePosition marker)
{
    return (pos.line == marker.Line && pos.character <= (marker.Char+marker.CharSpan))
        || pos.line < marker.Line;
}

bool IsPositionInBetween(lsp::Position pos, Pulsar::SourcePosition start, Pulsar::SourcePosition end)
{
    return IsPositionAfter(pos, start) && IsPositionBefore(pos, end);
}

bool IsPositionInBetween(lsp::Position pos, Pulsar::SourcePosition span)
{
    return IsPositionInBetween(pos, span, span);
}

Pulsar::String PulsarLSP::URIToNormalizedPath(const lsp::FileURI& uri)
{
    auto fsPath = std::filesystem::path(uri.path());
    std::error_code error;
    auto relPath = std::filesystem::relative(fsPath, error);
    Pulsar::String path = !(error || relPath.empty())
        ? relPath.generic_string().c_str()
        : fsPath.generic_string().c_str();
    return path;
}

lsp::FileURI PulsarLSP::NormalizedPathToURI(const Pulsar::String& path)
{
    std::filesystem::path relPath(path.Data());
    std::error_code error;
    std::filesystem::path absPath = std::filesystem::absolute(relPath, error);
    lsp::FileURI uri = !(error || absPath.empty())
        ? absPath.string()
        : relPath.string();
    return uri;
}

std::optional<PulsarLSP::ParsedDocument> PulsarLSP::ParsedDocument::From(const lsp::FileURI& uri, bool extractAll, UserProvidedOptions opt)
{
    Pulsar::String internalPath  = URIToNormalizedPath(uri);
    std::filesystem::path fsPath = internalPath.Data();
    if (!std::filesystem::exists(fsPath)) return {};

    std::ifstream file(fsPath, std::ios::binary);
    size_t fileSize = (size_t)std::filesystem::file_size(fsPath);

    std::string document;
    document.resize(fileSize);
    if (!file.read(document.data(), fileSize)) return {};

    return FromInMemory(uri, document, extractAll, opt);
}

std::optional<PulsarLSP::ParsedDocument> PulsarLSP::ParsedDocument::FromInMemory(const lsp::FileURI& uri, const std::string& document, bool extractAll, UserProvidedOptions opt)
{
    ParsedDocument parsedDocument;

    Pulsar::String path = URIToNormalizedPath(uri);
    Pulsar::Parser parser;

    Pulsar::HashMap<Pulsar::String, FunctionScope> functionScopes;
    Pulsar::List<FunctionDefinition> functionDefinitions;

    Pulsar::ParseSettings settings = Pulsar::ParseSettings_Default;
    settings.StoreDebugSymbols        = true;
    settings.MapGlobalProducersToVoid = opt.MapGlobalProducersToVoid;
    settings.LSPHooks.OnBlockNotification = [&path, extractAll, &functionScopes](Pulsar::LSPHooks::OnBlockNotificationParams&& params) {
        if (!extractAll && params.FilePath != path) return false;

        switch (params.Type) {
        case Pulsar::LSPBlockNotificationType::BlockStart: {
            Pulsar::String fnId = params.FilePath + ":" + params.FnDefinition.Name;
            auto fnPair = functionScopes.Find(fnId);
            if (fnPair) {
                auto& fn = fnPair->Value();
                if (fn.LocalScopes.Size() > 0) {
                    fn.LocalScopes.Back().EndPos = params.Position;
                }
                fn.LocalScopes.EmplaceBack(LocalScope{
                    .StartPos = params.Position,
                    .EndPos   = params.Position,
                    .Locals   = params.LocalScope.Locals,
                });
            } else {
                auto& fn = functionScopes.Emplace(fnId).Value();
                fn.FilePath = params.FilePath;
                fn.Name = params.FnDefinition.Name;
                params.LocalScope.Global.Globals.ForEach([&fn](const auto& bucket) {
                    fn.Globals.EmplaceBack(bucket.Key(), bucket.Value());
                });
                params.LocalScope.Global.Functions.ForEach([&fn](const auto& bucket) {
                    fn.Functions.EmplaceBack(bucket.Key(), bucket.Value());
                });
                params.LocalScope.Global.NativeFunctions.ForEach([&fn](const auto& bucket) {
                    fn.NativeFunctions.EmplaceBack(bucket.Key(), bucket.Value());
                });
                fn.LocalScopes.EmplaceBack(LocalScope{
                    .StartPos = params.Position,
                    .EndPos   = params.Position,
                    .Locals   = params.LocalScope.Locals,
                });
            }
        } break;
        case Pulsar::LSPBlockNotificationType::BlockEnd: {
            Pulsar::String fnId = params.FilePath + ":" + params.FnDefinition.Name;
            auto fnPair = functionScopes.Find(fnId);
            if (fnPair) {
                auto& fn = fnPair->Value();
                if (fn.LocalScopes.Size() > 0) {
                    fn.LocalScopes.Back().EndPos = params.Position;
                }
            }
        } break;
        case Pulsar::LSPBlockNotificationType::LocalScopeChanged: {
            Pulsar::String fnId = params.FilePath + ":" + params.FnDefinition.Name;
            auto fnPair = functionScopes.Find(fnId);
            if (fnPair) {
                auto& fn = fnPair->Value();
                if (fn.LocalScopes.Size() > 0) {
                    fn.LocalScopes.Back().EndPos = params.Position;
                }
                fn.LocalScopes.EmplaceBack(LocalScope{
                    .StartPos = params.Position,
                    .EndPos = params.Position,
                    .Locals = params.LocalScope.Locals,
                });
            }
        } break;
        default: break;
        }

        return false;
    };
    settings.LSPHooks.OnIdentifierUsage = [&path, extractAll, &functionScopes](Pulsar::LSPHooks::OnIdentifierUsageParams&& params) {
        if (!extractAll && params.FilePath != path) return false;

        Pulsar::String fnId = params.FilePath + ":" + params.FnDefinition.Name;
        auto fnPair = functionScopes.Find(fnId);
        if (params.FnDefinition.Name.Length() <= 0 && !fnPair) {
            // Create global scope
            fnPair = &functionScopes.Emplace(fnId);
            auto& globalCtx = fnPair->Value();
            globalCtx.FilePath = params.FilePath;
            globalCtx.Name = "";
        }

        auto& fn = fnPair->Value();
        IdentifierUsage& identUsage = fn.UsedIdentifiers.EmplaceBack(IdentifierUsage{
            .Identifier = params.Token,
            .Type = params.Type,
            .BoundIndex = params.BoundIdx,
        });
        if (params.Type == Pulsar::LSPIdentifierUsageType::Local && params.BoundIdx < params.LocalScope.Locals.Size())
            identUsage.LocalDeclaredAt = params.LocalScope.Locals[params.BoundIdx].DeclaredAt;

        return false;
    };
    settings.LSPHooks.OnFunctionDefinition = [&parsedDocument](Pulsar::LSPHooks::OnFunctionDefinitionParams&& params) {
        parsedDocument.FunctionDefinitions.PushBack(FunctionDefinition{
            .FilePath   = params.FilePath,
            .IsNative   = params.IsNative,
            .Index      = params.Index,
            .Definition = params.FnDefinition,
            .Args       = params.Args,
        });

        return false;
    };

    Pulsar::String pulsarDocument(document.c_str());
    if (!parser.AddSource(path, std::move(pulsarDocument))) return {};
    parsedDocument.ParseResult = parser.ParseIntoModule(parsedDocument.Module, settings);
    if (parsedDocument.ParseResult != Pulsar::ParseResult::OK) {
        parsedDocument.ErrorFilePath = *parser.GetErrorPath();
        parsedDocument.ErrorPosition = parser.GetErrorToken().SourcePos;
        parsedDocument.ErrorMessage  = parser.GetErrorMessage();
    }

    functionScopes.ForEach([&parsedDocument](auto& bucket) {
        parsedDocument.FunctionScopes.EmplaceBack(std::move(bucket.Value()));
    });

    return parsedDocument;
}

Pulsar::SharedRef<PulsarLSP::ParsedDocument> PulsarLSP::Server::GetDocument(const lsp::FileURI& uri) const
{
    Pulsar::String path = URIToNormalizedPath(uri);
    auto doc = m_DocumentCache.Find(path);
    if (!doc) return nullptr;
    return doc->Value();
}

Pulsar::SharedRef<PulsarLSP::ParsedDocument> PulsarLSP::Server::GetOrParseDocument(const lsp::FileURI& uri, bool forceCache)
{
    if (!forceCache) {
        auto doc = GetDocument(uri);
        if (doc) return doc;
    }

    return ParseAndStoreDocument(uri);
}

void PulsarLSP::Server::DropDocument(const lsp::FileURI& uri)
{
    Pulsar::String path = URIToNormalizedPath(uri);
    m_DocumentCache.Remove(path);
}

void PulsarLSP::Server::DropAllDocuments()
{
    m_DocumentCache.Clear();
}

std::optional<lsp::Location> PulsarLSP::Server::FindDeclaration(const lsp::FileURI& uri, lsp::Position pos)
{
    auto doc = GetOrParseDocument(uri);
    if (!doc || !doc->Module.HasSourceDebugSymbols()) return {};

    Pulsar::String path = URIToNormalizedPath(uri);
    for (size_t i = 0; i < doc->FunctionScopes.Size(); ++i) {
        const FunctionScope& fnScope = doc->FunctionScopes[i];
        if (fnScope.FilePath != path) continue;
        for (size_t j = 0; j < fnScope.UsedIdentifiers.Size(); ++j) {
            const IdentifierUsage& identUsage = fnScope.UsedIdentifiers[j];
            if (IsPositionInBetween(pos, identUsage.Identifier.SourcePos)) {
                switch (identUsage.Type) {
                case Pulsar::LSPIdentifierUsageType::Global: {
                    if (identUsage.BoundIndex >= doc->Module.Globals.Size()) break;
                    
                    const Pulsar::GlobalDefinition global = doc->Module.Globals[identUsage.BoundIndex];
                    if (!global.HasDebugSymbol()) break;
                    
                    if (global.DebugSymbol.SourceIdx >= doc->Module.SourceDebugSymbols.Size()) break;
                    const Pulsar::SourceDebugSymbol& sourceSymbol = doc->Module.SourceDebugSymbols[global.DebugSymbol.SourceIdx];
                    
                    lsp::FileURI locUri = NormalizedPathToURI(sourceSymbol.Path);
                    return lsp::Location{
                        .uri = locUri,
                        .range = SourcePositionToRange(global.DebugSymbol.Token.SourcePos)
                    };
                }
                case Pulsar::LSPIdentifierUsageType::Function: {
                    if (identUsage.BoundIndex >= doc->Module.Functions.Size()) break;
                    
                    const Pulsar::FunctionDefinition func = doc->Module.Functions[identUsage.BoundIndex];
                    if (!func.HasDebugSymbol()) break;
                    
                    if (func.DebugSymbol.SourceIdx >= doc->Module.SourceDebugSymbols.Size()) break;
                    const Pulsar::SourceDebugSymbol& sourceSymbol = doc->Module.SourceDebugSymbols[func.DebugSymbol.SourceIdx];
                    
                    lsp::FileURI locUri = NormalizedPathToURI(sourceSymbol.Path);
                    return lsp::Location{
                        .uri = locUri,
                        .range = SourcePositionToRange(func.DebugSymbol.Token.SourcePos)
                    };
                }
                case Pulsar::LSPIdentifierUsageType::NativeFunction: {
                    if (identUsage.BoundIndex >= doc->Module.NativeBindings.Size()) break;
                    
                    const Pulsar::FunctionDefinition func = doc->Module.NativeBindings[identUsage.BoundIndex];
                    if (!func.HasDebugSymbol()) break;
                    
                    if (func.DebugSymbol.SourceIdx >= doc->Module.SourceDebugSymbols.Size()) break;
                    const Pulsar::SourceDebugSymbol& sourceSymbol = doc->Module.SourceDebugSymbols[func.DebugSymbol.SourceIdx];
                    
                    lsp::FileURI locUri = NormalizedPathToURI(sourceSymbol.Path);
                    return lsp::Location{
                        .uri = locUri,
                        .range = SourcePositionToRange(func.DebugSymbol.Token.SourcePos)
                    };
                }
                case Pulsar::LSPIdentifierUsageType::Local: {
                    lsp::FileURI locUri = NormalizedPathToURI(fnScope.FilePath);
                    return lsp::Location{
                        .uri = locUri,
                        .range = SourcePositionToRange(identUsage.LocalDeclaredAt)
                    };
                }
                default: break;
                }
                return {};
            }
        }
    }
    return {};
}

lsp::CompletionItem PulsarLSP::CreateCompletionItemForBoundEntity(ParsedDocument::SharedRef doc, const BoundGlobal& global)
{
    lsp::CompletionItem item{};
    item.label = global.Name.Data();
    item.kind  = lsp::CompletionItemKind::Variable;

    std::string detail;
    if (global.Index < doc->Module.Globals.Size()) {
        const Pulsar::GlobalDefinition& glblDef = doc->Module.Globals[global.Index];
        if (glblDef.IsConstant) {
            detail += "const ";
        }
        detail += ValueTypeToString(glblDef.InitialValue.Type());
        detail += " -> ";
        detail += glblDef.Name.Data();
    }
    item.detail = std::move(detail);

    return item;
}

void InsertFunctionDefinitionDetails(lsp::CompletionItem& item, const PulsarLSP::FunctionDefinition& lspDef)
{
    const Pulsar::FunctionDefinition& def = lspDef.Definition;

    std::string detail = "(";
    if (lspDef.IsNative) detail += "*";

    detail += def.Name.Data();
    if (def.StackArity > 0) {
        detail += " ";
        detail += std::to_string(def.StackArity);
    }

    for (size_t i = 0; i < lspDef.Args.Size(); ++i) {
        detail += " ";
        detail += lspDef.Args[i].Name.Data();
    }

    detail += ")";
    if (def.Returns > 0) {
        detail += " -> ";
        detail += std::to_string(def.Returns);
    }

    item.detail = std::move(detail);
}

lsp::CompletionItem PulsarLSP::CreateCompletionItemForBoundEntity(ParsedDocument::SharedRef doc, const BoundFunction& fn)
{
    lsp::CompletionItem item{};
    item.label  = "(";
    item.label += fn.Name.Data();
    item.label += ")";
    item.kind   = lsp::CompletionItemKind::Function;

    for (size_t i = 0; i < doc->FunctionDefinitions.Size(); ++i) {
        const FunctionDefinition& lspDef = doc->FunctionDefinitions[i];
        if (!lspDef.IsNative && lspDef.Index == fn.Index) {
            InsertFunctionDefinitionDetails(item, lspDef);
            break;
        }
    }

    return item;
}

lsp::CompletionItem PulsarLSP::CreateCompletionItemForBoundEntity(ParsedDocument::SharedRef doc, const BoundNativeFunction& nativeFn)
{
    lsp::CompletionItem item{};
    item.label  = "(*";
    item.label += nativeFn.Name.Data();
    item.label += ")";
    item.kind   = lsp::CompletionItemKind::Function;

    for (size_t i = 0; i < doc->FunctionDefinitions.Size(); ++i) {
        const FunctionDefinition& lspDef = doc->FunctionDefinitions[i];
        if (lspDef.IsNative && lspDef.Index == nativeFn.Index) {
            InsertFunctionDefinitionDetails(item, lspDef);
            break;
        }
    }

    return item;
}

std::vector<lsp::CompletionItem> PulsarLSP::Server::GetCompletionItems(const lsp::FileURI& uri, lsp::Position pos)
{
    // TODO: Check if pos is within an error token and provide a solution

    auto doc = GetOrParseDocument(uri);
    if (!doc) return {};

    for (size_t i = 0; i < doc->FunctionScopes.Size(); ++i) {
        const LocalScope* cursorLocalScope = nullptr;
        const FunctionScope& fnScope = doc->FunctionScopes[i];
        for (size_t j = fnScope.LocalScopes.Size(); j > 0; --j) {
            const LocalScope& localScope = fnScope.LocalScopes[j-1];
            if (IsPositionInBetween(pos, localScope.StartPos, localScope.EndPos)) {
                cursorLocalScope = &localScope;
                break;
            }
        }

        if (cursorLocalScope) {
            std::vector<lsp::CompletionItem> result;
            for (size_t j = 0; j < fnScope.Functions.Size(); ++j) {
                result.emplace_back(CreateCompletionItemForBoundEntity(doc, fnScope.Functions[j]));
            }
            for (size_t j = 0; j < fnScope.NativeFunctions.Size(); ++j) {
                result.emplace_back(CreateCompletionItemForBoundEntity(doc, fnScope.NativeFunctions[j]));
            }
            for (size_t j = 0; j < fnScope.Globals.Size(); ++j) {
                result.emplace_back(CreateCompletionItemForBoundEntity(doc, fnScope.Globals[j]));
            }
            for (size_t j = 0; j < cursorLocalScope->Locals.Size(); ++j) {
                lsp::CompletionItem item{};
                item.label = cursorLocalScope->Locals[j].Name.Data();
                item.kind  = lsp::CompletionItemKind::Variable;
                result.emplace_back(std::move(item));
            }
            return result;
        }
    }

    return {};
}

std::vector<PulsarLSP::DiagnosticsForDocument> PulsarLSP::Server::GetDiagnosticReport(const lsp::FileURI& uri, bool sameDocument)
{
    auto doc = GetOrParseDocument(uri);
    if (!doc || doc->ParseResult == Pulsar::ParseResult::OK) {
        return {DiagnosticsForDocument{
            .uri = RecomputeURI(uri),
            .diagnostics = {},
            .version = std::nullopt
        }};
    }

    Pulsar::String docPath = URIToNormalizedPath(uri);
    bool isErrorInSameDocument = doc->ErrorFilePath == docPath;
    std::vector<DiagnosticsForDocument> result;
    // No diagnostics for this document
    if (!isErrorInSameDocument) {
        result.push_back(DiagnosticsForDocument{
            .uri = NormalizedPathToURI(docPath),
            .diagnostics = {},
            .version = std::nullopt
        });
    }

    if (sameDocument && !isErrorInSameDocument) return result;

    auto badDoc = isErrorInSameDocument
        ? doc : GetOrParseDocument(NormalizedPathToURI(doc->ErrorFilePath));

    lsp::Diagnostic error;
    error.range    = SourcePositionToRange(badDoc->ErrorPosition);
    error.message  = badDoc->ErrorMessage.Data();
    error.source   = "pulsar";
    error.severity = lsp::DiagnosticSeverity::Error;
    result.push_back(DiagnosticsForDocument{
        .uri = NormalizedPathToURI(badDoc->ErrorFilePath),
        .diagnostics = {std::move(error)},
        .version = std::nullopt
    });

    return result;
}

void PulsarLSP::Server::SendUpdatedDiagnosticReport(lsp::MessageHandler& handler, const lsp::FileURI& uri)
{
    Pulsar::String docPath = URIToNormalizedPath(uri);

    auto& messageDispatcher = handler.messageDispatcher();
    auto diagnostics = this->GetDiagnosticReport(uri, false);
    for (size_t i = 0; i < diagnostics.size(); ++i) {
        messageDispatcher.sendNotification<lsp::notifications::TextDocument_PublishDiagnostics>(
            static_cast<lsp::notifications::TextDocument_PublishDiagnostics::Params>(diagnostics[i]));
    }
}

void PulsarLSP::Server::ResetDiagnosticReport(lsp::MessageHandler& handler, const lsp::FileURI& uri)
{
    auto& messageDispatcher = handler.messageDispatcher();
    messageDispatcher.sendNotification<lsp::notifications::TextDocument_PublishDiagnostics>(
        lsp::notifications::TextDocument_PublishDiagnostics::Params{
            // FIXME: Some editors do not seem to like relative URIs (mayber they're serialized incorrectly by lsp-framework)
            .uri = RecomputeURI(uri),
            .diagnostics = {},
            .version = std::nullopt
        }
    );
}

void PulsarLSP::Server::StripModule(Pulsar::Module& mod) const
{
    // Delete saved sources to save some memory
    for (size_t i = 0; i < mod.SourceDebugSymbols.Size(); ++i) {
        Pulsar::SourceDebugSymbol& srcSymbol = mod.SourceDebugSymbols[i];
        srcSymbol.Source.Resize(0);
        // FIXME: String doesn't allow shrinking of capacity
        srcSymbol.Source.Reserve(0);
    }
}

PulsarLSP::ParsedDocument::SharedRef PulsarLSP::Server::ParseAndStoreDocument(const lsp::FileURI& uri)
{
    auto doc = ParsedDocument::From(uri, false, m_Options);
    if (!doc) {
        // Drop document that failed parsing.
        // This means that the document does not exist.
        DropDocument(uri);
        return nullptr;
    }

    StripModule(doc->Module);

    Pulsar::String path = URIToNormalizedPath(uri);
    return m_DocumentCache.Emplace(path, ParsedDocument::SharedRef::New(std::move(doc.value()))).Value();
}

PulsarLSP::ParsedDocument::SharedRef PulsarLSP::Server::ParseAndStoreInMemoryDocument(const lsp::FileURI& uri, const std::string& document)
{
    // Copy of ::ParseAndStoreDocument
    auto doc = ParsedDocument::FromInMemory(uri, document, false, m_Options);
    if (!doc) {
        DropDocument(uri);
        return nullptr;
    }

    StripModule(doc->Module);

    Pulsar::String path = URIToNormalizedPath(uri);
    return m_DocumentCache.Emplace(path, ParsedDocument::SharedRef::New(std::move(doc.value()))).Value();
}

void PulsarLSP::Server::Run(lsp::Connection& connection)
{
    lsp::MessageHandler messageHandler{connection};

    bool running = true;
    messageHandler.requestHandler()
        .add<lsp::requests::Initialize>([this](const lsp::jsonrpc::MessageId& id, lsp::requests::Initialize::Params&& params)
        {
            (void)id;
            if (params.initializationOptions && params.initializationOptions->isObject()) {
                const auto& userOptions = params.initializationOptions->object();

                if (auto mapGlobalProducersToVoid = userOptions.find("mapGlobalProducersToVoid");
                    mapGlobalProducersToVoid != userOptions.end() &&
                    mapGlobalProducersToVoid->second.isBoolean()
                ) this->m_Options.MapGlobalProducersToVoid = mapGlobalProducersToVoid->second.boolean();
                // TODO: Parse other settings.
            }
            
            lsp::requests::Initialize::Result result;

            lsp::TextDocumentSyncOptions documentSyncOptions{};
            documentSyncOptions.save = lsp::SaveOptions{ .includeText = this->m_Options.ReceiveTextFromClient };
            if (this->m_Options.ReceiveTextFromClient)
                documentSyncOptions.change = lsp::TextDocumentSyncKind::Full;

            lsp::DiagnosticOptions diagnosticOptions{};
            diagnosticOptions.identifier = "pulsar";
            diagnosticOptions.interFileDependencies = true;
            diagnosticOptions.workspaceDiagnostics  = false;

            result.capabilities.textDocumentSync    = documentSyncOptions;
            result.capabilities.completionProvider  = lsp::CompletionOptions{};
            result.capabilities.declarationProvider = true;
            result.capabilities.diagnosticProvider  = diagnosticOptions;
            result.serverInfo = lsp::InitializeResultServerInfo{
                .name = "Pulsar",
                .version = {}
            };
            return result;
        })
        .add<lsp::requests::TextDocument_Completion>([this](const lsp::jsonrpc::MessageId& id, lsp::requests::TextDocument_Completion::Params&& params)
            -> lsp::requests::TextDocument_Completion::Result
        {
            (void)id;
            return this->GetCompletionItems(params.textDocument.uri, params.position);
        })
        .add<lsp::requests::TextDocument_Declaration>([this](const lsp::jsonrpc::MessageId& id, lsp::requests::TextDocument_Declaration::Params&& params)
            -> lsp::requests::TextDocument_Declaration::Result
        {
            (void)id;
            auto decl = this->FindDeclaration(params.textDocument.uri, params.position);
            if (decl.has_value()) return decl.value();
            return nullptr;
        })
        .add<lsp::requests::TextDocument_Diagnostic>([this](const lsp::jsonrpc::MessageId& id, lsp::requests::TextDocument_Diagnostic::Params&& params)
            -> lsp::requests::TextDocument_Diagnostic::Result
        {
            (void)id; (void)params;
            lsp::RelatedFullDocumentDiagnosticReport result;
            return result;
        })
        .add<lsp::notifications::TextDocument_DidSave>([this, &messageHandler](lsp::notifications::TextDocument_DidSave::Params&& params)
        {
            auto doc = this->m_Options.ReceiveTextFromClient && params.text.has_value()
                ? this->ParseAndStoreInMemoryDocument(params.textDocument.uri, *params.text)
                : this->ParseAndStoreDocument(params.textDocument.uri);
            if (this->m_Options.DiagnosticsOnSave) {
                if (doc) {
                    this->SendUpdatedDiagnosticReport(messageHandler, params.textDocument.uri);
                } else {
                    this->ResetDiagnosticReport(messageHandler, params.textDocument.uri);
                }
            }
        })
        .add<lsp::notifications::TextDocument_DidChange>([this, &messageHandler](lsp::notifications::TextDocument_DidChange::Params&& params)
        {
            if (!this->m_Options.ReceiveTextFromClient) return;
            for (size_t i = params.contentChanges.size(); i > 0; --i) {
                const auto& changes = params.contentChanges[i-1];
                if (std::holds_alternative<lsp::TextDocumentContentChangeEvent_Text>(changes)) {
                    const std::string& document = std::get<lsp::TextDocumentContentChangeEvent_Text>(changes).text;
                    auto doc = this->ParseAndStoreInMemoryDocument(params.textDocument.uri, document);
                    if (this->m_Options.DiagnosticsOnChange) {
                        if (doc) {
                            this->SendUpdatedDiagnosticReport(messageHandler, params.textDocument.uri);
                        } else {
                            this->ResetDiagnosticReport(messageHandler, params.textDocument.uri);
                        }
                    }
                }
            }
        })
        .add<lsp::notifications::Exit>([&running]()
        {
            running = false;
        });

    while (running) messageHandler.processIncomingMessages();
}
