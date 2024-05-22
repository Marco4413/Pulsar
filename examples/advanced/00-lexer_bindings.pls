// Native Bindings
*(*lexer/from-file   path) -> 1.
*(*lexer/next-token lexer) -> 1.
*(*lexer/valid?     lexer) -> 1.

global const 1 -> TOKEN/EOF

*(lex-next lexer) -> 1: // [ Token ]
  lexer (*lexer/next-token)
  (!head) if TOKEN/EOF:
    -> token
    [ <- token ]
    .
  -> token
  lexer (lex-next)
    <- token (!prepend)
  .

*(lex-file filepath) -> 1:
  filepath (*lexer/from-file) -> lexer
  lexer (*lexer/valid?) if:
    lexer (lex-next)
    .
  "Could not read file at '"
    <- filepath (!append)
    "'." (!append)
  .

*(main args) -> 1:
  <- args
    (!head) -> program
    // Parse the program itself if none provided
    (!length) if > 0: (!head) else: program end
    (lex-file)
  .
