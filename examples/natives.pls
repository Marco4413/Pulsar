// Native Bindings
*(*print! v).
*(*hello-from-cpp).
*(*stack-dump).

*(main) -> 0:
    (*hello-from-cpp)
    0.2 3
    (*stack-dump) // [ 0.2, 3 ]
    (*print!) (*print!)
    .
