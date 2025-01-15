// Native Bindings
*(*println! v).
*(*stack-dump!).

*(main args):
  0.2 3
  (*stack-dump!) // [ 0.2, 3 ]
  (*println!) (*println!)
  .
