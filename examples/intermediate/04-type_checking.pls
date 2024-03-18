*(*panic!).
*(*panic/type!).

*(*println! val).

global const -> Void:
  0 -> x
  <- x
  x // x is of Void type because it was moved
  .

*(foo val) -> 1:
  <- val
    (!void?) if: "Foo" .
    (!string?) if: "Foo " (!prepend) .
    (*panic/type!) // Void or String
  .

*(main args):
  Void  (foo) (*println!)
  "Bar" (foo) (*println!)
  400   (foo) (*println!)
  .
