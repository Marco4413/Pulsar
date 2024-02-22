// Native Bindings
*(*module/from-file   path) -> 1.
*(*module/run       handle) -> 2.
*(*module/free      handle).

*(*lexer/from-file    path) -> 1.
*(*lexer/next-token handle) -> 2.
*(*lexer/free       handle).

*(lex-next handle) -> 2: // [ Handle, Token ]
    handle
        (*lexer/next-token)
        (!head) if 1: // EndOfFile
            -> token
            (!empty-list)
            <- token
                (!append)
            .
        -> token
            (lex-next)
            <- token
            (!prepend)
    .

*(lex-file filepath) -> 1:
    <- filepath
    (*lexer/from-file)
        (lex-next) -> result
        (*lexer/free)
    <- result
    .

*(main args) -> 1:
    <- args
        (!head) -> program
        // Parse the program itself if none provided
        (!length) if > 0: (!head) else: program end
        (lex-file)
    .
