#ifndef _PULSARLSP_LIBRARY_H
#define _PULSARLSP_LIBRARY_H

#include <optional>

#include <lsp/fileuri.h>
#include <lsp/types.h>

#include "pulsar/sourceviewer.h"
#include "pulsar/utf8.h"
#include "pulsar/structures/hashmap.h"
#include "pulsar/structures/list.h"
#include "pulsar/structures/ref.h"
#include "pulsar/structures/string.h"

namespace PulsarLSP
{
    Pulsar::String URIToNormalizedPath(const lsp::FileUri& uri);
    lsp::FileUri NormalizedPathToURI(const Pulsar::String& path);
    inline lsp::FileUri RecomputeURI(const lsp::FileUri& uri) { return NormalizedPathToURI(URIToNormalizedPath(uri)); }

    bool ReadFile(const lsp::FileUri& uri, std::string& buffer);
    bool ReadFile(const lsp::FileUri& uri, Pulsar::String& buffer);

    using SharedText = Pulsar::SharedRef<Pulsar::String>;
    using ConstSharedText = Pulsar::SharedRef<const Pulsar::String>;

    struct CachedDocument
    {
        using SharedRef = Pulsar::SharedRef<CachedDocument>;

        lsp::FileUri URI;
        SharedText Text;
        int Version;
    };

    using DocumentPatches = std::vector<lsp::TextDocumentContentChangeEvent>;
    using PositionEncodingKind = lsp::PositionEncodingKind;

    constexpr Pulsar::SourceViewer::PositionEncoding ToPulsarPositionEncoding(PositionEncodingKind encodingKind)
    {
        switch (encodingKind) {
        case PositionEncodingKind::UTF8:
            return Pulsar::SourceViewer::PositionEncoding::UTF8;
        case PositionEncodingKind::UTF16:
            return Pulsar::SourceViewer::PositionEncoding::UTF16;
        case PositionEncodingKind::UTF32:
            return Pulsar::SourceViewer::PositionEncoding::UTF32;
        default:
            return Pulsar::SourceViewer::PositionEncoding::UTF32;
        }
    }

    namespace Unicode
    {
        using Codepoint = Pulsar::Unicode::Codepoint;
        size_t GetEncodedSize(Codepoint code, PositionEncodingKind encodingKind);
    }

    namespace UTF8
    {
        using Decoder = Pulsar::UTF8::Decoder;
        using Codepoint = Pulsar::UTF8::Codepoint;
    }

    // A class which stores documents and can apply changes to them
    class Library
    {
    public:
        Library() = default;
        ~Library() = default;

        ConstSharedText FindDocument(const lsp::FileUri& uri) const;
        bool HasDocument(const lsp::FileUri& uri) const { return (bool)FindDocument(uri); }

        ConstSharedText FindOrLoadDocument(const lsp::FileUri& uri, int version=-1);

        ConstSharedText LoadDocument(const lsp::FileUri& uri, int version=-1);
        ConstSharedText StoreDocument(const lsp::FileUri& uri, const Pulsar::String& text, int version=-1);
        ConstSharedText PatchDocument(const lsp::FileUri& uri, const DocumentPatches& patches, int newVersion=-1);
        void DeleteDocument(const lsp::FileUri& uri);
        void DeleteAllDocuments();

        PositionEncodingKind GetPositionEncoding() const    { return m_PositionEncoding; }
        void SetPositionEncoding(PositionEncodingKind kind) { m_PositionEncoding = kind; }
    private:
        std::pair<size_t, size_t> GetRangeIndices(const Pulsar::String& text, lsp::Range range) const;
        void PutRangePatch(Pulsar::String& text, const lsp::TextDocumentContentChangeEvent_Range_Text& change);

    private:
        // Text is UTF8-encoded. However, positions are UTF16-encoded by default.
        Pulsar::HashMap<Pulsar::String, CachedDocument::SharedRef> m_Documents;
        PositionEncodingKind m_PositionEncoding = PositionEncodingKind::UTF16;
    };

}

#endif // _PULSARLSP_LIBRARY_H
