*(*stack-dump!).

*(main args) -> 2:
  0 -> n
  1 if:
    1 -> !n
    n * (!stack 1)
  end
  n
  // Stack [ 1, 0 ]
  .
