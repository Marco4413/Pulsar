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

std::optional<PulsarLSP::ParsedDocument> PulsarLSP::Server::CreateParsedDocument(const lsp::FileUri& uri, const Pulsar::String& document, bool extractAll)
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
    settings.IncludeResolver = [this, &parsedDocument, &path](Pulsar::Parser& parser, const Pulsar::String& cwf, const Pulsar::Token& token) {
        std::filesystem::path targetPath(token.StringVal.CString());
        Pulsar::List<Pulsar::String> triedPaths;

        Pulsar::String internalPath;
        ConstSharedText text = nullptr;
        { // Try relative path first
            std::filesystem::path workingPath(cwf.CString());
            std::filesystem::path filePath = workingPath.parent_path() / targetPath;
            lsp::FileUri filePathURI = lsp::FileUri::fromPath(filePath.string());

            internalPath = URIToNormalizedPath(filePathURI);
            text = this->m_Library.FindOrLoadDocument(filePathURI);
            triedPaths.EmplaceBack(internalPath);
        }

        if (!text) {
            // Try paths from include paths
            for (size_t i = m_Options.IncludePaths.size(); i > 0; --i) {
                std::filesystem::path workingPath(m_Options.IncludePaths[i-1]);
                std::filesystem::path filePath = workingPath / targetPath;
                lsp::FileUri filePathURI = lsp::FileUri::fromPath(filePath.string());

                internalPath = URIToNormalizedPath(filePathURI);
                text = this->m_Library.FindOrLoadDocument(filePathURI);
                if (text) break;

                triedPaths.EmplaceBack(internalPath);
            }

            if (!text) {
                // Relative and include paths failed
                Pulsar::String errorMsg = "Could not read file ";

                // I'll be damned if triedPaths hasn't got at least one path.
                errorMsg += '\'';
                errorMsg += triedPaths[0];
                errorMsg += '\'';

                for (size_t i = 1; i < triedPaths.Size(); ++i) {
                    errorMsg += ", '";
                    errorMsg += triedPaths[i];
                    errorMsg += '\'';
                }

                errorMsg += '.';
                return parser.SetError(Pulsar::ParseResult::FileNotRead, token, errorMsg);
            }
        }

        if (parser.AddSource(internalPath, *text) && path == cwf) {
            parsedDocument.IncludedFiles.EmplaceBack(internalPath, token.SourcePos);
        }

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
        if (!params.IsNative && params.IsRedeclaration) return false;

        if (params.IsRedeclaration) {
            // TODO: Split natives from non-natives. I'm getting tired of iterating over the array.
            for (size_t i = 0; i < parsedDocument.FunctionDefinitions.Size(); ++i) {
                FunctionDefinition& fnDef = parsedDocument.FunctionDefinitions[i];
                if (fnDef.IsNative == params.IsNative && fnDef.Index == params.Index) {
                    // May be redeclared in a different file
                    fnDef.FilePath   = params.FilePath;
                    // Update debug symbols
                    fnDef.Definition = params.FnDefinition;
                    fnDef.Args       = params.Args;
                }
            }
        } else {
            parsedDocument.FunctionDefinitions.PushBack(FunctionDefinition{
                .FilePath   = params.FilePath,
                .IsNative   = params.IsNative,
                .Index      = params.Index,
                .Definition = params.FnDefinition,
                .Args       = params.Args,
            });
        }
        

        return false;
    };

    if (!parser.AddSource(path, std::move(document))) return {};
    parsedDocument.ParseResult = parser.ParseIntoModule(parsedDocument.Module, settings);
    if (parsedDocument.ParseResult != Pulsar::ParseResult::OK) {
        parsedDocument.ErrorFilePath = *parser.GetErrorPath();
        parsedDocument.ErrorPosition = parser.GetErrorToken().SourcePos;
        parsedDocument.ErrorMessage  = parser.GetErrorMessage();
    }

    functionScopes.ForEach([&parsedDocument](FunctionScopesMap::Bucket& bucket) {
        parsedDocument.FunctionScopes.EmplaceBack(bucket.Value().Build());
    });

    return parsedDocument;
}

PulsarLSP::ParsedDocument::SharedRef PulsarLSP::Server::GetParsedDocument(const lsp::FileUri& uri) const
{
    Pulsar::String path = URIToNormalizedPath(uri);
    auto doc = m_ParsedCache.Find(path);
    if (!doc) return nullptr;
    return doc->Value();
}

PulsarLSP::ParsedDocument::SharedRef PulsarLSP::Server::GetOrParseDocument(const lsp::FileUri& uri, bool forceLoad)
{
    if (!forceLoad) {
        auto doc = GetParsedDocument(uri);
        if (doc) return doc;
    }

    return ParseDocument(uri, forceLoad);
}

void PulsarLSP::Server::DropParsedDocument(const lsp::FileUri& uri)
{
    Pulsar::String path = URIToNormalizedPath(uri);
    ResetDiagnosticReport(path);
    m_ParsedCache.Remove(path);
}

void PulsarLSP::Server::DropAllParsedDocuments()
{
    m_ParsedCache.ForEach([this](const auto& bucket) {
        this->ResetDiagnosticReport(bucket.Key());
    });
    m_ParsedCache.Clear();
}

bool PulsarLSP::Server::OpenDocument(const lsp::FileUri& uri, const std::string& docText, int version)
{
    return (bool)m_Library.StoreDocument(uri, docText.c_str(), version);
}

void PulsarLSP::Server::PatchDocument(const lsp::FileUri& uri, const DocumentPatches& patches, int version)
{
    m_Library.PatchDocument(uri, patches, version);
    DropParsedDocument(uri);
}

void PulsarLSP::Server::CloseDocument(const lsp::FileUri& uri)
{
    DeleteDocument(uri);
}

void PulsarLSP::Server::DeleteDocument(const lsp::FileUri& uri)
{
    m_Library.DeleteDocument(uri);
    DropParsedDocument(uri);
}

std::optional<lsp::Location> PulsarLSP::Server::FindDeclaration(const lsp::FileUri& uri, lsp::Position pos)
{
    auto doc = GetOrParseDocument(uri);
    if (!doc || !doc->Module.HasSourceDebugSymbols()) return {};

    ConstSharedText docText = m_Library.FindDocument(uri);
    pos = EditorPositionToDocumentPosition(docText, pos);

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
                    
                    const Pulsar::GlobalDefinition& global = doc->Module.Globals[identUsage.BoundIndex];
                    if (!global.HasDebugSymbol()) break;
                    
                    if (global.DebugSymbol.SourceIdx >= doc->Module.SourceDebugSymbols.Size()) break;
                    const Pulsar::SourceDebugSymbol& sourceSymbol = doc->Module.SourceDebugSymbols[global.DebugSymbol.SourceIdx];
                    
                    lsp::FileUri locUri = NormalizedPathToURI(sourceSymbol.Path);
                    return lsp::Location{
                        .uri = locUri,
                        .range = DocumentRangeToEditorRange(docText, SourcePositionToRange(global.DebugSymbol.Token.SourcePos))
                    };
                }
                case Pulsar::LSPIdentifierUsageType::Function: {
                    if (identUsage.BoundIndex >= doc->Module.Functions.Size()) break;
                    
                    const Pulsar::FunctionDefinition& func = doc->Module.Functions[identUsage.BoundIndex];
                    if (!func.HasDebugSymbol()) break;
                    
                    if (func.DebugSymbol.SourceIdx >= doc->Module.SourceDebugSymbols.Size()) break;
                    const Pulsar::SourceDebugSymbol& sourceSymbol = doc->Module.SourceDebugSymbols[func.DebugSymbol.SourceIdx];
                    
                    lsp::FileUri locUri = NormalizedPathToURI(sourceSymbol.Path);
                    return lsp::Location{
                        .uri = locUri,
                        .range = DocumentRangeToEditorRange(docText, SourcePositionToRange(func.DebugSymbol.Token.SourcePos))
                    };
                }
                case Pulsar::LSPIdentifierUsageType::NativeFunction: {
                    if (identUsage.BoundIndex >= doc->Module.NativeBindings.Size()) break;
                    
                    const Pulsar::FunctionDefinition& func = doc->Module.NativeBindings[identUsage.BoundIndex];
                    if (!func.HasDebugSymbol()) break;
                    
                    if (func.DebugSymbol.SourceIdx >= doc->Module.SourceDebugSymbols.Size()) break;
                    const Pulsar::SourceDebugSymbol& sourceSymbol = doc->Module.SourceDebugSymbols[func.DebugSymbol.SourceIdx];
                    
                    lsp::FileUri locUri = NormalizedPathToURI(sourceSymbol.Path);
                    return lsp::Location{
                        .uri = locUri,
                        .range = DocumentRangeToEditorRange(docText, SourcePositionToRange(func.DebugSymbol.Token.SourcePos))
                    };
                }
                case Pulsar::LSPIdentifierUsageType::Local: {
                    lsp::FileUri locUri = NormalizedPathToURI(fnScope.FilePath);
                    return lsp::Location{
                        .uri = locUri,
                        .range = DocumentRangeToEditorRange(docText, SourcePositionToRange(identUsage.LocalDeclaredAt))
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

std::optional<lsp::Location> PulsarLSP::Server::FindDefinition(const lsp::FileUri& uri, lsp::Position editorPos)
{
    auto doc = GetOrParseDocument(uri);
    if (!doc) return {};
    
    lsp::Position pos = EditorPositionToDocumentPosition(uri, editorPos);

    for (size_t i = 0; i < doc->IncludedFiles.Size(); ++i) {
        const auto& includedFile = doc->IncludedFiles[i];
        if (IsPositionInBetween(pos, includedFile.IncludedAt)) {
            return lsp::Location{
                .uri = NormalizedPathToURI(includedFile.FilePath),
                .range = NULL_RANGE
            };
        }
    }

    return FindDeclaration(uri, editorPos);
}

std::vector<lsp::DocumentSymbol> PulsarLSP::Server::GetSymbols(const lsp::FileUri& uri)
{
    auto doc = GetOrParseDocument(uri);
    if (!doc) return {};

    ConstSharedText docText = m_Library.FindDocument(uri);

    std::vector<lsp::DocumentSymbol> result;
    for (size_t i = 0; i < doc->Module.Globals.Size(); ++i) {
        const auto& glblDef = doc->Module.Globals[i];
        // TODO: Use this method EVERYWHERE. I don't know why I didn't think about this sooner.
        // Index 0 is the root document of the module.
        if (glblDef.DebugSymbol.SourceIdx != 0) continue;
        lsp::Range symbolRange = DocumentRangeToEditorRange(docText, SourcePositionToRange(glblDef.DebugSymbol.Token.SourcePos));
        result.emplace_back(lsp::DocumentSymbol{
            .name = CreateGlobalDefinitionDetails(glblDef, doc),
            .kind = lsp::SymbolKind::Variable,
            .range = symbolRange,
            .selectionRange = symbolRange,
        });
    }
    for (size_t i = 0; i < doc->FunctionDefinitions.Size(); ++i) {
        const auto& funcDef = doc->FunctionDefinitions[i];
        if (funcDef.Definition.DebugSymbol.SourceIdx != 0) continue;
        lsp::Range symbolRange = DocumentRangeToEditorRange(docText, SourcePositionToRange(funcDef.Definition.DebugSymbol.Token.SourcePos));
        result.emplace_back(lsp::DocumentSymbol{
            .name = CreateFunctionDefinitionDetails(funcDef),
            .kind = lsp::SymbolKind::Function,
            .range = symbolRange,
            .selectionRange = symbolRange,
        });
    }
    return result;
}

lsp::CompletionItem PulsarLSP::CreateCompletionItemForBoundEntity(ParsedDocument::SharedRef doc, const BoundGlobal& global, lsp::Range replaceWithName)
{
    lsp::CompletionItem item{};
    item.label = global.Name.CString();
    item.kind  = lsp::CompletionItemKind::Variable;
    if (IsNullRange(replaceWithName)) {
        item.insertText = item.label;
    } else {
        lsp::TextEdit edit{
            .range   = replaceWithName,
            .newText = item.label,
        };
        item.filterText = edit.newText;
        item.textEdit = { std::move(edit) };
    }

    if (global.Index < doc->Module.Globals.Size()) {
        const Pulsar::GlobalDefinition& glblDef = doc->Module.Globals[global.Index];
        item.detail = CreateGlobalDefinitionDetails(glblDef, doc);
    }

    return item;
}

std::string PulsarLSP::CreateGlobalDefinitionDetails(const Pulsar::GlobalDefinition& def, ParsedDocument::SharedRef doc)
{
    std::string detail = "global ";
    if (def.IsConstant) {
        detail += "const ";
    }
    detail += "(";
    if (doc) {
        detail += ValueTypeToString(def.InitialValue, doc->Module).CString();
    } else {
        detail += ValueTypeToString(def.InitialValue.Type());
    }
    detail += ")";
    detail += " -> ";
    detail += def.Name.CString();
    return detail;
}

std::string PulsarLSP::CreateFunctionDefinitionDetails(const FunctionDefinition& lspDef)
{
    const Pulsar::FunctionDefinition& def = lspDef.Definition;

    std::string detail = "(";
    if (lspDef.IsNative) detail += "*";

    detail += def.Name.CString();
    if (def.StackArity > 0) {
        detail += " ";
        detail += std::to_string(def.StackArity);
    }

    for (size_t i = 0; i < lspDef.Args.Size(); ++i) {
        detail += " ";
        detail += lspDef.Args[i].Name.CString();
    }

    detail += ")";
    if (def.Returns > 0) {
        detail += " -> ";
        detail += std::to_string(def.Returns);
    }

    return detail;
}

lsp::CompletionItem PulsarLSP::CreateCompletionItemForBoundEntity(ParsedDocument::SharedRef doc, const BoundFunction& fn, lsp::Range replaceWithName)
{
    lsp::CompletionItem item{};
    item.label  = "(";
    item.label += fn.Name.CString();
    item.label += ")";
    item.kind   = lsp::CompletionItemKind::Function;
    if (IsNullRange(replaceWithName)) {
        item.insertText = item.label;
    } else {
        lsp::TextEdit edit{
            .range   = replaceWithName,
            .newText = fn.Name.CString(),
        };
        item.filterText = edit.newText;
        item.textEdit = { std::move(edit) };
    }

    for (size_t i = 0; i < doc->FunctionDefinitions.Size(); ++i) {
        const FunctionDefinition& lspDef = doc->FunctionDefinitions[i];
        if (!lspDef.IsNative && lspDef.Index == fn.Index) {
            item.detail = CreateFunctionDefinitionDetails(lspDef);
            break;
        }
    }

    return item;
}

lsp::CompletionItem PulsarLSP::CreateCompletionItemForBoundEntity(ParsedDocument::SharedRef doc, const BoundNativeFunction& nativeFn, lsp::Range replaceWithName)
{
    lsp::CompletionItem item{};
    item.label  = "(*";
    item.label += nativeFn.Name.CString();
    item.label += ")";
    item.kind   = lsp::CompletionItemKind::Function;
    if (IsNullRange(replaceWithName)) {
        item.insertText = item.label;
    } else {
        lsp::TextEdit edit{
            .range   = replaceWithName,
            .newText = nativeFn.Name.CString(),
        };
        item.filterText = edit.newText;
        item.textEdit = { std::move(edit) };
    }

    for (size_t i = 0; i < doc->FunctionDefinitions.Size(); ++i) {
        const FunctionDefinition& lspDef = doc->FunctionDefinitions[i];
        if (lspDef.IsNative && lspDef.Index == nativeFn.Index) {
            item.detail = CreateFunctionDefinitionDetails(lspDef);
            break;
        }
    }

    return item;
}

lsp::CompletionItem PulsarLSP::CreateCompletionItemForLocal(const LocalScope::Local& local, lsp::Range replaceWithName)
{
    lsp::CompletionItem item{};
    item.label = local.Name.CString();
    item.kind  = lsp::CompletionItemKind::Variable;
    if (IsNullRange(replaceWithName)) {
        item.insertText = item.label;
    } else {
        lsp::TextEdit edit{
            .range   = replaceWithName,
            .newText = item.label,
        };
        item.filterText = edit.newText;
        item.textEdit = { std::move(edit) };
    }
    return item;
}

std::vector<lsp::CompletionItem> PulsarLSP::Server::GetCompletionItems(const lsp::FileUri& uri, lsp::Position pos)
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

#define PULSAR_CODEBLOCK(code) \
    ("```pulsar\n" + (code) + "\n```")

std::optional<lsp::Hover> PulsarLSP::Server::GetHover(const lsp::FileUri& uri, lsp::Position pos)
{
    auto doc = GetOrParseDocument(uri);
    if (!doc || !doc->Module.HasSourceDebugSymbols()) return std::nullopt;

    ConstSharedText docText = m_Library.FindDocument(uri);
    pos = EditorPositionToDocumentPosition(docText, pos);

    // FIXME: Maybe refactor into a separate method, it's a copy of FindDeclaration
    Pulsar::String path = URIToNormalizedPath(uri);
    for (size_t i = 0; i < doc->FunctionScopes.Size(); ++i) {
        const FunctionScope& fnScope = doc->FunctionScopes[i];
        if (fnScope.FilePath != path) continue;
        for (size_t j = 0; j < fnScope.UsedIdentifiers.Size(); ++j) {
            const IdentifierUsage& identUsage = fnScope.UsedIdentifiers[j];
            if (IsPositionInBetween(pos, identUsage.Identifier.SourcePos)) {
                lsp::Range usageRange = DocumentRangeToEditorRange(docText, SourcePositionToRange(identUsage.Identifier.SourcePos));
                switch (identUsage.Type) {
                case Pulsar::LSPIdentifierUsageType::Global: {
                    if (identUsage.BoundIndex >= doc->Module.Globals.Size()) break;

                    const Pulsar::GlobalDefinition& global = doc->Module.Globals[identUsage.BoundIndex];
                    return lsp::Hover{
                        .contents = lsp::MarkupContent{
                            .kind = lsp::MarkupKind::Markdown,
                            // Markdown should not have script execution (if it does it's non-standard)
                            // So we don't care if custom type names have weird names
                            .value = PULSAR_CODEBLOCK(CreateGlobalDefinitionDetails(global, doc)),
                        },
                        .range = usageRange,
                    };
                }
                case Pulsar::LSPIdentifierUsageType::Function: {
                    if (identUsage.BoundIndex >= doc->Module.Functions.Size()) break;

                    for (size_t k = 0; k < doc->FunctionDefinitions.Size(); ++k) {
                        const FunctionDefinition& fnDef = doc->FunctionDefinitions[k];
                        if (!fnDef.IsNative && fnDef.Index == identUsage.BoundIndex) {
                            return lsp::Hover{
                                .contents = lsp::MarkupContent{
                                    .kind = lsp::MarkupKind::Markdown,
                                    .value = PULSAR_CODEBLOCK(CreateFunctionDefinitionDetails(fnDef)),
                                },
                                .range = usageRange,
                            };
                        }
                    }
                } break;
                case Pulsar::LSPIdentifierUsageType::NativeFunction: {
                    if (identUsage.BoundIndex >= doc->Module.NativeBindings.Size()) break;

                    for (size_t k = 0; k < doc->FunctionDefinitions.Size(); ++k) {
                        const FunctionDefinition& fnDef = doc->FunctionDefinitions[k];
                        if (fnDef.IsNative && fnDef.Index == identUsage.BoundIndex) {
                            return lsp::Hover{
                                .contents = lsp::MarkupContent{
                                    .kind = lsp::MarkupKind::Markdown,
                                    .value = PULSAR_CODEBLOCK(CreateFunctionDefinitionDetails(fnDef)),
                                },
                                .range = usageRange,
                            };
                        }
                    }
                } break;
                case Pulsar::LSPIdentifierUsageType::Local: {
                    return lsp::Hover{
                        .contents = lsp::MarkupContent{
                            .kind = lsp::MarkupKind::Markdown,
                            .value = PULSAR_CODEBLOCK(identUsage.Identifier.StringVal).CString(),
                        },
                        .range = usageRange,
                    };
                }
                default: break;
                }
                return std::nullopt;
            }
        }
    }
    return std::nullopt;
}

std::vector<lsp::CompletionItem> PulsarLSP::Server::GetErrorCompletionItems(ParsedDocument::SharedRef doc, const FunctionScope& funcScope, const LocalScope& localScope)
{
    std::vector<lsp::CompletionItem> result;

    lsp::Range errorRange = DocumentRangeToEditorRange(
        NormalizedPathToURI(doc->ErrorFilePath),
        SourcePositionToRange(doc->ErrorPosition));

    switch (doc->ParseResult) {
    case Pulsar::ParseResult::UsageOfUndeclaredLocal: {
        for (size_t i = 0; i < funcScope.Globals.Size(); ++i) {
            result.emplace_back(CreateCompletionItemForBoundEntity(doc, funcScope.Globals[i], errorRange));
        }
        for (size_t i = 0; i < localScope.Locals.Size(); ++i) {
            result.emplace_back(CreateCompletionItemForLocal(localScope.Locals[i], errorRange));
        }
        // Since the Server listens to TextDocument/DidChange,
        //  and may send a Diagnostic if the user writes 'i',
        //  if it's an unbound global/local, the LSP client should receive the 'if' keyword.
        const PulsarLSP::Completion::KeywordList& kws = PulsarLSP::Completion::GetKeywords();
        for (size_t i = 0; i < kws.Size(); ++i) {
            lsp::CompletionItem item = kws[i];
            lsp::TextEdit edit{
                .range   = errorRange,
                .newText = item.label,
            };
            item.filterText = edit.newText;
            item.textEdit = { std::move(edit) };
            result.emplace_back(std::move(item));
        }
    } break;
    case Pulsar::ParseResult::UsageOfUnknownInstruction: {
        PulsarLSP::Completion::GetInstructions().ForEach([&result, errorRange](const auto& bucket) {
            lsp::CompletionItem item = bucket.Value();
            lsp::TextEdit edit{
                .range   = errorRange,
                .newText = bucket.Key(),
            };
            item.filterText = edit.newText;
            item.textEdit = { std::move(edit) };
            result.emplace_back(std::move(item));
        });
    } break;
    case Pulsar::ParseResult::UsageOfUndeclaredFunction: {
        for (size_t i = 0; i < funcScope.Functions.Size(); ++i) {
            result.emplace_back(CreateCompletionItemForBoundEntity(doc, funcScope.Functions[i], errorRange));
        }
    } break;
    case Pulsar::ParseResult::UsageOfUndeclaredNativeFunction: {
        for (size_t i = 0; i < funcScope.NativeFunctions.Size(); ++i) {
            result.emplace_back(CreateCompletionItemForBoundEntity(doc, funcScope.NativeFunctions[i], errorRange));
        }
    } break;
    case Pulsar::ParseResult::UnexpectedToken: {
        // The following error is catched by this:
        //   else i
        // The 'if' keyword should be sent.
        // We send them all and let the editor filter the keyword.
        const PulsarLSP::Completion::KeywordList& kws = PulsarLSP::Completion::GetKeywords();
        for (size_t i = 0; i < kws.Size(); ++i) {
            lsp::CompletionItem item = kws[i];
            lsp::TextEdit edit{
                .range   = errorRange,
                .newText = item.label,
            };
            item.filterText = edit.newText;
            item.textEdit = { std::move(edit) };
            result.emplace_back(std::move(item));
        }
    } break;
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

    const PulsarLSP::Completion::KeywordList& kws = PulsarLSP::Completion::GetKeywords();
    for (size_t j = 0; j < kws.Size(); ++j) {
        lsp::CompletionItem item = kws[j];
        item.insertText = item.label;
        result.emplace_back(std::move(item));
    }

    return result;
}

std::vector<PulsarLSP::DiagnosticsForDocument> PulsarLSP::Server::GetDiagnosticReport(const lsp::FileUri& uri, bool sameDocument)
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

    // Make sure the file was retrieved correctly
    if (badDoc) {
        lsp::Range errorRange = DocumentRangeToEditorRange(
            NormalizedPathToURI(doc->ErrorFilePath),
            SourcePositionToRange(badDoc->ErrorPosition));

        lsp::Diagnostic error;
        error.range    = errorRange;
        error.message  = Pulsar::ParseResultToString(badDoc->ParseResult);
        error.message += ": ";
        error.message += badDoc->ErrorMessage.CString();
        error.source   = "pulsar";
        error.severity = lsp::DiagnosticSeverity::Error;
        result.push_back(DiagnosticsForDocument{
            .uri = NormalizedPathToURI(badDoc->ErrorFilePath),
            .diagnostics = {std::move(error)},
            .version = std::nullopt
        });
    }

    return result;
}

void PulsarLSP::Server::SendUpdatedDiagnosticReport(const lsp::FileUri& uri)
{
    Pulsar::String docPath = URIToNormalizedPath(uri);

    auto diagnostics = this->GetDiagnosticReport(uri, false);
    for (size_t i = 0; i < diagnostics.size(); ++i) {
        m_MessageHandler.sendNotification<lsp::notifications::TextDocument_PublishDiagnostics>(
            static_cast<lsp::notifications::TextDocument_PublishDiagnostics::Params>(diagnostics[i]));
    }
}

void PulsarLSP::Server::ResetDiagnosticReport(const lsp::FileUri& uri)
{
    Pulsar::String normalizedPath = URIToNormalizedPath(uri);
    ResetDiagnosticReport(normalizedPath);
}

void PulsarLSP::Server::ResetDiagnosticReport(const Pulsar::String& normalizedPath)
{
    m_MessageHandler.sendNotification<lsp::notifications::TextDocument_PublishDiagnostics>(
        lsp::notifications::TextDocument_PublishDiagnostics::Params{
            // FIXME: Some editors do not seem to like relative URIs (mayber they're serialized incorrectly by lsp-framework)
            .uri = NormalizedPathToURI(normalizedPath),
            .diagnostics = {},
            .version = std::nullopt
        }
    );
}

lsp::Position PulsarLSP::Server::DocumentPositionToEditorPosition(ConstSharedText doc, lsp::Position pos) const
{
    if (!doc) return pos;

    UTF8::Decoder start(*doc);
    UTF8::DecoderExt::AdvanceToLine(start, 0, (size_t)pos.line);

    lsp::uint editorStartChar = 0;
    for (lsp::uint i = 0; i < pos.character && start; ++i) {
        editorStartChar += (lsp::uint)Unicode::GetEncodedSize(start.Next(), GetPositionEncoding());
    }

    return lsp::Position{
        .line = pos.line,
        .character = editorStartChar
    };
}

lsp::Position PulsarLSP::Server::EditorPositionToDocumentPosition(ConstSharedText doc, lsp::Position pos) const
{
    if (!doc) return pos;

    UTF8::Decoder start(*doc);
    UTF8::DecoderExt::AdvanceToLine(start, 0, (size_t)pos.line);

    lsp::uint startChar = 0;
    for (lsp::uint i = 0; i < pos.character && start; ++startChar) {
        i += (lsp::uint)Unicode::GetEncodedSize(start.Next(), GetPositionEncoding());
    }

    return lsp::Position{
        .line = pos.line,
        .character = startChar
    };
}

lsp::Position PulsarLSP::Server::DocumentPositionToEditorPosition(const lsp::FileUri& uri, lsp::Position pos) const
{
    ConstSharedText doc = m_Library.FindDocument(uri);
    return DocumentPositionToEditorPosition(doc, pos);
}

lsp::Position PulsarLSP::Server::EditorPositionToDocumentPosition(const lsp::FileUri& uri, lsp::Position pos) const
{
    ConstSharedText doc = m_Library.FindDocument(uri);
    return EditorPositionToDocumentPosition(doc, pos);
}

lsp::Range PulsarLSP::Server::DocumentRangeToEditorRange(ConstSharedText doc, lsp::Range range) const
{
    if (!doc) return range;

    UTF8::Decoder start(*doc);
    UTF8::DecoderExt::AdvanceToLine(start, 0, (size_t)range.start.line);

    UTF8::Decoder end = start;
    UTF8::DecoderExt::AdvanceToLine(end, (size_t)range.start.line, (size_t)range.end.line);

    lsp::uint editorStartChar = 0;
    for (lsp::uint i = 0; i < range.start.character && start; ++i) {
        editorStartChar += (lsp::uint)Unicode::GetEncodedSize(start.Next(), GetPositionEncoding());
    }

    lsp::uint editorEndChar = 0;
    for (lsp::uint i = 0; i < range.end.character && end; ++i) {
        editorEndChar += (lsp::uint)Unicode::GetEncodedSize(end.Next(), GetPositionEncoding());
    }

    return lsp::Range{
        .start = {
            .line = range.start.line,
            .character = editorStartChar
        },
        .end = {
            .line = range.end.line,
            .character = editorEndChar
        }
    };
}

lsp::Range PulsarLSP::Server::EditorRangeToDocumentRange(ConstSharedText doc, lsp::Range range) const
{
    if (!doc) return range;

    UTF8::Decoder start(*doc);
    UTF8::DecoderExt::AdvanceToLine(start, 0, (size_t)range.start.line);

    UTF8::Decoder end = start;
    UTF8::DecoderExt::AdvanceToLine(end, (size_t)range.start.line, (size_t)range.end.line);

    lsp::uint startChar = 0;
    for (lsp::uint i = 0; i < range.start.character && start; ++startChar) {
        i += (lsp::uint)Unicode::GetEncodedSize(start.Next(), GetPositionEncoding());
    }

    lsp::uint endChar = 0;
    for (lsp::uint i = 0; i < range.end.character && end; ++endChar) {
        i += (lsp::uint)Unicode::GetEncodedSize(end.Next(), GetPositionEncoding());
    }

    return lsp::Range{
        .start = {
            .line = range.start.line,
            .character = startChar
        },
        .end = {
            .line = range.end.line,
            .character = endChar
        }
    };
}

lsp::Range PulsarLSP::Server::DocumentRangeToEditorRange(const lsp::FileUri& uri, lsp::Range range) const
{
    ConstSharedText doc = m_Library.FindDocument(uri);
    return DocumentRangeToEditorRange(doc, range);
}

lsp::Range PulsarLSP::Server::EditorRangeToDocumentRange(const lsp::FileUri& uri, lsp::Range range) const
{
    ConstSharedText doc = m_Library.FindDocument(uri);
    return EditorRangeToDocumentRange(doc, range);
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

PulsarLSP::ParsedDocument::SharedRef PulsarLSP::Server::ParseDocument(const lsp::FileUri& uri, bool forceLoad)
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

PulsarLSP::Server::Server(lsp::Connection& connection)
    : m_MessageHandler(connection),
      m_Library(),
      m_ParsedCache()
{
    m_MessageHandler
        .add<lsp::requests::Initialize>([this](lsp::requests::Initialize::Params&& params)
        {
            if (params.initializationOptions && params.initializationOptions->isObject()) {
                const auto& userOptions = params.initializationOptions->object();

                GET_INIT_BOOLEAN_OPTION(diagnosticsOnOpen,        DiagnosticsOnOpen);
                GET_INIT_BOOLEAN_OPTION(diagnosticsOnSave,        DiagnosticsOnSave);
                GET_INIT_BOOLEAN_OPTION(diagnosticsOnChange,      DiagnosticsOnChange);
                GET_INIT_BOOLEAN_OPTION(fullSyncOnSave,           FullSyncOnSave);
                GET_INIT_BOOLEAN_OPTION(mapGlobalProducersToVoid, MapGlobalProducersToVoid);

                if (auto includePaths = userOptions.find("includePaths");
                    includePaths != userOptions.end() &&
                    includePaths->second.isArray()
                ) {
                    for (const auto& includePath : includePaths->second.array()) {
                        if (!includePath.isString())
                            continue;
                        this->m_Options.IncludePaths.emplace_back(includePath.string());
                    }
                }
            }

            // You can see the standard was developed by Microsoft
            //  because UTF16 is the default encoding.
            PositionEncodingKind positionEncoding = PositionEncodingKind::UTF16;

            bool supportsUTF8  = false;
            bool supportsUTF32 = false;
            if (params.capabilities.general && params.capabilities.general->positionEncodings) {
                for (auto encoding : *params.capabilities.general->positionEncodings) {
                    // TODO: Add some optimizations to UTF8 and UTF32 if needed
                    if (encoding == PositionEncodingKind::UTF8) {
                        supportsUTF8 = true;
                    } else if (encoding == PositionEncodingKind::UTF32) {
                        supportsUTF32 = true;
                    }
                }
            }

            // Because this project is *based* we'll prefer any encoding other than ::UTF16
            // Windows devs be in shambles fr rn 😎👍
            if (supportsUTF8) {
                positionEncoding = PositionEncodingKind::UTF8;
            } else if (supportsUTF32) {
                positionEncoding = PositionEncodingKind::UTF32;
            }

            this->m_Library.SetPositionEncoding(positionEncoding);
            return lsp::requests::Initialize::Result{
                .capabilities = {
                    .positionEncoding       = positionEncoding,
                    .textDocumentSync       = lsp::TextDocumentSyncOptions{
                        .openClose = true,
                        .change    = lsp::TextDocumentSyncKind::Incremental,
                        .save      = lsp::SaveOptions{ .includeText = this->m_Options.FullSyncOnSave },
                    },
                    .completionProvider     = lsp::CompletionOptions{},
                    .hoverProvider          = true,
                    .declarationProvider    = true,
                    .definitionProvider     = true,
                    .documentSymbolProvider = true,
                    .diagnosticProvider     = lsp::DiagnosticOptions{
                        .interFileDependencies = true,
                        .workspaceDiagnostics  = false,
                        .identifier            = "pulsar",
                    }
                },
                .serverInfo = lsp::InitializeResultServerInfo{
                    .name    = "Pulsar",
                    .version = {}
                }
            };
        })
        .add<lsp::requests::TextDocument_Completion>([this](lsp::requests::TextDocument_Completion::Params&& params)
            -> lsp::requests::TextDocument_Completion::Result
        {
            return this->GetCompletionItems(params.textDocument.uri, params.position);
        })
        .add<lsp::requests::TextDocument_Hover>([this](lsp::requests::TextDocument_Hover::Params&& params)
            -> lsp::requests::TextDocument_Hover::Result
        {
            auto hover = this->GetHover(params.textDocument.uri, params.position);
            if (hover) return *hover;
            return nullptr;
        })
        .add<lsp::requests::TextDocument_Declaration>([this](lsp::requests::TextDocument_Declaration::Params&& params)
            -> lsp::requests::TextDocument_Declaration::Result
        {
            auto decl = this->FindDeclaration(params.textDocument.uri, params.position);
            if (decl) return *decl;
            return nullptr;
        })
        .add<lsp::requests::TextDocument_Definition>([this](lsp::requests::TextDocument_Definition::Params&& params)
            -> lsp::requests::TextDocument_Definition::Result
        {
            auto def = this->FindDefinition(params.textDocument.uri, params.position);
            if (def) return *def;
            return nullptr;
        })
        .add<lsp::requests::TextDocument_DocumentSymbol>([this](lsp::requests::TextDocument_DocumentSymbol::Params&& params)
            -> lsp::requests::TextDocument_DocumentSymbol::Result
        {
            return this->GetSymbols(params.textDocument.uri);
        })
        .add<lsp::requests::TextDocument_Diagnostic>([](lsp::requests::TextDocument_Diagnostic::Params&& params)
            -> lsp::requests::TextDocument_Diagnostic::Result
        {
            (void)params;
            lsp::RelatedFullDocumentDiagnosticReport result;
            return result;
        })
        .add<lsp::notifications::TextDocument_DidOpen>([this](lsp::notifications::TextDocument_DidOpen::Params&& params)
        {
            if (this->OpenDocument(params.textDocument.uri, params.textDocument.text, params.textDocument.version) && this->m_Options.DiagnosticsOnOpen) {
                this->SendUpdatedDiagnosticReport(params.textDocument.uri);
            }
        })
        .add<lsp::notifications::TextDocument_DidClose>([this](lsp::notifications::TextDocument_DidClose::Params&& params)
        {
            this->CloseDocument(params.textDocument.uri);
        })
        .add<lsp::notifications::TextDocument_DidSave>([this](lsp::notifications::TextDocument_DidSave::Params&& params)
        {
            if (this->m_Options.FullSyncOnSave && params.text.has_value()) {
                if (this->OpenDocument(params.textDocument.uri, *params.text) && this->m_Options.DiagnosticsOnSave) {
                    this->SendUpdatedDiagnosticReport(params.textDocument.uri);
                }
            } else if (this->m_Options.DiagnosticsOnSave) {
                this->SendUpdatedDiagnosticReport(params.textDocument.uri);
            }
        })
        .add<lsp::notifications::TextDocument_DidChange>([this](lsp::notifications::TextDocument_DidChange::Params&& params)
        {
            this->PatchDocument(params.textDocument.uri, params.contentChanges, params.textDocument.version);
            if (this->m_Options.DiagnosticsOnChange) {
                this->SendUpdatedDiagnosticReport(params.textDocument.uri);
            }
        })
        .add<lsp::requests::Shutdown>([this]()
            -> lsp::requests::Shutdown::Result
        {
            this->DropAllParsedDocuments();
            this->m_Library.DeleteAllDocuments();
            this->m_Options.IncludePaths.clear();
            return nullptr;
        })
        .add<lsp::notifications::Exit>([this]()
        {
            this->m_IsRunning = false;
        });
}

void PulsarLSP::Server::Run()
{
    if (m_IsRunning) return;

    m_IsRunning = true;
    while (m_IsRunning) {
        m_MessageHandler.processIncomingMessages();
    }
}
