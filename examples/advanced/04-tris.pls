#include "01-printf.pls"

// The game does not need to buffer the output
global 0 -> printf/print-to-buffer?

global const ' ' -> tris/player/NONE
global const 'X' -> tris/player/X
global const 'O' -> tris/player/O

global tris/player/X -> tris/player/turn
global [
  tris/player/NONE, tris/player/NONE, tris/player/NONE,
  tris/player/NONE, tris/player/NONE, tris/player/NONE,
  tris/player/NONE, tris/player/NONE, tris/player/NONE,
] -> tris/board

*(*stdin/read) -> 1.
*(*stdout/write! str).

*(list/set-index list idx val) -> 1:
  idx if 0:
    <- list (!tail) <- val (!prepend)
    .
  <- list (!head) -> head
    idx 1 -
    <- val
    (list/set-index)
    <- head (!prepend)
  .

*(char/to-upper ch) -> 1:
  ch if < 'a': ch .
  ch if > 'z': ch .
  ch 'a' - 'A' +
  .

*(tris/row? ch) -> 1:
  ch if < 'A': 0 .
  ch if > 'C': 0 .
  1
  .

*(tris/col? ch) -> 1:
  ch if < '1': 0 .
  ch if > '3': 0 .
  1
  .

*(tris/str->idx s) -> 1: // String -> Integer
  <- s
    // The index must be a 2 char String
    (!length) if < 2: -1 .
    0 (!index) (char/to-upper) // 'A' 'B' 'C'
      <-> row
      (tris/row?) if not: -1 .
      row 'A' - 3 *
    -> idx
    1 (!index) // '1' '2' '3'
      <-> col
      (tris/col?) if not: -1 .
      col '1' - idx +
  .

*(tris/input!):
  "It's %c's turn!\n" [ tris/player/turn ]
    (printf!)
  do:
    "Position: "
      (*stdout/write!)
    (*stdin/read)
      (tris/str->idx)
        (!dup) if not >= 0:
          continue
        (!dup) if >= 9:
          continue
      -> idx
    <- tris/board
      idx (!index) if != tris/player/NONE:
        -> tris/board
        continue
      idx tris/player/turn
        (list/set-index)
    -> tris/board
  end
  tris/player/turn if tris/player/X:
    tris/player/O -> tris/player/turn
  else:
    tris/player/X -> tris/player/turn
  end
  .

*(tris/draw!):
  // ANSI Codes to Clear and Set Cursor to (1,1)
  "\x1B;[2J" \ "\x1B;[1;1H"
    (*stdout/write!)
    "  1 2 3"
  \n"A %c|%c|%c"
  \n"  -+-+-"
  \n"B %c|%c|%c"
  \n"  -+-+-"
  \n"C %c|%c|%c"
  \n"\n"
    tris/board (printf!)
  .

*(tris/winner) -> 1:
  <- tris/board
    // Check for rows
    0 -> i while i < 9:
      i (!index) -> player
      if player != tris/player/NONE:
        i 1 + (!index) if player:
          i 2 + (!index) if player:
            -> tris/board
            player
            .
        end
      end
      i 3 + -> i
    end
    // Check for cols
    0 -> i while i < 3:
      i (!index) -> player
      if player != tris/player/NONE:
        i 3 + (!index) if player:
          i 6 + (!index) if player:
            -> tris/board
            player
            .
        end
      end
      i 1 + -> i
    end
    // Check for diags
    0 (!index) -> player
    if player != tris/player/NONE:
      4 (!index) if player:
        8 (!index) if player:
          -> tris/board
          player
          .
      end
    end
    2 (!index) -> player
    if player != tris/player/NONE:
      4 (!index) if player:
        6 (!index) if player:
          -> tris/board
          player
          .
      end
    end
  -> tris/board
  tris/player/NONE
  .

*(tris/tie?) -> 1:
  tris/board while:
    (!empty?) if: 1 .
    (!head) if tris/player/NONE:
      break
  end
  0
  .

*(main args):
    "Welcome to Tris!"
  \n""
  \n"Positions are a pair of a letter and a number."
  \n"The letter represents the row and the number the column."
  \n"i.e. A1, A2, A3"
  \n"     B1, B2, B3"
  \n"     C1, C2, C3"
  \n""
  \n"Press ENTER when you're ready!"
  \n"" (*stdout/write!)
  (*stdin/read) (!pop)

  (tris/draw!)
  while:
    (tris/input!)
    (tris/draw!)
    (tris/winner) -> winner
    winner if not tris/player/NONE:
      "%c Won!\n" [ winner ] (printf!)
      break
    (tris/tie?) if:
      "It's a Tie.\n" (*stdout/write!)
      break
  end
  .
