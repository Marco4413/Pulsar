#ifndef _PULSARLSP_SERVER_H
#define _PULSARLSP_SERVER_H

#include <lsp/connection.h>
#include <lsp/fileuri.h>
#include <lsp/messagehandler.h>
#include <lsp/types.h>

#include "pulsar/parser.h"
#include "pulsar/runtime.h"
#include "pulsar/structures/hashmap.h"

#include "pulsar-lsp/library.h"

namespace PulsarLSP
{
    struct LocalScope
    {
        using Local = Pulsar::LocalScope::LocalVar;

        bool IsOpen; // True if the scope was never closed due to errors
        Pulsar::SourcePosition StartPos;
        Pulsar::SourcePosition EndPos;
        Pulsar::List<Local> Locals;
    };

    struct IdentifierUsage
    {
        Pulsar::Token Identifier;
        Pulsar::LSPIdentifierUsageType Type;
        size_t BoundIndex;
        // This contains the position of the local this entity is bound to when Type == ::Local
        Pulsar::SourcePosition LocalDeclaredAt = {};
    };

    struct BoundGlobal
    {
        Pulsar::String Name;
        size_t Index;
    };

    struct BoundFunction
    {
        Pulsar::String Name;
        size_t Index;
    };

    struct BoundNativeFunction
    {
        Pulsar::String Name;
        size_t Index;
    };

    struct FunctionScope
    {
        Pulsar::String FilePath;
        Pulsar::String Name;
        Pulsar::List<BoundGlobal> Globals;
        Pulsar::List<BoundFunction> Functions;
        Pulsar::List<BoundNativeFunction> NativeFunctions;
        Pulsar::List<LocalScope> LocalScopes;
        Pulsar::List<IdentifierUsage> UsedIdentifiers;
    };

    class FunctionScopeBuilder
    {
    public:
        FunctionScopeBuilder() = default;
        ~FunctionScopeBuilder() = default;

        FunctionScopeBuilder& SetFilePath(const Pulsar::String& path);
        FunctionScopeBuilder& SetName(const Pulsar::String& name);
        FunctionScopeBuilder& AddGlobal(BoundGlobal&& global);
        FunctionScopeBuilder& AddFunction(BoundFunction&& func);
        FunctionScopeBuilder& AddNativeFunction(BoundNativeFunction&& nativeFunc);
        FunctionScopeBuilder& AddIdentifierUsage(IdentifierUsage&& identUsage);

        bool HasOpenBlock() const { return m_OpenLocalScopes.Size() > 0; }
        FunctionScopeBuilder& StartBlock(Pulsar::SourcePosition pos, const Pulsar::List<LocalScope::Local>& locals);
        FunctionScopeBuilder& EndBlock(Pulsar::SourcePosition pos);

        FunctionScope&& Build();

    private:
        FunctionScope m_FunctionScope;
        Pulsar::List<LocalScope> m_OpenLocalScopes;
    };

    struct FunctionDefinition
    {
        Pulsar::String FilePath;
        bool IsNative;
        size_t Index;
        Pulsar::FunctionDefinition Definition;
        Pulsar::List<LocalScope::Local> Args;
    };

    struct IncludedFile
    {
        Pulsar::String FilePath;
        Pulsar::SourcePosition IncludedAt;
    };

    struct UserProvidedOptions
    {
        // Send diagnostics on open
        bool DiagnosticsOnOpen   = true;
        // Send diagnostics on save
        bool DiagnosticsOnSave   = true;
        // Send diagnostics on change
        bool DiagnosticsOnChange = true;
        // Resync full document on save
        bool FullSyncOnSave      = true;
        bool MapGlobalProducersToVoid = true;
    };

    inline const UserProvidedOptions UserProvidedOptions_Default{};

    struct ParsedDocument
    {
        using SharedRef = Pulsar::SharedRef<ParsedDocument>;

        Pulsar::Module Module;
        Pulsar::List<IncludedFile> IncludedFiles;
        Pulsar::List<FunctionScope> FunctionScopes;
        Pulsar::List<FunctionDefinition> FunctionDefinitions;

        Pulsar::ParseResult ParseResult;
        Pulsar::String         ErrorFilePath;
        Pulsar::SourcePosition ErrorPosition;
        Pulsar::String         ErrorMessage;
    };

    constexpr Pulsar::SourcePosition NULL_SOURCE_POSITION{0,0,0,0};
    constexpr lsp::Range NULL_RANGE{.start={0,0},.end={0,0}};

    // Does not modify `doc`
    // doc may be nullptr, it is used to resolve custom type names
    std::string CreateGlobalDefinitionDetails(const Pulsar::GlobalDefinition& def, ParsedDocument::SharedRef doc);
    std::string CreateFunctionDefinitionDetails(const FunctionDefinition& lspDef);

    // Does not modify `doc`
    // If replaceWithName is non-zero, the characters referenced by it are replaced with the name of the entity.
    lsp::CompletionItem CreateCompletionItemForBoundEntity(ParsedDocument::SharedRef doc, const BoundGlobal& global, lsp::Range replaceWithName=NULL_RANGE);
    lsp::CompletionItem CreateCompletionItemForBoundEntity(ParsedDocument::SharedRef doc, const BoundFunction& fn, lsp::Range replaceWithName=NULL_RANGE);
    lsp::CompletionItem CreateCompletionItemForBoundEntity(ParsedDocument::SharedRef doc, const BoundNativeFunction& nativeFn, lsp::Range replaceWithName=NULL_RANGE);
    lsp::CompletionItem CreateCompletionItemForLocal(const LocalScope::Local& local, lsp::Range replaceWithName=NULL_RANGE);

    using DiagnosticsForDocument = lsp::PublishDiagnosticsParams;

    class Server
    {
    public:
        // connection and this instance MUST NOT change address for the lifespan of the Server object.
        Server(lsp::Connection& connection);
        ~Server() = default;

        ParsedDocument::SharedRef GetParsedDocument(const lsp::FileURI& uri) const;
        ParsedDocument::SharedRef GetOrParseDocument(const lsp::FileURI& uri, bool forceLoad=false);

        ParsedDocument::SharedRef ParseDocument(const lsp::FileURI& uri, bool forceLoad=false);

        void DropParsedDocument(const lsp::FileURI& uri);
        void DropAllParsedDocuments();

        bool OpenDocument(const lsp::FileURI& uri, const std::string& docText, int version=-1);
        void PatchDocument(const lsp::FileURI& uri, const DocumentPatches& patches, int version=-1);
        void CloseDocument(const lsp::FileURI& uri);
        // Close and drop parsed document
        void DeleteDocument(const lsp::FileURI& uri);

        std::optional<lsp::Location> FindDeclaration(const lsp::FileURI& uri, lsp::Position pos);
        std::optional<lsp::Location> FindDefinition(const lsp::FileURI& uri, lsp::Position pos);
        std::vector<lsp::DocumentSymbol> GetSymbols(const lsp::FileURI& uri);
        std::vector<lsp::CompletionItem> GetCompletionItems(const lsp::FileURI& uri, lsp::Position pos);
        std::optional<lsp::Hover> GetHover(const lsp::FileURI& uri, lsp::Position pos);
        // Every element of the vector has a unique URI
        std::vector<DiagnosticsForDocument> GetDiagnosticReport(const lsp::FileURI& uri, bool sameDocument=true);
        void SendUpdatedDiagnosticReport(const lsp::FileURI& uri);
        void ResetDiagnosticReport(const lsp::FileURI& uri);

        bool IsRunning() const { return m_IsRunning; }
        void Run();

        PositionEncodingKind GetPositionEncoding() const { return m_Library.GetPositionEncoding(); }
    private:
        void StripModule(Pulsar::Module& mod) const;

        // normalizedPath MUST BE NORMALIZED
        void ResetDiagnosticReport(const Pulsar::String& normalizedPath);

        lsp::Position DocumentPositionToEditorPosition(ConstSharedText doc, lsp::Position pos) const;
        lsp::Position EditorPositionToDocumentPosition(ConstSharedText doc, lsp::Position pos) const;

        lsp::Position DocumentPositionToEditorPosition(const lsp::FileURI& uri, lsp::Position pos) const;
        lsp::Position EditorPositionToDocumentPosition(const lsp::FileURI& uri, lsp::Position pos) const;

        lsp::Range DocumentRangeToEditorRange(ConstSharedText doc, lsp::Range range) const;
        lsp::Range EditorRangeToDocumentRange(ConstSharedText doc, lsp::Range range) const;

        lsp::Range DocumentRangeToEditorRange(const lsp::FileURI& uri, lsp::Range range) const;
        lsp::Range EditorRangeToDocumentRange(const lsp::FileURI& uri, lsp::Range range) const;

        std::vector<lsp::CompletionItem> GetErrorCompletionItems(ParsedDocument::SharedRef doc, const FunctionScope& funcScope, const LocalScope& localScope);
        std::vector<lsp::CompletionItem> GetScopeCompletionItems(ParsedDocument::SharedRef doc, const FunctionScope& funcScope, const LocalScope& localScope);

        std::optional<ParsedDocument> CreateParsedDocument(const lsp::FileURI& uri, const Pulsar::String& document, bool extractAll=false);
    private:
        bool m_IsRunning = false;
        lsp::MessageHandler m_MessageHandler;
        Library m_Library;

        Pulsar::HashMap<Pulsar::String, ParsedDocument::SharedRef> m_ParsedCache;
        UserProvidedOptions m_Options = UserProvidedOptions_Default;
    };

}

#endif // _PULSARLSP_SERVER_H
