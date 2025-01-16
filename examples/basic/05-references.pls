*(*println! val).

*(identity x) -> 1: x.

*(main args):
  "Hello, native!"
    <& (identity) (!icall)
    <& (*println!) (!icall)
  .
