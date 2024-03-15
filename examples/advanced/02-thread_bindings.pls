#include "00-lexer_bindings.pls"

// NOTE: Pulsar itself is not designed to be thread-safe (because threads are not part of the language).
//   Therefore, make sure that the native bindings you're using are thread-safe
//   and don't pass handles to threads if you don't know whether they're thread-safe or not.
//   In this example lexers are created and used by single threads.

// Set to 1 to run the example single-threaded
global const 0 -> thread_bindings/run-single-threaded?

*(*thread/run      fn     args) -> 1.
*(*thread/join     handle     ) -> 1.
*(*thread/join-all handle-list) -> 1.
*(*thread/alive?   handle     ) -> 2.
*(*thread/valid?   handle     ) -> 2.

*(main-st args) -> 1:
  [] -> results
  <- args (!tail) do:
    (!empty?) if: break
      (!head) (lex-file)
        -> result
      <- results <- result (!append) -> results
    continue
  end
  <- results
  .

*(main-mt args) -> 1:
  [] -> threads
  <- args (!tail) do:
    (!empty?) if: break
    (!head) -> filepath
    <& (lex-file) [ <- filepath ] (*thread/run)
      -> thread
    <- threads <- thread (!append)
      -> threads
    continue
  end
  <- threads (*thread/join-all)
  .

*(main args) -> 1:
  thread_bindings/run-single-threaded? if:
    <- args (main-st)
  else:
    <- args (main-mt)
  end
  .


