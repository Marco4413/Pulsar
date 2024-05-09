*(operands) -> 2: 0.2 3.

// sum's arguments are directly put onto the stack,
//   removing the overhead of moving/copying them from locals.
*(sum 2)   -> 1: + .
*(sub a b) -> 1: a b - .

*(main args) -> 4:
  (operands)         (sum) // 0.2 + 3
  (operands) (!swap) (sum) // 3 + 0.2
  (operands)         (sub) // 0.2 - 3
  (operands) (!swap) (sub) // 3 - 0.2
  (!swap)
  .
