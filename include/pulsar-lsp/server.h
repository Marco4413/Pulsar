#ifndef _PULSARLSP_SERVER_H
#define _PULSARLSP_SERVER_H

#include <lsp/connection.h>
#include <lsp/fileuri.h>
#include <lsp/types.h>

#include "pulsar/parser.h"
#include "pulsar/runtime.h"
#include "pulsar/structures/hashmap.h"

namespace PulsarLSP
{
    Pulsar::String URIToNormalizedPath(const lsp::FileURI& uri);
    lsp::FileURI NormalizedPathToURI(const Pulsar::String& path);

    struct LocalScope
    {
        Pulsar::SourcePosition StartPos;
        Pulsar::SourcePosition EndPos;
        Pulsar::List<Pulsar::LocalScope::LocalVar> Locals;
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

    // TODO: A FunctionScope should only contain the scope of a single function.
    //       Currently all functions with the same name and in the same file are combined together.
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

    struct FunctionDefinition
    {
        Pulsar::String FilePath;
        bool IsNative;
        size_t Index;
        Pulsar::FunctionDefinition Definition;
        Pulsar::List<Pulsar::LocalScope::LocalVar> Args;
    };

    struct UserProvidedOptions
    {
        bool MapGlobalProducersToVoid = true;
    };

    inline const UserProvidedOptions UserProvidedOptions_Default{};

    struct ParsedDocument
    {
        using SharedRef = Pulsar::SharedRef<ParsedDocument>;

        Pulsar::Module Module;
        Pulsar::List<FunctionScope> FunctionScopes;
        Pulsar::List<FunctionDefinition> FunctionDefinitions;

        static std::optional<ParsedDocument> From(const lsp::FileURI& uri, bool extractAll=false, UserProvidedOptions opt=UserProvidedOptions_Default);
    };

    // Does not modify `doc`
    lsp::CompletionItem CreateCompletionItemForBoundEntity(ParsedDocument::SharedRef doc, const BoundGlobal& global);
    lsp::CompletionItem CreateCompletionItemForBoundEntity(ParsedDocument::SharedRef doc, const BoundFunction& fn);
    lsp::CompletionItem CreateCompletionItemForBoundEntity(ParsedDocument::SharedRef doc, const BoundNativeFunction& nativeFn);

    class Server
    {
    public:
        Server() = default;
        ~Server() = default;

        ParsedDocument::SharedRef GetDocument(const lsp::FileURI& uri) const;
        ParsedDocument::SharedRef GetOrParseDocument(const lsp::FileURI& uri, bool forceCache=false);
        void DropDocument(const lsp::FileURI& uri);
        void DropAllDocuments();

        std::optional<lsp::Location> FindDeclaration(const lsp::FileURI& uri, lsp::Position pos);
        std::vector<lsp::CompletionItem> GetCompletionItems(const lsp::FileURI& uri, lsp::Position pos);

        void Run(lsp::Connection& connection);

    private:
        void StripModule(Pulsar::Module& mod) const;
        ParsedDocument::SharedRef ParseAndStoreDocument(const lsp::FileURI& uri);

    private:
        Pulsar::HashMap<Pulsar::String, ParsedDocument::SharedRef> m_DocumentCache;
        UserProvidedOptions m_Options = UserProvidedOptions_Default;
    };

}

#endif // _PULSARLSP_SERVER_H
