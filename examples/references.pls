*(*hello-from-cpp).

*(call func) -> 1:
    10 func icall!
    .

*(identity x) -> 1: x.

*(main) -> 1:
    <& (identity) (call)
    <& (*hello-from-cpp) (call)
    .
