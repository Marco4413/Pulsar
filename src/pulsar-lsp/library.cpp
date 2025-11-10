#include "pulsar-lsp/library.h"

#include <filesystem>
#include <fstream>

#include "pulsar/parser.h"
#include "pulsar/utf8.h"

#include "pulsar-lsp/helpers.h"

Pulsar::String PulsarLSP::URIToNormalizedPath(const lsp::FileUri& uri)
{
    std::filesystem::path path(uri.path());
    Pulsar::String internalPath(path.generic_string().c_str());

    // Ignore return value, if false internalPath is not changed
    Pulsar::Parser::PathToNormalizedFileSystemPath(internalPath, internalPath);
    return internalPath;
}

lsp::FileUri PulsarLSP::NormalizedPathToURI(const Pulsar::String& path)
{
    std::filesystem::path relPath(path.CString());
    std::error_code error;
    std::filesystem::path absPath = std::filesystem::absolute(relPath, error);
    lsp::FileUri uri = lsp::FileUri::fromPath(
        !(error || absPath.empty())
            ? absPath.string()
            : relPath.string()
    );
    return uri;
}

bool PulsarLSP::ReadFile(const lsp::FileUri& uri, std::string& buffer)
{
    Pulsar::String path  = URIToNormalizedPath(uri);
    std::filesystem::path fsPath = path.CString();
    if (!std::filesystem::exists(fsPath)) return false;

    std::ifstream file(fsPath, std::ios::binary);
    size_t fileSize = (size_t)std::filesystem::file_size(fsPath);

    buffer.resize(fileSize);
    return (bool)file.read(buffer.data(), fileSize);
}

bool PulsarLSP::ReadFile(const lsp::FileUri& uri, Pulsar::String& buffer)
{
    Pulsar::String path  = URIToNormalizedPath(uri);
    std::filesystem::path fsPath = path.CString();
    if (!std::filesystem::exists(fsPath)) return false;

    std::ifstream file(fsPath, std::ios::binary);
    size_t fileSize = (size_t)std::filesystem::file_size(fsPath);

    buffer.Resize(fileSize);
    return (bool)file.read(buffer.Data(), fileSize);
}


size_t PulsarLSP::Unicode::GetEncodedSize(Codepoint code, PositionEncodingKind encodingKind)
{
    return Pulsar::SourceViewer::GetEncodedSize(code, ToPulsarPositionEncoding(encodingKind));
}

PulsarLSP::ConstSharedText PulsarLSP::Library::FindDocument(const lsp::FileUri& uri) const
{
    Pulsar::String path = URIToNormalizedPath(uri);
    auto doc = m_Documents.Find(path);
    return doc ? doc->Value()->Text : nullptr;
}

PulsarLSP::ConstSharedText PulsarLSP::Library::FindOrLoadDocument(const lsp::FileUri& uri, int version)
{
    auto doc = FindDocument(uri);
    return doc ? doc : LoadDocument(uri, version);
}

PulsarLSP::ConstSharedText PulsarLSP::Library::LoadDocument(const lsp::FileUri& uri, int version)
{
    Pulsar::String buffer;
    if (!ReadFile(uri, buffer)) return nullptr;
    return StoreDocument(uri, buffer, version);
}

PulsarLSP::ConstSharedText PulsarLSP::Library::StoreDocument(const lsp::FileUri& uri, const Pulsar::String& text, int version)
{
    Pulsar::String path = URIToNormalizedPath(uri);
    auto cachedDoc = m_Documents.Find(path);
    if (cachedDoc) {
        if (version >= 0) cachedDoc->Value()->Version = version;
        return cachedDoc->Value()->Text = SharedText::New(text);
    } else {
        auto newlyCachedDocument = CachedDocument::SharedRef::New(CachedDocument{
            .URI = uri,
            .Text = SharedText::New(text),
            .Version = (version >= 0 ? version : 0)
        });
        m_Documents.Insert(std::move(path), newlyCachedDocument);
        return newlyCachedDocument->Text;
    }
}

PulsarLSP::ConstSharedText PulsarLSP::Library::PatchDocument(const lsp::FileUri& uri, const DocumentPatches& patches, int newVersion)
{
    Pulsar::String docPath = URIToNormalizedPath(uri);
    auto docPair = m_Documents.Find(docPath);
    if (!docPair) return nullptr;

    auto doc = docPair->Value();
    if (newVersion >= 0) {
        doc->Version = newVersion;
    }

    for (size_t i = 0; i < patches.size(); ++i) {
        const auto& patch = patches[i];
        if (std::holds_alternative<lsp::TextDocumentContentChangeEvent_Text>(patch)) {
            *doc->Text = std::get<lsp::TextDocumentContentChangeEvent_Text>(patch).text.c_str();
        } else if (std::holds_alternative<lsp::TextDocumentContentChangeEvent_Range_Text>(patch)) {
            PutRangePatch(*doc->Text, std::get<lsp::TextDocumentContentChangeEvent_Range_Text>(patch));
        }
    }

    return doc->Text;
}


void PulsarLSP::Library::DeleteDocument(const lsp::FileUri& uri)
{
    m_Documents.Remove(URIToNormalizedPath(uri));
}

void PulsarLSP::Library::DeleteAllDocuments()
{
    m_Documents.Clear();
}

std::pair<size_t, size_t> PulsarLSP::Library::GetRangeIndices(const Pulsar::String& text, lsp::Range range) const
{
    Pulsar::SourcePosition rangeStart = PositionToSourcePosition(range.start);
    Pulsar::SourcePosition rangeEnd   = PositionToSourcePosition(range.end);

    Pulsar::SourceViewer sourceViewer(text);
    sourceViewer.ConvertPositionTo(
            rangeStart, ToPulsarPositionEncoding(GetPositionEncoding()),
            rangeStart, ToPulsarPositionEncoding(GetPositionEncoding()),
            false);
    sourceViewer.ConvertPositionTo(
            rangeEnd, ToPulsarPositionEncoding(GetPositionEncoding()),
            rangeEnd, ToPulsarPositionEncoding(GetPositionEncoding()),
            false);
    return { rangeStart.Index, rangeEnd.Index };
}

void PulsarLSP::Library::PutRangePatch(Pulsar::String& text, const lsp::TextDocumentContentChangeEvent_Range_Text& change)
{
    if (text.Length() == 0) {
        text = change.text.c_str();
        return;
    }

    auto indices = GetRangeIndices(text, change.range);
    size_t p = indices.first;
    size_t q = indices.second;

    Pulsar::String back = text.SubString(q, text.Length());
    text.Resize(p);
    text += change.text.c_str();
    text += back;
}
