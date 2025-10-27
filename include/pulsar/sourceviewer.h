#ifndef _PULSAR_SOURCEVIEWER_H
#define _PULSAR_SOURCEVIEWER_H

#include "pulsar/core.h"
#include "pulsar/lexer.h"
#include "pulsar/structures/stringview.h"

namespace Pulsar
{
    /**
     * Class which helps with showing source code to the user.
     * It will also include utilities from Pulsar-LSP to convert between UTF-8/16/32 codepoint counts.
     */
    class SourceViewer
    {
    public:
        enum class PositionEncoding
        {
            UTF8, UTF16, UTF32
        };

        struct Range
        {
            size_t Before;
            size_t After;
        };

        static constexpr Range Range_Infinite{~size_t(0),~size_t(0)};

        struct RangeView
        {
            // StringView of the given range.
            StringView View;
            // Bytes from View.Data() to the start of
            //  the SourcePosition that generated this view.
            size_t BytesToTokenStart;
            // Unicode::Width() to the start of
            //  the SourcePosition that generated this view.
            size_t WidthToTokenStart;
            // Unicode::Width() of the SourcePosition that generated this view.
            size_t TokenWidth;
            // True if contents of line start were trimmed
            //  from View due to being out of range.
            bool HasTrimmedBefore;
            // True if contents of line end were trimmed
            //  from View due to being out of range.
            bool HasTrimmedAfter;
        };

    public:
        static size_t GetEncodedSize(Unicode::Codepoint codepoint, PositionEncoding encoding);

    public:
        // The given source must outlive any View returned by the methods of this class.
        SourceViewer(StringView source)
            : m_Source(source) {}

        // If `pos.Index` is valid, consider enabling `usePositionIndex` to speed up line search.
        // `pos.Index` allows for jumping directly to the line so only bounds are computed.
        // The returned `StringView` is contained between `source.Data()` and `source.Data()+source.Length()`.
        StringView ComputeLineView(
                SourcePosition pos,
                bool usePositionIndex=true
            ) const;
        // If `false` is returned, `inPos` was ill-formed and `outPos` represents an approximation of the correct position.
        // Converts to a position represented as `PositionEncoding` from another `PositionEncoding`.
        // `outPos.Index` is filled with a valid index if `true` is returned.
        // The default encoding of `SourcePosition` is `::UTF32`.
        bool ConvertPositionTo(
                SourcePosition& outPos, PositionEncoding outEncoding,
                SourcePosition  inPos,  PositionEncoding inEncoding=PositionEncoding::UTF32,
                bool usePositionIndex=true
            ) const;
        // Computes a `RangeView` which shows the token at pos with at most `range.Before`
        //  chars before the token and at most `range.After` chars after the end of the token.
        RangeView ComputeRangeView(
                SourcePosition pos, Range range=Range_Infinite,
                bool usePositionIndex=true
            ) const;

    private:
        StringView m_Source;
    };
}

#endif // _PULSAR_SOURCEVIEWER_H
