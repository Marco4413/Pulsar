#include "00-lexer_bindings.pls"

// NOTE: Pulsar itself is not designed to be thread-safe (because threads are not part of the language).
//   Therefore, make sure that the native bindings you're using are thread-safe
//   and don't pass handles to threads if you don't know whether they're thread-safe or not.
//   In this example lexers are created and used by single threads.

// Set to 1 to run the example single-threaded
global const 0 -> thread_bindings/run-single-threaded?

*(*thread/run      args fn) -> 1.
*(*thread/join     thread ) -> 2.
*(*thread/join-all threads) -> 1.
*(*thread/alive?   thread ) -> 1.
*(*thread/valid?   thread ) -> 1.

*(main-st args) -> 1:
  [] -> results
  <- args (!tail) while:
    (!empty?) if: break
    (!head) (lex-file)
      -> result
    <- results <- result (!append) -> results
  end
  <- results
  .

*(main-mt args) -> 1:
  [] -> threads
  <- args (!tail) while:
    (!empty?) if: break
    (!head) -> filepath
    [ <- filepath ] <& (lex-file) (*thread/run)
      -> thread
    <- threads <- thread (!append)
      -> threads
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


