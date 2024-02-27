*(*println! v).

*(dup x) -> 2: x x.

*(for n func) -> 0:
    if n > 1:
        n 1 -; func; (for)
    end
    n 1 -; func; (!icall)
    (*println!)
    .

*(is-even x) -> 1: x 1 +; 2 %.
*(int-div-by-2 x) -> 1: x 2 /.
*(dbl-div-by-2 x) -> 1: x 2.0 /.

*(main args):
    // Comment out to see the differences.
    // It's a bit of a mess how things are printed.
    5; <& (is-even); (for)
    // 5; <& (int-div-by-2); (for)
    // 5; <& (dbl-div-by-2); (for)
    .
