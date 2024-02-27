// Native Bindings
*(*println! v).
*(*hello-from-cpp).
*(*stack-dump).

*(main args):
    (*hello-from-cpp)
    0.2 3
    (*stack-dump) // [ 0.2, 3 ]
    (*println!) (*println!)
    .
