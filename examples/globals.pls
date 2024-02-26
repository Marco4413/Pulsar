#include "lists-sorting.pls"
#include "recursion.pls"

// global producers!
// They're functions that are evaluated by the parser to assign pre-computed values to globals.
// It will reduce execution time. However, the time it takes to parse the file will increase. 
global const -> FIB92: 92 (fib-list).
global const -> FIB92_SORTED: FIB92 (sort).

global 0 -> my-global

*(glob-mut-1):
    FIB92 -> my-global
    .

*(glob-mut-2):
    FIB92_SORTED -> my-global
    .

*(glob-mut-3?):
    // Here the creation of a local is forced.
    // Therefore, shadowing the global.
    FIB92_SORTED -> !my-global
    .

*(main args) -> 1:
    [my-global]
        (glob-mut-1)  <- my-global (!append)
        (glob-mut-2)  <- my-global (!append)
        (glob-mut-3?) <- my-global (!append)
    .
