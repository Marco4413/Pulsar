// MIT Copyright (c) 2025 [Marco4413](https://github.com/Marco4413/Pulsar)

// We got UTF8 support in Pulsar before its Lexer added support for it.
// - Marco4413 25/01/23

/**
 * Creates a new Decoder from a String.
 * String -> Decoder
 */
*(utf8/decoder/new 1) -> 1:
  0 (!pack 2)
  .

/**
 * Destroys a previously created Decoder returning the String it was build with.
 * Decoder -> String
 */
*(utf8/decoder/destroy 1) -> 1:
  (!unpack 2) (!pop)
  .

/**
 * Advances the Decoder by one codepoint returning it.
 * Decoder -> Decoder, Integer (codepoint, 0 on error)
 */
*(utf8/decoder/next 1) -> 2:
  (!unpack 2) (!dup) local data cursor init-cursor:
    <- data
      (!length) cursor - -> remaining-data
      remaining-data if <= 0: init-cursor (!pack 2) 0 .
      cursor (!index) -> byte1
    -> data

    cursor 1 + -> cursor
    byte1 0x80 & if not:
      [ <- data, cursor ] byte1 .

    remaining-data if < 2: [ <- data, init-cursor ] 0 .
    byte1 0xE0 & if 0xC0:
      <- data
        cursor     (!index) -> byte2
      cursor 1 + (!pack 2)

      byte1 0x1F &  6 <<
      byte2 0x3F &       |
      .

    remaining-data if < 3: [ <- data, init-cursor ] 0 .
    byte1 0xF0 & if 0xE0:
      <- data
        cursor     (!index) -> byte2
        cursor 1 + (!index) -> byte3
      cursor 2 + (!pack 2)

      byte1 0x0F & 12 <<
      byte2 0x3F &  6 << |
      byte3 0x3F &       |
      .

    remaining-data if < 4: [ <- data, init-cursor ] 0 .
    byte1 0xF8 & if 0xF0:
      <- data
        cursor     (!index) -> byte2
        cursor 1 + (!index) -> byte3
        cursor 2 + (!index) -> byte4
      cursor 3 + (!pack 2)

      byte1 0x07 & 18 <<
      byte2 0x3F & 12 << |
      byte3 0x3F &  6 << |
      byte4 0x3F &       |
      .

    [ <- data, init-cursor ] 0
  end
  .

/**
 * Advances the Decoder by one codepoint returning its encoded byte length.
 * Decoder -> Decoder, Integer (skipped, 0 on error)
 */
*(utf8/decoder/skip 1) -> 2:
  (!unpack 2) (!dup) local data cursor init-cursor:
    <- data
      (!length) cursor - -> remaining-data
      remaining-data if <= 0: cursor (!pack 2) 0 .
      cursor (!index) -> byte1
    -> data

    cursor 1 + -> cursor
    byte1 0x80 & if not:
      [ <- data, cursor ] 1 .

    remaining-data if < 2: [ <- data, init-cursor ] 0 .
    byte1 0xE0 & if 0xC0:
      <- data cursor 1 + (!pack 2) 2 .

    remaining-data if < 3: [ <- data, init-cursor ] 0 .
    byte1 0xF0 & if 0xE0:
      <- data cursor 2 + (!pack 2) 3 .

    remaining-data if < 4: [ <- data, init-cursor ] 0 .
    byte1 0xF8 & if 0xF0:
      <- data cursor 3 + (!pack 2) 4 .

    [ <- data, init-cursor ] 0
  end
  .

/**
 * Calculates the length in codepoints of the given utf8-encoded String.
 * String -> String, Integer (length of the utf8-encoded string)
 */
*(utf8/length 1) -> 2:
  0 -> len

  (utf8/decoder/new)
  while:
    (utf8/decoder/skip) if not: break
    len 1 + -> len
  end
  (utf8/decoder/destroy)

  len
  .
