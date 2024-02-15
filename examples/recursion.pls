*(swap x y) -> 2: y x.
*(__fib x) -> 2:
    x if <= 1: x 1 -; x.
    x 1 - (__fib)
    // Stack [fib(x-2), fib(x-1)]
    -> fib_x-1; fib_x-1
    +; fib_x-1; (swap)
    .

*(fib x) -> 1: x (__fib).

*(fact x) -> 1:
    x if <= 1: 1.
    x 1 - (fact)
    x *
    .

*(main) -> 2:
    // Both numbers should(tm) fit within a 64 bit signed integer.
    92 (fib)
    23 (fact)
    .
