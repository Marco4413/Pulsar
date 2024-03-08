*(gen_n->n+5 n) -> 5:
  // This is highly inefficient.
  n
  n 1 + (!dup) -> n
  n 1 + (!dup) -> n
  n 1 + (!dup) -> n
  n 1 +
  .

*(main args) -> 5: 1 (gen_n->n+5) .
