#include "pulsar-lsp/server.h"

#include <filesystem>
#include <fstream>

#include <lsp/messages.h>

#include "pulsar-lsp/completion.h"
#include "pulsar-lsp/helpers.h"

PulsarLSP::FunctionScopeBuilder& PulsarLSP::FunctionScopeBuilder::SetFilePath(const Pulsar::String& path)
{
    m_FunctionScope.FilePath = path;
    return *this;
}

PulsarLSP::FunctionScopeBuilder& PulsarLSP::FunctionScopeBuilder::SetName(const Pulsar::String& name)
{
    m_FunctionScope.Name = name;
    return *this;
}

PulsarLSP::FunctionScopeBuilder& PulsarLSP::FunctionScopeBuilder::AddGlobal(BoundGlobal&& global)
{
    m_FunctionScope.Globals.EmplaceBack(std::forward<BoundGlobal>(global));
    return *this;
}

PulsarLSP::FunctionScopeBuilder& PulsarLSP::FunctionScopeBuilder::AddFunction(BoundFunction&& func)
{
    m_FunctionScope.Functions.EmplaceBack(std::forward<BoundFunction>(func));
    return *this;
}

PulsarLSP::FunctionScopeBuilder& PulsarLSP::FunctionScopeBuilder::AddNativeFunction(BoundNativeFunction&& nativeFunc)
{
    m_FunctionScope.NativeFunctions.EmplaceBack(std::forward<BoundNativeFunction>(nativeFunc));
    return *this;
}

PulsarLSP::FunctionScopeBuilder& PulsarLSP::FunctionScopeBuilder::AddIdentifierUsage(IdentifierUsage&& identUsage)
{
    m_FunctionScope.UsedIdentifiers.EmplaceBack(std::forward<IdentifierUsage>(identUsage));
    return *this;
}

PulsarLSP::FunctionScopeBuilder& PulsarLSP::FunctionScopeBuilder::StartBlock(Pulsar::SourcePosition pos, const Pulsar::List<LocalScope::Local>& locals)
{
    m_OpenLocalScopes.EmplaceBack(LocalScope{
        .IsOpen   = true,
        .StartPos = pos,
        .EndPos   = pos,
        .Locals   = locals,
    });
    return *this;
}

PulsarLSP::FunctionScopeBuilder& PulsarLSP::FunctionScopeBuilder::EndBlock(Pulsar::SourcePosition pos)
{
    if (HasOpenBlock()) {
        LocalScope& localScope = m_OpenLocalScopes.Back();
        localScope.IsOpen = false;
        localScope.EndPos = pos;
        m_FunctionScope.LocalScopes.EmplaceBack(std::move(localScope));
        m_OpenLocalScopes.PopBack();
    }
    return *this;
}

PulsarLSP::FunctionScope&& PulsarLSP::FunctionScopeBuilder::Build()
{
    for (size_t i = m_OpenLocalScopes.Size(); i > 0; --i) {
        m_FunctionScope.LocalScopes.EmplaceBack(std::move(m_OpenLocalScopes[i-1]));
    }
    m_OpenLocalScopes.Clear();
    return std::move(m_FunctionScope);
}

std::optional<PulsarLSP::ParsedDocument> PulsarLSP::Server::CreateParsedDocument(const lsp::FileURI& uri, const Pulsar::String& document, bool extractAll)
{
    ParsedDocument parsedDocument;

    Pulsar::String path = URIToNormalizedPath(uri);
    Pulsar::Parser parser;

    using FunctionScopesMap = Pulsar::HashMap<Pulsar::String, FunctionScopeBuilder>;
    FunctionScopesMap functionScopes;
    Pulsar::List<FunctionDefinition> functionDefinitions;

    Pulsar::ParseSettings settings = Pulsar::ParseSettings_Default;
    settings.StoreDebugSymbols        = true;
    settings.MapGlobalProducersToVoid = m_Options.MapGlobalProducersToVoid;
    settings.IncludeResolver = [this](Pulsar::Parser& parser, const Pulsar::String& cwf, const Pulsar::Token& token) {
        std::filesystem::path targetPath(token.StringVal.Data());
        std::filesystem::path workingPath(cwf.Data());
        std::filesystem::path filePath = workingPath.parent_path() / targetPath;
        lsp::FileURI filePathURI = filePath.generic_string().c_str();

        Pulsar::String internalPath = URIToNormalizedPath(filePathURI);
        auto text = this->m_Library.FindOrLoadDocument(filePathURI);
        if (!text) return parser.SetError(Pulsar::ParseResult::FileNotRead, token, "Could not read file '" + internalPath + "'.");

        parser.AddSource(internalPath, *text);
        return Pulsar::ParseResult::OK;
    };
    settings.LSPHooks.OnBlockNotification = [&path, extractAll, &functionScopes](Pulsar::LSPHooks::OnBlockNotificationParams&& params) {
        if (!extractAll && params.FilePath != path) return false;

        Pulsar::String selfIdx = ":";
        if (auto idx = params.LocalScope.Global.Functions.Find(params.FnDefinition.Name); idx) {
            selfIdx += Pulsar::UIntToString((uint64_t)idx->Value());
        }
        Pulsar::String fnId = params.FilePath + selfIdx + ":" + params.FnDefinition.Name;

        switch (params.Type) {
        case Pulsar::LSPBlockNotificationType::BlockStart: {
            auto fnPair = functionScopes.Find(fnId);
            if (fnPair) {
                fnPair->Value()
                    .StartBlock(params.Position, params.LocalScope.Locals);
            } else {
                auto& fnBuilder = functionScopes.Emplace(fnId)
                    .Value()
                    .SetFilePath(params.FilePath)
                    .SetName(params.FnDefinition.Name)
                    .StartBlock(params.Position, params.LocalScope.Locals);
                params.LocalScope.Global.Globals.ForEach([&fnBuilder](const auto& bucket) {
                    fnBuilder.AddGlobal({ .Name = bucket.Key(), .Index = bucket.Value() });
                });
                params.LocalScope.Global.Functions.ForEach([&fnBuilder](const auto& bucket) {
                    fnBuilder.AddFunction({ .Name = bucket.Key(), .Index = bucket.Value() });
                });
                params.LocalScope.Global.NativeFunctions.ForEach([&fnBuilder](const auto& bucket) {
                    fnBuilder.AddNativeFunction({ .Name = bucket.Key(), .Index = bucket.Value() });
                });
            }
        } break;
        case Pulsar::LSPBlockNotificationType::BlockEnd: {
            auto fnPair = functionScopes.Find(fnId);
            if (fnPair) {
                fnPair->Value()
                    .EndBlock(params.Position);
            }
        } break;
        case Pulsar::LSPBlockNotificationType::LocalScopeChanged: {
            auto fnPair = functionScopes.Find(fnId);
            if (fnPair) {
                fnPair->Value()
                    .EndBlock(params.Position)
                    .StartBlock(params.Position, params.LocalScope.Locals);
            }
        } break;
        default: break;
        }

        return false;
    };
    settings.LSPHooks.OnIdentifierUsage = [&path, extractAll, &functionScopes](Pulsar::LSPHooks::OnIdentifierUsageParams&& params) {
        if (!extractAll && params.FilePath != path) return false;

        Pulsar::String selfIdx = ":";
        if (auto idx = params.LocalScope.Global.Functions.Find(params.FnDefinition.Name); idx) {
            selfIdx += Pulsar::UIntToString((uint64_t)idx->Value());
        }
        Pulsar::String fnId = params.FilePath + selfIdx + ":" + params.FnDefinition.Name;

        auto fnPair = functionScopes.Find(fnId);
        if (params.FnDefinition.Name.Length() <= 0 && !fnPair) {
            // Create global scope
            fnPair = &functionScopes.Emplace(fnId);
            fnPair->Value()
                .SetFilePath(params.FilePath)
                .SetName("");
        }

        Pulsar::SourcePosition localDeclaredAt = NULL_SOURCE_POSITION;
        if (params.Type == Pulsar::LSPIdentifierUsageType::Local && params.BoundIdx < params.LocalScope.Locals.Size())
            localDeclaredAt = params.LocalScope.Locals[params.BoundIdx].DeclaredAt;

        fnPair->Value()
            .AddIdentifierUsage(IdentifierUsage{
                .Identifier      = params.Token,
                .Type            = params.Type,
                .BoundIndex      = params.BoundIdx,
                .LocalDeclaredAt = localDeclaredAt,
            });

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

    if (!parser.AddSource(path, std::move(document))) return {};
    parsedDocument.ParseResult = parser.ParseIntoModule(parsedDocument.Module, settings);
    if (parsedDocument.ParseResult != Pulsar::ParseResult::OK) {
        parsedDocument.ErrorFilePath = *parser.GetErrorPath();
        parsedDocument.ErrorPosition = parser.GetErrorToken().SourcePos;
        parsedDocument.ErrorMessage  = parser.GetErrorMessage();
    }

    functionScopes.ForEach([&parsedDocument](FunctionScopesMap::BucketType& bucket) {
        parsedDocument.FunctionScopes.EmplaceBack(bucket.Value().Build());
    });

    return parsedDocument;
}

PulsarLSP::ParsedDocument::SharedRef PulsarLSP::Server::GetParsedDocument(const lsp::FileURI& uri) const
{
    Pulsar::String path = URIToNormalizedPath(uri);
    auto doc = m_ParsedCache.Find(path);
    if (!doc) return nullptr;
    return doc->Value();
}

PulsarLSP::ParsedDocument::SharedRef PulsarLSP::Server::GetOrParseDocument(const lsp::FileURI& uri, bool forceLoad)
{
    if (!forceLoad) {
        auto doc = GetParsedDocument(uri);
        if (doc) return doc;
    }

    return ParseDocument(uri, forceLoad);
}

void PulsarLSP::Server::DropParsedDocument(const lsp::FileURI& uri)
{
    Pulsar::String path = URIToNormalizedPath(uri);
    m_ParsedCache.Remove(path);
}

void PulsarLSP::Server::DropAllParsedDocuments()
{
    m_ParsedCache.Clear();
}

bool PulsarLSP::Server::OpenDocument(const lsp::FileURI& uri, const std::string& docText, int version)
{
    return (bool)m_Library.StoreDocument(uri, docText.c_str(), version);
}

void PulsarLSP::Server::PatchDocument(const lsp::FileURI& uri, const DocumentPatches& patches, int version)
{
    m_Library.PatchDocument(uri, patches, version);
    DropParsedDocument(uri);
}

void PulsarLSP::Server::CloseDocument(const lsp::FileURI& uri)
{
    m_Library.DeleteDocument(uri);
}

void PulsarLSP::Server::DeleteDocument(const lsp::FileURI& uri)
{
    CloseDocument(uri);
    DropParsedDocument(uri);
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

lsp::CompletionItem PulsarLSP::CreateCompletionItemForBoundEntity(ParsedDocument::SharedRef doc, const BoundGlobal& global, Pulsar::SourcePosition replaceWithName)
{
    lsp::CompletionItem item{};
    item.label = global.Name.Data();
    item.kind  = lsp::CompletionItemKind::Variable;
    if (replaceWithName == NULL_SOURCE_POSITION) {
        item.insertText = item.label;
    } else {
        lsp::TextEdit edit{
            .range   = SourcePositionToRange(replaceWithName),
            .newText = item.label,
        };
        item.filterText = edit.newText;
        item.textEdit = { std::move(edit) };
    }

    std::string detail;
    if (global.Index < doc->Module.Globals.Size()) {
        const Pulsar::GlobalDefinition& glblDef = doc->Module.Globals[global.Index];
        if (glblDef.IsConstant) {
            detail += "const ";
        }
        detail += ValueTypeToString(glblDef.InitialValue, doc->Module).Data();
        detail += " -> ";
        detail += glblDef.Name.Data();
    }
    item.detail = std::move(detail);

    return item;
}

void PulsarLSP::InsertFunctionDefinitionDetails(lsp::CompletionItem& item, const FunctionDefinition& lspDef)
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

lsp::CompletionItem PulsarLSP::CreateCompletionItemForBoundEntity(ParsedDocument::SharedRef doc, const BoundFunction& fn, Pulsar::SourcePosition replaceWithName)
{
    lsp::CompletionItem item{};
    item.label  = "(";
    item.label += fn.Name.Data();
    item.label += ")";
    item.kind   = lsp::CompletionItemKind::Function;
    if (replaceWithName == NULL_SOURCE_POSITION) {
        item.insertText = item.label;
    } else {
        lsp::TextEdit edit{
            .range   = SourcePositionToRange(replaceWithName),
            .newText = fn.Name.Data(),
        };
        item.filterText = edit.newText;
        item.textEdit = { std::move(edit) };
    }

    for (size_t i = 0; i < doc->FunctionDefinitions.Size(); ++i) {
        const FunctionDefinition& lspDef = doc->FunctionDefinitions[i];
        if (!lspDef.IsNative && lspDef.Index == fn.Index) {
            InsertFunctionDefinitionDetails(item, lspDef);
            break;
        }
    }

    return item;
}

lsp::CompletionItem PulsarLSP::CreateCompletionItemForBoundEntity(ParsedDocument::SharedRef doc, const BoundNativeFunction& nativeFn, Pulsar::SourcePosition replaceWithName)
{
    lsp::CompletionItem item{};
    item.label  = "(*";
    item.label += nativeFn.Name.Data();
    item.label += ")";
    item.kind   = lsp::CompletionItemKind::Function;
    if (replaceWithName == NULL_SOURCE_POSITION) {
        item.insertText = item.label;
    } else {
        lsp::TextEdit edit{
            .range   = SourcePositionToRange(replaceWithName),
            .newText = nativeFn.Name.Data(),
        };
        item.filterText = edit.newText;
        item.textEdit = { std::move(edit) };
    }

    for (size_t i = 0; i < doc->FunctionDefinitions.Size(); ++i) {
        const FunctionDefinition& lspDef = doc->FunctionDefinitions[i];
        if (lspDef.IsNative && lspDef.Index == nativeFn.Index) {
            InsertFunctionDefinitionDetails(item, lspDef);
            break;
        }
    }

    return item;
}

lsp::CompletionItem PulsarLSP::CreateCompletionItemForLocal(const LocalScope::Local& local, Pulsar::SourcePosition replaceWithName)
{
    lsp::CompletionItem item{};
    item.label = local.Name.Data();
    item.kind  = lsp::CompletionItemKind::Variable;
    if (replaceWithName == NULL_SOURCE_POSITION) {
        item.insertText = item.label;
    } else {
        lsp::TextEdit edit{
            .range   = SourcePositionToRange(replaceWithName),
            .newText = item.label,
        };
        item.filterText = edit.newText;
        item.textEdit = { std::move(edit) };
    }
    return item;
}

std::vector<lsp::CompletionItem> PulsarLSP::Server::GetCompletionItems(const lsp::FileURI& uri, lsp::Position pos)
{
    auto doc = GetOrParseDocument(uri);
    if (!doc) return {};

    for (size_t i = 0; i < doc->FunctionScopes.Size(); ++i) {
        const LocalScope* cursorLocalScope = nullptr;
        const FunctionScope& fnScope = doc->FunctionScopes[i];
        for (size_t j = 0; j < fnScope.LocalScopes.Size(); ++j) {
            const LocalScope& localScope = fnScope.LocalScopes[j];
            // If there was an error while parsing, a bunch of scopes may have IsOpen set to true.
            // That tells the LSP that from localScope.StartPos onwards the scope's variables are available.
            if ((localScope.IsOpen && IsPositionAfter(pos, localScope.StartPos)) ||
                IsPositionInBetween(pos, localScope.StartPos, localScope.EndPos)
            ) {
                cursorLocalScope = &localScope;
                break;
            }
        }

        if (cursorLocalScope) {
            Pulsar::String docPath = URIToNormalizedPath(uri);
            if (doc->ParseResult != Pulsar::ParseResult::OK  &&
                IsPositionInBetween(pos, doc->ErrorPosition) &&
                doc->ErrorFilePath == docPath
            ) {
                return GetErrorCompletionItems(doc, fnScope, *cursorLocalScope);
            } else {
                return GetScopeCompletionItems(doc, fnScope, *cursorLocalScope);
            }
        }
    }

    return {};
}

std::vector<lsp::CompletionItem> PulsarLSP::Server::GetErrorCompletionItems(ParsedDocument::SharedRef doc, const FunctionScope& funcScope, const LocalScope& localScope)
{
    std::vector<lsp::CompletionItem> result;

    switch (doc->ParseResult) {
    case Pulsar::ParseResult::UsageOfUndeclaredLocal: {
        for (size_t i = 0; i < funcScope.Globals.Size(); ++i) {
            result.emplace_back(CreateCompletionItemForBoundEntity(doc, funcScope.Globals[i], doc->ErrorPosition));
        }
        for (size_t i = 0; i < localScope.Locals.Size(); ++i) {
            result.emplace_back(CreateCompletionItemForLocal(localScope.Locals[i], doc->ErrorPosition));
        }
    } break;
    case Pulsar::ParseResult::UsageOfUnknownInstruction: {
        PulsarLSP::Completion::GetInstructions().ForEach([&result, pos = doc->ErrorPosition](const auto& bucket) {
            lsp::CompletionItem item = bucket.Value();
            lsp::TextEdit edit{
                .range   = SourcePositionToRange(pos),
                .newText = bucket.Key(),
            };
            item.filterText = edit.newText;
            item.textEdit = { std::move(edit) };
            result.emplace_back(std::move(item));
        });
    } break;
    case Pulsar::ParseResult::UsageOfUndeclaredFunction: {
        for (size_t i = 0; i < funcScope.Functions.Size(); ++i) {
            result.emplace_back(CreateCompletionItemForBoundEntity(doc, funcScope.Functions[i], doc->ErrorPosition));
        }
    } break;
    case Pulsar::ParseResult::UsageOfUndeclaredNativeFunction: {
        for (size_t i = 0; i < funcScope.NativeFunctions.Size(); ++i) {
            result.emplace_back(CreateCompletionItemForBoundEntity(doc, funcScope.NativeFunctions[i], doc->ErrorPosition));
        }
    } break;
    case Pulsar::ParseResult::UnexpectedToken:
        // TODO: Add keywords.
        break;
    default: break;
    }

    return result;
}

std::vector<lsp::CompletionItem> PulsarLSP::Server::GetScopeCompletionItems(ParsedDocument::SharedRef doc, const FunctionScope& funcScope, const LocalScope& localScope)
{
    std::vector<lsp::CompletionItem> result;

    for (size_t j = 0; j < funcScope.Functions.Size(); ++j) {
        result.emplace_back(CreateCompletionItemForBoundEntity(doc, funcScope.Functions[j]));
    }
    for (size_t j = 0; j < funcScope.NativeFunctions.Size(); ++j) {
        result.emplace_back(CreateCompletionItemForBoundEntity(doc, funcScope.NativeFunctions[j]));
    }
    PulsarLSP::Completion::GetInstructions().ForEach([&result](const auto& bucket) {
        lsp::CompletionItem item = bucket.Value();
        item.insertText = item.label;
        result.emplace_back(std::move(item));
    });
    for (size_t j = 0; j < funcScope.Globals.Size(); ++j) {
        result.emplace_back(CreateCompletionItemForBoundEntity(doc, funcScope.Globals[j]));
    }
    for (size_t j = 0; j < localScope.Locals.Size(); ++j) {
        result.emplace_back(CreateCompletionItemForLocal(localScope.Locals[j]));
    }

    return result;
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
    error.message  = Pulsar::ParseResultToString(badDoc->ParseResult);
    error.message += ": ";
    error.message += badDoc->ErrorMessage.Data();
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
        srcSymbol.Source.Reserve(0);
    }
}

PulsarLSP::ParsedDocument::SharedRef PulsarLSP::Server::ParseDocument(const lsp::FileURI& uri, bool forceLoad)
{
    auto text = forceLoad
        ? m_Library.LoadDocument(uri)
        : m_Library.FindOrLoadDocument(uri);

    if (!text) {
        DropParsedDocument(uri);
        return nullptr;
    }

    auto doc = CreateParsedDocument(uri, *text, false);
    if (!doc) {
        DropParsedDocument(uri);
        return nullptr;
    }

    StripModule(doc->Module);

    Pulsar::String path = URIToNormalizedPath(uri);
    return m_ParsedCache.Emplace(
        path,
        ParsedDocument::SharedRef::New(std::move(*doc))
    ).Value();
}

#define GET_INIT_BOOLEAN_OPTION(jsonField, structField) \
    if (auto jsonField = userOptions.find(#jsonField);  \
        jsonField != userOptions.end() &&               \
        jsonField->second.isBoolean()                   \
    ) this->m_Options.structField = jsonField->second.boolean()

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

                GET_INIT_BOOLEAN_OPTION(diagnosticsOnOpen,        DiagnosticsOnOpen);
                GET_INIT_BOOLEAN_OPTION(diagnosticsOnSave,        DiagnosticsOnSave);
                GET_INIT_BOOLEAN_OPTION(diagnosticsOnChange,      DiagnosticsOnChange);
                GET_INIT_BOOLEAN_OPTION(fullSyncOnSave,           FullSyncOnSave);
                GET_INIT_BOOLEAN_OPTION(mapGlobalProducersToVoid, MapGlobalProducersToVoid);
            }

            lsp::requests::Initialize::Result result;

            lsp::TextDocumentSyncOptions documentSyncOptions{};
            documentSyncOptions.save = lsp::SaveOptions{ .includeText = this->m_Options.FullSyncOnSave };
            documentSyncOptions.change = lsp::TextDocumentSyncKind::Incremental;
            documentSyncOptions.openClose = true;

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
        .add<lsp::notifications::TextDocument_DidOpen>([this, &messageHandler](lsp::notifications::TextDocument_DidOpen::Params&& params)
        {
            if (this->OpenDocument(params.textDocument.uri, params.textDocument.text, params.textDocument.version) && this->m_Options.DiagnosticsOnOpen) {
                this->SendUpdatedDiagnosticReport(messageHandler, params.textDocument.uri);
            }
        })
        .add<lsp::notifications::TextDocument_DidClose>([this](lsp::notifications::TextDocument_DidClose::Params&& params)
        {
            this->CloseDocument(params.textDocument.uri);
        })
        .add<lsp::notifications::TextDocument_DidSave>([this, &messageHandler](lsp::notifications::TextDocument_DidSave::Params&& params)
        {
            if (this->m_Options.FullSyncOnSave && params.text.has_value()) {
                if (this->OpenDocument(params.textDocument.uri, *params.text) && this->m_Options.DiagnosticsOnSave) {
                    this->SendUpdatedDiagnosticReport(messageHandler, params.textDocument.uri);
                }
            } else if (this->m_Options.DiagnosticsOnSave) {
                this->SendUpdatedDiagnosticReport(messageHandler, params.textDocument.uri);
            }
        })
        .add<lsp::notifications::TextDocument_DidChange>([this, &messageHandler](lsp::notifications::TextDocument_DidChange::Params&& params)
        {
            this->PatchDocument(params.textDocument.uri, params.contentChanges, params.textDocument.version);
            if (this->m_Options.DiagnosticsOnChange) {
                this->SendUpdatedDiagnosticReport(messageHandler, params.textDocument.uri);
            }
        })
        .add<lsp::requests::Shutdown>([](const lsp::jsonrpc::MessageId& id)
            -> lsp::requests::Shutdown::Result
        {
            (void)id;
            return nullptr;
        })
        .add<lsp::notifications::Exit>([&running]()
        {
            running = false;
        });

    while (running) messageHandler.processIncomingMessages();
}
