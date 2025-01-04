#include "pulsar-lsp/server.h"

#include <filesystem>

#include <lsp/messages.h>
#include <lsp/messagehandler.h>

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

std::optional<PulsarLSP::ParsedDocument> PulsarLSP::ParsedDocument::From(const lsp::FileURI& uri, bool extractAll)
{
    ParsedDocument parsedDocument;

    Pulsar::String path = URIToNormalizedPath(uri);
    Pulsar::Parser parser;

    Pulsar::HashMap<Pulsar::String, Function> functions;

    Pulsar::ParseSettings settings = Pulsar::ParseSettings_Default;
    settings.StoreDebugSymbols = true;
    settings.LSPHooks.OnBlockNotification = [&path, extractAll, &functions](Pulsar::LSPHooks::OnBlockNotificationParams&& params) {
        if (!extractAll && params.FilePath != path) return false;

        switch (params.Type) {
        case Pulsar::LSPBlockNotificationType::BlockStart: {
            Pulsar::String fnId = params.FilePath + ":" + params.FnName;
            auto fnPair = functions.Find(fnId);
            if (fnPair) {
                auto& fn = fnPair->Value();
                if (fn.Scopes.Size() > 0) {
                    fn.Scopes.Back().EndPos = params.Position;
                }
                fn.Scopes.EmplaceBack(Scope{
                    .StartPos = params.Position,
                    .EndPos = params.Position,
                    .Locals = params.LocalScope.Locals,
                });
            } else {
                auto& fn = functions.Emplace(fnId).Value();
                fn.FilePath = params.FilePath;
                fn.Name = params.FnName;
                params.LocalScope.Global.Globals.ForEach([&fn](const auto& bucket) {
                    fn.Globals.EmplaceBack(bucket.Key());
                });
                params.LocalScope.Global.Functions.ForEach([&fn](const auto& bucket) {
                    fn.Functions.EmplaceBack(bucket.Key());
                });
                params.LocalScope.Global.NativeFunctions.ForEach([&fn](const auto& bucket) {
                    fn.NativeFunctions.EmplaceBack(bucket.Key());
                });
                fn.Scopes.EmplaceBack(Scope{
                    .StartPos = params.Position,
                    .EndPos = params.Position,
                    .Locals = params.LocalScope.Locals,
                });
            }
        } break;
        case Pulsar::LSPBlockNotificationType::BlockEnd: {
            Pulsar::String fnId = params.FilePath + ":" + params.FnName;
            auto fnPair = functions.Find(fnId);
            if (fnPair) {
                auto& fn = fnPair->Value();
                if (fn.Scopes.Size() > 0) {
                    fn.Scopes.Back().EndPos = params.Position;
                }
            }
        } break;
        case Pulsar::LSPBlockNotificationType::LocalScopeChanged: {
            Pulsar::String fnId = params.FilePath + ":" + params.FnName;
            auto fnPair = functions.Find(fnId);
            if (fnPair) {
                auto& fn = fnPair->Value();
                if (fn.Scopes.Size() > 0) {
                    fn.Scopes.Back().EndPos = params.Position;
                }
                fn.Scopes.EmplaceBack(Scope{
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
    settings.LSPHooks.OnIdentifierUsage = [&path, extractAll, &functions](Pulsar::LSPHooks::OnIdentifierUsageParams&& params) {
        if (!extractAll && params.FilePath != path) return false;

        Pulsar::String fnId = params.FilePath + ":" + params.FnName;
        auto fnPair = functions.Find(fnId);
        if (params.FnName.Length() <= 0 && !fnPair) {
            // Create global scope
            fnPair = &functions.Emplace(fnId);
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

    if (parser.AddSourceFile(path) != Pulsar::ParseResult::OK) return {};
    parser.ParseIntoModule(parsedDocument.Module, settings);

    Pulsar::List<Function> result;
    functions.ForEach([&parsedDocument](auto& bucket) {
        parsedDocument.Functions.EmplaceBack(std::move(bucket.Value()));
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
    for (size_t i = 0; i < doc->Functions.Size(); ++i) {
        const Function& fn = doc->Functions[i];
        if (fn.FilePath != path) continue;
        for (size_t j = 0; j < fn.UsedIdentifiers.Size(); ++j) {
            const IdentifierUsage& identUsage = fn.UsedIdentifiers[j];
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
                    lsp::FileURI locUri = NormalizedPathToURI(fn.FilePath);
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

std::vector<lsp::CompletionItem> PulsarLSP::Server::GetCompletionItems(const lsp::FileURI& uri, lsp::Position pos)
{
    auto doc = GetOrParseDocument(uri);
    if (!doc) return {};

    for (size_t i = 0; i < doc->Functions.Size(); ++i) {
        const Scope* cursorScope = nullptr;
        const Function& fn = doc->Functions[i];
        for (size_t j = fn.Scopes.Size(); j > 0; --j) {
            const Scope& scope = fn.Scopes[j-1];
            if (IsPositionInBetween(pos, scope.StartPos, scope.EndPos)) {
                cursorScope = &scope;
                break;
            }
        }

        if (cursorScope) {
            std::vector<lsp::CompletionItem> result;
            for (size_t j = 0; j < fn.Functions.Size(); ++j) {
                lsp::CompletionItem item{};
                item.label  = "(";
                item.label += fn.Functions[j].Data();
                item.label += ")";
                item.kind   = lsp::CompletionItemKind::Function;
                result.emplace_back(std::move(item));
            }
            for (size_t j = 0; j < fn.NativeFunctions.Size(); ++j) {
                lsp::CompletionItem item{};
                item.label  = "(*";
                item.label += fn.NativeFunctions[j].Data();
                item.label += ")";
                item.kind   = lsp::CompletionItemKind::Function;
                result.emplace_back(std::move(item));
            }
            for (size_t j = 0; j < fn.Globals.Size(); ++j) {
                lsp::CompletionItem item{};
                item.label = fn.Globals[j].Data();
                item.kind  = lsp::CompletionItemKind::Variable;
                result.emplace_back(std::move(item));
            }
            for (size_t j = 0; j < cursorScope->Locals.Size(); ++j) {
                lsp::CompletionItem item{};
                item.label = cursorScope->Locals[j].Name.Data();
                item.kind  = lsp::CompletionItemKind::Variable;
                result.emplace_back(std::move(item));
            }
            return result;
        }
    }

    return {};
}

Pulsar::SharedRef<PulsarLSP::ParsedDocument> PulsarLSP::Server::ParseAndStoreDocument(const lsp::FileURI& uri)
{
    auto doc = ParsedDocument::From(uri, false);
    if (!doc) {
        // Drop document that failed parsing.
        // This means that the document does not exist.
        DropDocument(uri);
        return nullptr;
    }

    // Delete saved sources to save some memory
    for (size_t i = 0; i < doc->Module.SourceDebugSymbols.Size(); ++i) {
        Pulsar::SourceDebugSymbol& srcSymbol = doc->Module.SourceDebugSymbols[i];
        srcSymbol.Source.Resize(0);
        srcSymbol.Source.Reserve(0);
    }

    Pulsar::String path = URIToNormalizedPath(uri);
    return m_DocumentCache.Emplace(path, Pulsar::SharedRef<ParsedDocument>::New(std::move(doc.value()))).Value();
}

void PulsarLSP::Server::Run(lsp::Connection& connection)
{
    lsp::MessageHandler messageHandler{connection};

    bool running = true;
    messageHandler.requestHandler()
        .add<lsp::requests::Initialize>([](const lsp::jsonrpc::MessageId& id, lsp::requests::Initialize::Params&& params)
        {
            (void)id; (void)params;
            lsp::requests::Initialize::Result result;

            lsp::TextDocumentSyncOptions documentSyncOptions{};
            documentSyncOptions.save = true;

            result.capabilities.textDocumentSync = documentSyncOptions;
            result.capabilities.completionProvider = lsp::CompletionOptions{};
            result.capabilities.declarationProvider = true;
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
        .add<lsp::notifications::TextDocument_DidSave>([this](lsp::notifications::TextDocument_DidSave::Params&& params)
        {
            (void)this->GetOrParseDocument(params.textDocument.uri, true);
        })
        .add<lsp::notifications::Exit>([&running]()
        {
            running = false;
        });

    while (running) messageHandler.processIncomingMessages();
}
