*(*this-thread/sleep! delay).
*(*thread/run args fn) -> 1.
*(*thread/join   thread) -> 2.
*(*thread/join-all threads) -> 1.
*(*thread/alive? thread) -> 1.
*(*thread/valid? thread) -> 1.
