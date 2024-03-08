*(fib n) -> 1:
  [1, 0]
  n if <= 0: (!tail).
  n if 1: .
  while n > 1:
    0 (!index) -> fib_n-1
    1 (!index) <- fib_n-1
      + (!prepend)
    n 1 - -> n
  end
  .

*(main args) -> 1:
  92 (fib)
  .
