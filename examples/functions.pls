*(swap x y) -> 2: y x.

*(operands) -> 2: 0.2 3.

*(main args) -> 4:
    (operands)        + // 0.2 + 3
    (operands) (swap) + // 3 + 0.2
    (operands)        - // 0.2 - 3
    (operands) (swap) - // 3 - 0.2
    (swap)
    .
