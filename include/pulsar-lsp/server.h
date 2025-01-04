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

    struct Scope
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

    struct Function
    {
        Pulsar::String FilePath;
        Pulsar::String Name;
        Pulsar::List<Pulsar::String> Globals;
        Pulsar::List<Pulsar::String> Functions;
        Pulsar::List<Pulsar::String> NativeFunctions;
        Pulsar::List<Scope> Scopes;
        Pulsar::List<IdentifierUsage> UsedIdentifiers;
    };

    struct ParsedDocument
    {
        Pulsar::Module Module;
        Pulsar::List<Function> Functions;

        static std::optional<ParsedDocument> From(const lsp::FileURI& uri, bool extractAll=false);
    };

    class Server
    {
    public:
        Server() = default;
        ~Server() = default;

        Pulsar::SharedRef<ParsedDocument> GetDocument(const lsp::FileURI& uri) const;
        Pulsar::SharedRef<ParsedDocument> GetOrParseDocument(const lsp::FileURI& uri, bool forceCache=false);
        void DropDocument(const lsp::FileURI& uri);
        void DropAllDocuments();

        std::optional<lsp::Location> FindDeclaration(const lsp::FileURI& uri, lsp::Position pos);
        std::vector<lsp::CompletionItem> GetCompletionItems(const lsp::FileURI& uri, lsp::Position pos);

        void Run(lsp::Connection& connection);

    private:
        Pulsar::SharedRef<ParsedDocument> ParseAndStoreDocument(const lsp::FileURI& uri);

    private:
        Pulsar::HashMap<Pulsar::String, Pulsar::SharedRef<ParsedDocument>> m_DocumentCache;
    };

}

#endif // _PULSARLSP_SERVER_H
