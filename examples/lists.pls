*(*stack-dump).

*(map l f) -> 1:
    <- l
        (!length) if = 0: .
        (!head) f (!icall) -> v
        f (map)
    <- v
        (*stack-dump)
        (!prepend)
    .

*(add-4 x) -> 1: x 4 +.

*(main) -> 1:
    [ 1, 2, 3 ]
        0 (!prepend)
    <-> list
    list; <- list <& (add-4) (map); (!concat)
    .
