#include "00-lexer_bindings.pls"

*(*thread/run      args fn) -> 1.
*(*thread/join     thread ) -> 2.
*(*thread/join-all threads) -> 1.
*(*thread/alive?   thread ) -> 1.
*(*thread/valid?   thread ) -> 1.

*(*channel/new) -> 1.
*(*channel/send!   value channel).
*(*channel/receive channel) -> 1.
*(*channel/close!  channel).
*(*channel/free!   channel).
*(*channel/empty?  channel) -> 1.
*(*channel/closed? channel) -> 1.
*(*channel/valid?  channel) -> 1.

*(*stdin/read) -> 1.
*(*stdout/write! str).
*(*stdout/writeln! str).

*(*println! val).

*(worker-thread in out):
  while:
    in (*channel/receive)
      (!void?) if: break
      (lex-file) out
        (*channel/send!)
  end
  .

*(main args):
  (*channel/new) -> in
  (*channel/new) -> out
  [ in, out ] <& (worker-thread)
    (*thread/run) -> thread

  while:
    "PATH> " (*stdout/write!)
    (*stdin/read)
      (!dup) if "q": (!pop) break
      in (*channel/send!)
    out (*channel/receive)
      (*println!)
  end

  in (*channel/close!)
  <- thread (*thread/join) (!pop 2)
  // Only free Channels when they're no longer used
  <- in  (*channel/free!)
  <- out (*channel/free!)
  .