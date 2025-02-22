#include "01-printf.pls"
#include "../intermediate/05-random.pls"

// printf is used only as a frame buffer
//  and to easily generate snake/window/frame
global 1 -> printf/print-to-buffer?

// Positions start from 1 because of the
//  standard used by ANSI Escape Codes
global [[1,1]] -> snake
global  [1,0]  -> snake/direction
global  [1,0]  -> snake/next-direction
global       0 -> snake/game-over?
global  "None" -> snake/game-over-reason
global       2 -> snake/growth
global [10, 4] -> snake/fruit
// How many times we should try to regen the fruit
//  if it overlaps the snake
global       4 -> snake/fruit/regen-attempts

// Game Settings
global const         1 -> snake/wrap-edges?
global const         1 -> snake/show-update-budget?
global const       300 -> snake/update-time
global const  [ 2, 2 ] -> snake/world/origin
global const        12 -> snake/world/width
global const         6 -> snake/world/height
global const       'W' -> snake/key/up
global const       'A' -> snake/key/left
global const       'S' -> snake/key/down
global const       'D' -> snake/key/right

global const -> snake/ui-position:
  snake/world/origin 1 (!index)
    snake/world/height +
    3 +
  -> y
  [ 1, y ]
  .

global const -> snake/window/frame:
  snake/world/origin (!head) 1 - -> pad-left 
  "%.*s+%.*s+\n" [
    pad-left, " ",
    snake/world/width, "-"
  ] (printf!)
  0 -> i while i < snake/world/height:
    "%.*s|%.*s|\n" [
      pad-left, " ",
      snake/world/width, " "
    ] (printf!)
    i 1 + -> i
  end
  "%.*s+%.*s+\n" [
    pad-left, " ",
    snake/world/width, "-"
  ] (printf!)
  <- printf/buffer
  .

*(*stdin/read) -> 1.

*(*time/micros) -> 1.
*(*time/steady) -> 1.

*(*this-thread/sleep! delay).

// Multi-threaded Snake?
// (*stdin/read) blocks
// So input and game update must be on different threads
*(*thread/run      args fn) -> 1.
*(*thread/join     thread ) -> 2.

*(*channel/new) -> 1.
*(*channel/send!   value channel).
*(*channel/receive channel) -> 1.
*(*channel/close!  channel).
*(*channel/empty?  channel) -> 1.
*(*channel/closed? channel) -> 1.

*(char/to-upper ch) -> 1:
  ch if < 'a': ch .
  ch if > 'z': ch .
  ch 'a' - 'A' +
  .

*(math/abs x) -> 1:
  x if < 0: x -1 * .
  x
  .

*(list/remove-back l) -> 1:
  <- l
    (!empty?) if: .
    (!head) -> item
    (!empty?) if: .
    (list/remove-back)
    <- item
    (!prepend)
  .

*(vec2/pack 2) -> 1: (!pack 2) .

*(vec2/unpack 1) -> 2: (!unpack 2) .

*(vec2/add v1 v2) -> 1:
  <- v1 (vec2/unpack)
  <- v2 (vec2/unpack)
    local x1 y1 x2 y2:
      x1 x2 +
      y1 y2 +
        (vec2/pack)
    end
  .

*(vec2/opposite v) -> 1:
  <- v (vec2/unpack) local x y:
    x -1 *
    y -1 *
      (vec2/pack)
  end
  .

*(vec2/equals? v1 v2) -> 1:
  <- v1 (vec2/unpack)
  <- v2 (vec2/unpack)
    local x1 y1 x2 y2:
      x1 if x2: y1 if y2: 1 . end
      0
    end
  .

*(ansi/set-cursor! x y):
  "\x1B;[%d;%dH" [ y, x ]
    (printf!)
  .

*(ansi/clear-all!):
  "\x1B;[2J"
    (printf/print!)
  .

*(snake/draw!):
  snake/world/origin
    (!head) (!pop) // x
    (!head) -> pad-top
  1 pad-top (ansi/set-cursor!)
  snake/window/frame
    (printf/print!)

  snake
    (!head) -> snake-head
    while:
      (!empty?) if: break
      (!head) snake/world/origin (vec2/add)
        (vec2/unpack)
        (ansi/set-cursor!)
      "*" (printf/print!)
    end
  
  snake/fruit snake/world/origin (vec2/add)
    (vec2/unpack)
    (ansi/set-cursor!)
  "F" (printf/print!)
  
  snake-head snake/world/origin (vec2/add)
    (vec2/unpack)
    (ansi/set-cursor!)
  "O" (printf/print!)

  snake/ui-position (vec2/unpack)
    (ansi/set-cursor!)
  .

*(snake/out-of-bounds? v) -> 1:
  <- v (vec2/unpack) local x y:
    x if <= 0: 1 .
    y if <= 0: 1 .
    x if > snake/world/width: 1 .
    y if > snake/world/height: 1 .
    0
  end
  .

*(snake/wrap v) -> 1:
  <- v (vec2/unpack) local x y:
    if x <= 0: snake/world/width  else if x > snake/world/width:  1 else: x end
    if y <= 0: snake/world/height else if y > snake/world/height: 1 else: y end
      (vec2/pack)
  end
  .

*(snake/on-top-of-snake? v) -> 1:
  snake while:
    (!empty?) if: break
    (!head) v (vec2/equals?) if: 1 .
  end
  0
  .

*(snake/fruit/regen!):
  0 -> i while:
    (random/int) (math/abs)
      snake/world/width %
      1 +
    (random/int) (math/abs)
      snake/world/height %
      1 +
    (vec2/pack) -> snake/fruit
    i if >= snake/fruit/regen-attempts: break
    snake/fruit (snake/on-top-of-snake?) if not: break
    i 1 + -> i
  end
  .

*(snake/update!):
  snake/game-over? if: .

  <- snake/next-direction (!void?) if not:
    -> snake/direction
  end

  <- snake 0 (!index) -> snake-head -> snake
  
  snake-head snake/fruit (vec2/equals?) if:
    (snake/fruit/regen!)
    snake/growth 1 + -> snake/growth
  end

  if snake/growth <= 0:
    <- snake
      (list/remove-back)
    -> snake
  else:
    snake/growth 1 - -> snake/growth
  end

  snake-head snake/direction (vec2/add)
    snake/wrap-edges? if:
      (snake/wrap)
    else:
      (!dup) (snake/out-of-bounds?) if:
        "Wall collision." -> snake/game-over-reason
        1 -> snake/game-over? .
    end
    (!dup) (snake/on-top-of-snake?) if:
      "Self collision." -> snake/game-over-reason
      1 -> snake/game-over? .
    <- snake (!swap) (!prepend) -> snake
  .

*(snake/input-thread! ch):
  while:
    ch (*channel/closed?) if:
      break
    (*stdin/read)
      (!empty?) if: (!pop) continue
      (!length) 1 - (!index) (char/to-upper) -> dir
    if dir = snake/key/up:
      [  0, -1 ] ch (*channel/send!)
    else if dir = snake/key/left:
      [ -1,  0 ] ch (*channel/send!)
    else if dir = snake/key/down:
      [  0,  1 ] ch (*channel/send!)
    else if dir = snake/key/right:
      [  1,  0 ] ch (*channel/send!)
    end
  end
  .

*(main args):
    "Welcome to Snake!"
  \n""
  \n"Since this snake runs on your terminal,"
  \n"keys must be typed and committed with ENTER."
  \n""
  \n"Keys:"
  \n"  %c: Up"
  \n"  %c: Down"
  \n"  %c: Left"
  \n"  %c: Right"
  \n""
  \n"If running the Pulsar source file, you can edit it"
  \n"to change game settings (it's all at the top)."
  \n""
  \n"Press ENTER when you're ready!"
  \n"" [
    snake/key/up,
    snake/key/down,
    snake/key/left,
    snake/key/right,
  ] (printf!) (printf/flush-buffer!)
  (*stdin/read) (!pop)

  (*time/micros) -> random/seed

  (*channel/new) -> inp-ch
  [ inp-ch ] <& (snake/input-thread!)
    (*thread/run) -> inp-thread

  while not snake/game-over?:
    snake/update-time
    (*time/steady)
      do:
        inp-ch (*channel/empty?) if not:
          inp-ch (*channel/receive) -> new-direction

          snake/direction (vec2/opposite)
            new-direction (vec2/equals?) if not:
              new-direction -> snake/next-direction
            end
          continue
      end

      (snake/update!)
      (ansi/clear-all!)
      (snake/draw!)
      (printf/flush-buffer!)
    (*time/steady)
      (!swap) - <-> time - // snake/update-time - TIME
    snake/show-update-budget? if:
      "Update Budget: %d/%dms\n" [ time, snake/update-time ] (printf!)
        (printf/flush-buffer!)
    end
      (*this-thread/sleep!)
  end

    "Game Over!"
  \n"Reason: %s"
  \n""
  \n"Press ENTER to quit."
  \n"" [
    snake/game-over-reason
  ] (printf!) (printf/flush-buffer!)

  inp-ch (*channel/close!)
  <- inp-ch (!pop)
  <- inp-thread (*thread/join) (!pop 2)
  .
