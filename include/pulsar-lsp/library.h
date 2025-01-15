#ifndef _PULSARLSP_LIBRARY_H
#define _PULSARLSP_LIBRARY_H

#include <optional>

#include <lsp/fileuri.h>
#include <lsp/types.h>

#include "pulsar/structures/hashmap.h"
#include "pulsar/structures/list.h"
#include "pulsar/structures/ref.h"
#include "pulsar/structures/string.h"

namespace PulsarLSP
{
    Pulsar::String URIToNormalizedPath(const lsp::FileURI& uri);
    lsp::FileURI NormalizedPathToURI(const Pulsar::String& path);
    inline lsp::FileURI RecomputeURI(const lsp::FileURI& uri) { return NormalizedPathToURI(URIToNormalizedPath(uri)); }

    bool ReadFile(const lsp::FileURI& uri, std::string& buffer);
    bool ReadFile(const lsp::FileURI& uri, Pulsar::String& buffer);

    using SharedText = Pulsar::SharedRef<Pulsar::String>;
    using ConstSharedText = Pulsar::SharedRef<const Pulsar::String>;

    struct CachedDocument
    {
        using SharedRef = Pulsar::SharedRef<CachedDocument>;

        lsp::FileURI URI;
        SharedText Text;
        int Version;
    };

    using DocumentPatches = std::vector<lsp::TextDocumentContentChangeEvent>;

    // A class which stores documents and can apply changes to them
    class Library
    {
    public:
        Library() = default;
        ~Library() = default;

        ConstSharedText FindDocument(const lsp::FileURI& uri) const;
        bool HasDocument(const lsp::FileURI& uri) const { return (bool)FindDocument(uri); }

        ConstSharedText FindOrLoadDocument(const lsp::FileURI& uri, int version=-1);

        ConstSharedText LoadDocument(const lsp::FileURI& uri, int version=-1);
        ConstSharedText StoreDocument(const lsp::FileURI& uri, const Pulsar::String& text, int version=-1);
        ConstSharedText PatchDocument(const lsp::FileURI& uri, const DocumentPatches& patches, int newVersion=-1);
        void DeleteDocument(const lsp::FileURI& uri);
        void DeleteAllDocuments();

    private:
        Pulsar::HashMap<Pulsar::String, CachedDocument::SharedRef> m_Documents;
    };

}

#endif // _PULSARLSP_LIBRARY_H
