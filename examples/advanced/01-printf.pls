*(*print! val).

global [] -> printf/context-stack
global 2 -> printf/default-precision
global 0 -> printf/print-to-buffer?
global "" -> printf/buffer

// Functions meant for public use:
// (printf/save-ctx!)
// (printf/restore-ctx!)
// (printf/flush-buffer!)
// (printf fmt args) -> 1
// (printf! fmt args)
// (printf-box fmt args) -> 1
// (printf-box! fmt args)

*(printf/save-ctx!):
  <- printf/context-stack
    [ printf/default-precision, printf/print-to-buffer? ]
    <- printf/buffer (!append)
    "" -> printf/buffer
    (!prepend)
  -> printf/context-stack
  .

*(printf/restore-ctx!):
  <- printf/context-stack
    (!head)
    (!head) -> printf/default-precision
    (!head) -> printf/print-to-buffer?
    (!head) -> printf/buffer
    (!pop)
  -> printf/context-stack
  .

*(printf/flush-buffer!):
  <- printf/buffer (*print!)
  "" -> printf/buffer
  .

*(printf/print! val):
  printf/print-to-buffer? if:
    <- printf/buffer <- val (!append)
    -> printf/buffer
  else:
    <- val (*print!)
  end
  .

*(printf/index-of str ch) -> 2:
  <- str (!length) -> str-len
    0 -> i while i < str-len:
      i (!index) if ch: i .
      i 1 + -> i
    end
    (!length)
  .

*(printf/itos n) -> 1:
  n if 0: "0" .
  0 -> negative?
  n if < 0:
    1 -> negative?
    n -1 * -> n
  end
  "" while n > 0:
    n 10 % '0' + (!prepend)
    n 10 / -> n
  end
  negative? if: '-' (!prepend) end
  .

*(printf/int-pow base exp) -> 1:
  exp if 0: 1 .
  0 -> inv
  exp if < 0:
    1 -> inv
    exp -1 * -> exp
  end
  base while exp > 1:
    base *
    exp 1 - -> exp
  end
  inv if:
    1.0 (!swap) /
  end
  .

*(printf/str-rep str n) -> 1:
  n if 0: "" .
  str
    1 -> i while i < n:
      str (!append)
      i 1 + -> i
    end
  .

*(printf/dtos n prec) -> 1:
  n if 0: "0.0" .
  n (!floor) (printf/itos)
  "." (!append)
  n n (!floor) -
    (!dup) if 0:
      (!pop)
      "0" prec (printf/str-rep)
        (!append)
      .
    (!dup) if < 0:
      -1 *
    end
    10 prec (printf/int-pow) *
    (!floor) (printf/itos)
    (!append)
  .

// Printf which returns args that are not consumed
// Use (printf!) if you don't care about this feature
*(printf fmt args) -> 1:
  <- args <- fmt
    do: '%' (printf/index-of) (!prefix) (printf/print!)
      (!length) if 0: break
      1 (!index) -> ch
      2 (!prefix) (!pop)
      (!swap) // [ fmt, args ]
      if ch = 's':
        (!head) (printf/print!)
      else if ch = 'c':
        (!head) 0 - // Type checking! Chars are Integers and - only works on numbers
          "" (!swap) (!append) (printf/print!)
      else if ch = 'd':
        (!head) (printf/itos) (printf/print!)
      else if ch = 'f':
        (!head) printf/default-precision (printf/dtos) (printf/print!)
      else if ch = '.':
        (!swap) // [ args, fmt ]
        (!length) if < 2:
          "%." (printf/print!)
          continue
        2 (!prefix)
        1 (!index) -> var-type
        if var-type = 'f':
          0 (!index) -> precision
          (!pop)
          (!swap) // [ fmt, args ]
          (!head) precision '0' -
            (printf/dtos) (printf/print!)
        else if var-type = 's':
          0 (!index) -> rep-count
          (!pop)
          (!swap) // [ fmt, args ]
          if rep-count = '*':
            (!head) -> var-rep-count
            (!head) <- var-rep-count
              (printf/str-rep) (printf/print!)
          else:
            (!head) rep-count '0' -
              (printf/str-rep) (printf/print!)
          end
        else:
          // ABORT
          "%."
            (!swap) // [ "%." "...f" ]
            (!append) (printf/print!)
          continue
        end
      else:
        "%" ch (!append) (printf/print!)
      end
      (!swap) // [ args, fmt ]
      continue
    end
    (!pop) // Pops fmt
  .

*(printf! fmt args):
  <- fmt <- args (printf)
  .

// Printf-Box which returns args that are not consumed
// Use (printf-box!) if you don't care about this feature
*(printf-box fmt args) -> 1:
  [] -> lines
  0  -> lines-len
  0  -> max-line-len
  (printf/save-ctx!)
    1  -> printf/print-to-buffer?
    <- fmt do:
      '\n' (printf/index-of) (!prefix)
        <- args (printf) -> args
      <- lines <- printf/buffer
        (!length) if > max-line-len:
          (!length) -> max-line-len
        end
        (!append) -> lines
        lines-len 1 + -> lines-len
      "" -> printf/buffer
      (!length) if 0: break
      1 (!prefix) (!pop) // Remove '\n'
      continue
    end
  (printf/restore-ctx!)
  "+%.*s+\n" [ max-line-len, "-" ] (printf!)
  <- lines 0 -> i while i < lines-len:
    (!head)
      (!length) max-line-len (!swap) -
        -> padding-len
      [ padding-len, " " ]
        (!swap) (!prepend)
    "|%s%.*s|\n" (!swap) (printf!)
    i 1 + -> i
  end
  "+%.*s+" [ max-line-len, "-" ] (printf!)
  <- args
  .

*(printf-box! fmt args):
  <- fmt <- args (printf-box)
  .

*(main args):
  (printf/save-ctx!)
    // All printf/* globals are saved
    // (except for printf/context-stack)
    1 -> printf/print-to-buffer?
    4 -> printf/default-precision
    "%s, %s%c\n%.*sPI=%f\n" [
      "Hello", "World", '!',
      8, " ", 3.1415
    ] (printf!)
    "I'm %s in here!\n%.*sHelp me!" [
      "trapped",
      4, " "
    ] (printf-box!)
    "\n" (printf/print!)
    (printf/flush-buffer!)
  (printf/restore-ctx!)
  // All printf/* globals are restored
  .
