#include "pulsar/sourceviewer.h"

#include "pulsar/unicode.h"
#include "pulsar/utf8.h"

size_t Pulsar::SourceViewer::GetEncodedSize(Unicode::Codepoint codepoint, PositionEncoding encoding)
{
    switch (encoding) {
    case PositionEncoding::UTF8:
        return Unicode::GetUTF8EncodedSize(codepoint);
    case PositionEncoding::UTF16:
        return Unicode::GetUTF16EncodedSize(codepoint);
    case PositionEncoding::UTF32:
        return Unicode::GetUTF32EncodedSize(codepoint);
    default:
        return 1;
    }
}

inline bool IsCROrLF(char ch)
{
    return ch == '\r' || ch == '\n';
}

Pulsar::StringView Pulsar::SourceViewer::ComputeLineView(
        SourcePosition pos, bool usePositionIndex
    ) const
{
    size_t lineStartIndex = 0, lineEndIndex = 0;
    if (usePositionIndex) {
        if (pos.Index <= m_Source.Length()) {
            lineStartIndex = pos.Index;
            while (lineStartIndex > 0 && !IsCROrLF(m_Source[lineStartIndex-1])) {
                --lineStartIndex;
            }

            lineEndIndex = pos.Index;
            while (lineEndIndex < m_Source.Length() && !IsCROrLF(m_Source[lineEndIndex])) {
                ++lineEndIndex;
            }
        }
    } else {
        for (size_t line = pos.Line; line > 0; --line) {
            while (lineStartIndex < m_Source.Length() && !IsCROrLF(m_Source[lineStartIndex]))
                ++lineStartIndex;
            // At end of file, no new line to skip and no more lines to skip
            if (lineStartIndex >= m_Source.Length())
                break;
            // Skip CR or LF character
            ++lineStartIndex;
            // If char was CR and the next one is LF skip LF.
            // Pulsar::Lexer treats CR, LF and CRLF as new line sequences.
            if (m_Source[lineStartIndex-1] == '\r' && lineStartIndex < m_Source.Length() && m_Source[lineStartIndex] == '\n')
                ++lineStartIndex;
        }

        lineEndIndex = lineStartIndex;
        while (lineEndIndex < m_Source.Length() && !IsCROrLF(m_Source[lineEndIndex]))
            ++lineEndIndex;
    }

    StringView lineView = m_Source;
    lineView.RemovePrefix(lineStartIndex);
    lineView.SetLength(lineEndIndex-lineStartIndex);
    return lineView;
}

bool Pulsar::SourceViewer::ConvertPositionTo(
        SourcePosition& outPos, PositionEncoding outEncoding,
        SourcePosition  inPos,  PositionEncoding inEncoding,
        bool usePositionIndex
    ) const
{
    bool isOk = true;
    // Lines are always encoded the same, since source is UTF8 we can compute the view
    StringView lineView = ComputeLineView(inPos, usePositionIndex);
    UTF8::Decoder decoder(lineView);

    outPos = inPos;
    outPos.Char     = 0;
    outPos.CharSpan = 0;

    for (size_t inChar = inPos.Char; inChar > 0;) {
        Unicode::Codepoint codepoint = decoder.Next();

        size_t inEncodedSize = GetEncodedSize(codepoint, inEncoding);
        // This is an error
        if (inEncodedSize > inChar) {
            isOk = false;
            break;
        }

        size_t outEncodedSize = GetEncodedSize(codepoint, outEncoding);
        inChar      -= inEncodedSize;
        outPos.Char += outEncodedSize;
    }

    outPos.Index = decoder.GetDecodedBytes() + static_cast<size_t>(lineView.Data() - m_Source.Data());

    for (size_t inCharSpan = inPos.CharSpan; inCharSpan > 0;) {
        Unicode::Codepoint codepoint = decoder.Next();

        size_t inEncodedSize = GetEncodedSize(codepoint, inEncoding);
        // This is an error
        if (inEncodedSize > inCharSpan) {
            isOk = false;
            break;
        }

        size_t outEncodedSize = GetEncodedSize(codepoint, outEncoding);
        inCharSpan      -= inEncodedSize;
        outPos.CharSpan += outEncodedSize;
    }

    return isOk;
}

Pulsar::SourceViewer::RangeView Pulsar::SourceViewer::ComputeRangeView(
        SourcePosition pos, Range range,
        bool usePositionIndex
    ) const
{
    StringView lineView = ComputeLineView(pos, usePositionIndex);
    size_t lineLength = UTF8::Length(lineView);

    RangeView tokenView {
        .View              = lineView,
        .BytesToTokenStart = 0,
        .WidthToTokenStart = 0,
        .TokenWidth        = 0,
        .HasTrimmedBefore  = false,
        .HasTrimmedAfter   = true,
    };

    size_t viewStartCodepoint = 0;
    if (range.Before < pos.Char) {
        viewStartCodepoint = pos.Char-range.Before;
        tokenView.HasTrimmedBefore = true;
    }

    constexpr size_t MAX_SIZE_T = ~size_t(0);

    size_t viewEndCodepoint = pos.Char+pos.CharSpan;
    // Check for overflow. range.After may be set to a big number to indicate infinity
    if (range.After > MAX_SIZE_T - viewEndCodepoint) {
        viewEndCodepoint  = MAX_SIZE_T;
    } else {
        viewEndCodepoint += range.After;
    }
    if (viewEndCodepoint > lineLength) {
        viewEndCodepoint = lineLength;
        tokenView.HasTrimmedAfter = false;
    }

    UTF8::Decoder decoder(lineView);
    size_t codepointIndex = 0;
    for (; codepointIndex < viewStartCodepoint; ++codepointIndex)
        decoder.Skip();

    size_t bytesToViewStart = decoder.GetDecodedBytes();
    tokenView.View.RemovePrefix(bytesToViewStart);

    for (; codepointIndex < pos.Char; ++codepointIndex) {
        Unicode::Codepoint codepoint = decoder.Next();
        tokenView.WidthToTokenStart += Unicode::Width(codepoint);
    }

    tokenView.BytesToTokenStart = decoder.GetDecodedBytes()-bytesToViewStart;

    for (; codepointIndex < pos.Char+pos.CharSpan; ++codepointIndex) {
        Unicode::Codepoint codepoint = decoder.Next();
        tokenView.TokenWidth += Unicode::Width(codepoint);
    }

    for (; codepointIndex < viewEndCodepoint; ++codepointIndex)
        decoder.Skip();

    tokenView.View.SetLength(decoder.GetDecodedBytes()-bytesToViewStart);
    return tokenView;
}
