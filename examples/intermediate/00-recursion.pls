*(__fib x) -> 2:
  x if <= 1: x 1 -; x .
  x 1 - (__fib)
    // Stack [fib(x-2), fib(x-1)]
    <-> fib_x-1 +
      fib_x-1
      (!swap)
  .

*(fib x) -> 1: x (__fib).

*(fib-list x) -> 1:
  x if <= 1:
    x; x 1 -
      (!pack 2)
    .
  x 1 - (fib-list)
  (!head)  -> fib_x-1
  (!head) <-> fib_x-2
    (!prepend)
  fib_x-1 (!prepend)
  <- fib_x-2 <- fib_x-1 +
    (!prepend)
  .

*(fact x) -> 1:
  x if <= 1: 1.
  x 1 - (fact)
    x *
  .

*(main args) -> 3:
  // All numbers should(tm) fit within a 64 bit signed integer.
  92 (fib-list)
  92 (fib)
  23 (fact)
  .
