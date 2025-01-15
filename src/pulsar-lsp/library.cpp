#include <pulsar-lsp/library.h>

#include <filesystem>
#include <fstream>

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

bool PulsarLSP::ReadFile(const lsp::FileURI& uri, std::string& buffer)
{
    Pulsar::String path  = URIToNormalizedPath(uri);
    std::filesystem::path fsPath = path.Data();
    if (!std::filesystem::exists(fsPath)) return false;

    std::ifstream file(fsPath, std::ios::binary);
    size_t fileSize = (size_t)std::filesystem::file_size(fsPath);

    buffer.resize(fileSize);
    return (bool)file.read(buffer.data(), fileSize);
}

bool PulsarLSP::ReadFile(const lsp::FileURI& uri, Pulsar::String& buffer)
{
    Pulsar::String path  = URIToNormalizedPath(uri);
    std::filesystem::path fsPath = path.Data();
    if (!std::filesystem::exists(fsPath)) return false;

    std::ifstream file(fsPath, std::ios::binary);
    size_t fileSize = (size_t)std::filesystem::file_size(fsPath);

    buffer.Resize(fileSize);
    return (bool)file.read((char*)buffer.Data(), fileSize);
}

PulsarLSP::ConstSharedText PulsarLSP::Library::FindDocument(const lsp::FileURI& uri) const
{
    Pulsar::String path = URIToNormalizedPath(uri);
    auto doc = m_Documents.Find(path);
    return doc ? doc->Value()->Text : nullptr;
}

PulsarLSP::ConstSharedText PulsarLSP::Library::FindOrLoadDocument(const lsp::FileURI& uri, int version)
{
    auto doc = FindDocument(uri);
    return doc ? doc : LoadDocument(uri, version);
}

PulsarLSP::ConstSharedText PulsarLSP::Library::LoadDocument(const lsp::FileURI& uri, int version)
{
    Pulsar::String buffer;
    if (!ReadFile(uri, buffer)) return nullptr;
    return StoreDocument(uri, buffer, version);
}

PulsarLSP::ConstSharedText PulsarLSP::Library::StoreDocument(const lsp::FileURI& uri, const Pulsar::String& text, int version)
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

size_t FindLineStartIndex(const Pulsar::String& text, size_t line)
{
    size_t currentLine = 0;

    size_t p;
    for (p = 0; p < text.Length() && currentLine != line; ++p) {
        // CRLF
        if (text.Length()-p >= 2 && text[p] == '\r' && text[p+1] == '\n') {
            ++currentLine;
            ++p; // +1 because CRLF is a sequence of 2 bytes
        // CR or LF
        } else if (text[p] == '\r' || text[p] == '\n') {
            ++currentLine;
        }
    }

    return p;
}

size_t FindCharacterIndex(const Pulsar::String& text, size_t p, size_t character)
{
    for (size_t currCharacter = 0; currCharacter < character; ++currCharacter) {
        if (p >= text.Length()) break;
        // TODO: Support UTF-8 encoding
        //       Some fixes should also be made to Pulsar::Lexer
        // CRLF
        if (text.Length()-p >= 2 && text[p] == '\r' && text[p+1] == '\n') {
            p += 2;
            break;
        // CR or LF
        } else if (text[p] == '\r' || text[p] == '\n') {
            p += 1;
            break;
        }

        ++p;
    }

    return p;
}

std::pair<size_t, size_t> GetRangeIndices(const Pulsar::String& text, lsp::Range range)
{
    size_t p = FindLineStartIndex(text, (size_t)range.start.line);
    size_t q = FindLineStartIndex(text, (size_t)range.end.line);

    p = FindCharacterIndex(text, p, (size_t)range.start.character);
    q = FindCharacterIndex(text, q, (size_t)range.end.character);

    // They're probably already sorted
    if (p <= q) {
        return {p, q};
    } else {
        return {q, p};
    }
}

void PutRangePatch(Pulsar::String& text, const lsp::TextDocumentContentChangeEvent_Range_Text& change)
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

PulsarLSP::ConstSharedText PulsarLSP::Library::PatchDocument(const lsp::FileURI& uri, const DocumentPatches& patches, int newVersion)
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


void PulsarLSP::Library::DeleteDocument(const lsp::FileURI& uri)
{
    m_Documents.Remove(URIToNormalizedPath(uri));
}

void PulsarLSP::Library::DeleteAllDocuments()
{
    m_Documents.Clear();
}
