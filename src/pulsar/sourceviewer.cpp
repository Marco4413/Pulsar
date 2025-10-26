#include "pulsar/sourceviewer.h"

#include "pulsar/unicode.h"
#include "pulsar/utf8.h"

Pulsar::StringView Pulsar::SourceViewer::ComputeLineView(SourcePosition pos) const
{
    size_t lineStartIndex = pos.Index;
    while (lineStartIndex > 0 && m_Source[lineStartIndex-1] != '\n') {
        --lineStartIndex;
    }

    size_t lineEndIndex = pos.Index;
    while (lineEndIndex < m_Source.Length() && m_Source[lineEndIndex] != '\n') {
        ++lineEndIndex;
    }

    StringView lineView = m_Source;
    lineView.RemovePrefix(lineStartIndex);
    lineView.SetLength(lineEndIndex-lineStartIndex);
    return lineView;
}

Pulsar::SourceViewer::RangeView Pulsar::SourceViewer::ComputeRangeView(SourcePosition pos, Range range) const
{
    StringView lineView = ComputeLineView(pos);
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
