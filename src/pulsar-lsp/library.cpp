#include <pulsar-lsp/library.h>

#include <filesystem>
#include <fstream>

#include "pulsar/utf8.h"

Pulsar::String PulsarLSP::URIToNormalizedPath(const lsp::FileUri& uri)
{
    auto rawPath = std::filesystem::path(uri.path());

    std::error_code error;
    auto normalizedPath = std::filesystem::relative(rawPath, error);

    if (error || normalizedPath.empty()) {
        normalizedPath = std::filesystem::canonical(rawPath, error);
    }

    Pulsar::String internalPath = !(error || normalizedPath.empty())
        ? normalizedPath.generic_string().c_str()
        : rawPath.generic_string().c_str();

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
    switch (encodingKind) {
    case PositionEncodingKind::UTF8:
        return Pulsar::UTF8::GetEncodedSize(code);
    case PositionEncodingKind::UTF16:
        if (code <= 0xFFFF) return 1;
        return 2;
    case PositionEncodingKind::UTF32:
    default:
        return 1;
    }
}

void PulsarLSP::UTF8::DecoderExt::AdvanceToLine(Decoder& decoder, size_t startLine, size_t line)
{
    size_t currentLine = startLine;
    while (decoder && currentLine != line) {
        Codepoint ch = decoder.Next();
        // CRLF
        if (ch == '\r' && decoder.Peek() == '\n') {
            ++currentLine;
            decoder.Skip();
        // CR or LF
        } else if (ch == '\r' || ch == '\n') {
            ++currentLine;
        }
    }
}

void PulsarLSP::UTF8::DecoderExt::AdvanceToChar(Decoder& decoder, size_t startChar, size_t character, PositionEncodingKind encodingKind)
{
    size_t currCharacter = startChar;
    while (decoder && currCharacter < character) {
        Codepoint ch = decoder.Next();
        currCharacter += Unicode::GetEncodedSize(ch, encodingKind);

        if (ch == '\r' && decoder.Peek() == '\n') {
            decoder.Skip();
            break;
        } else if (ch == '\r' || ch == '\n') {
            break;
        }
    }
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
    UTF8::Decoder start(text);
    UTF8::DecoderExt::AdvanceToLine(start, 0, (size_t)range.start.line);

    UTF8::Decoder end = start;
    UTF8::DecoderExt::AdvanceToLine(end, (size_t)range.start.line, (size_t)range.end.line);

    UTF8::DecoderExt::AdvanceToChar(start, 0, (size_t)range.start.character, GetPositionEncoding());
    UTF8::DecoderExt::AdvanceToChar(end, 0, (size_t)range.end.character, GetPositionEncoding());

    return { start.GetDecodedBytes(), end.GetDecodedBytes() };
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
