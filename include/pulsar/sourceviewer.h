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
        // The given source must outlive any object returned by this class.
        SourceViewer(StringView source)
            : m_Source(source) {}

        StringView ComputeLineView(SourcePosition pos) const;
        // Computes a RangeView which shows the token at pos with at most range.Before
        //  chars before the token and at most range.After chars after the end of the token.
        RangeView ComputeRangeView(SourcePosition pos, Range range=Range_Infinite) const;

    private:
        StringView m_Source;
    };
}

#endif // _PULSAR_SOURCEVIEWER_H
