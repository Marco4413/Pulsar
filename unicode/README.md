# I Hate Unicode

Plenty of Unicode characters are defined as "Full Width",
which means they take up 2 "Narrow" characters on the terminal.

The width also depends on Locale ("Ambiguous" width characters.
yup, they put "Ambiguous" on an official spec document) but we
don't care about those.

The `tables.lua` script will be able to parse the following tables:
- https://unicode.org/Public/16.0.0/ucd/extracted/DerivedGeneralCategory.txt
- https://unicode.org/Public/16.0.0/ucd/EastAsianWidth.txt
- https://unicode.org/Public/emoji/16.0/emoji-sequences.txt

And generate C++ structures which can be copy-pasted into
`src/pulsar/unicode.cpp` to keep it up-to-date with the Unicode spec.
Moreover, those generated structures must support a binary search.

**TODO: Find the 0-width Unicode characters table if any and support it.**
