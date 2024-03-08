#include "00-recursion.pls"
#include "01-lists_sorting.pls"

global const -> FIB92:
  92 (fib-list) (sort)
  .

*(index list idx) -> 1:
  idx if 0: <- list (!head).
  <- list (!tail) idx 1 - (index)
  .

*(index-while list idx) -> 1:
  <- list // The list is on the stack
  while idx > 0:
    (!tail) // Pop the head of the list
    idx 1 - -> idx
  end
  (!head)
  .

*(main args) -> 1:
  // Run them one at a time to see the difference in performance.
  // FIB92 91 (index)
  FIB92 91 (index-while)
  .
