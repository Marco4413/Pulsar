#!/usr/bin/env -S ./pulsar-tools run --no-bind-all --bind-error -Llibpulsar-tools-ext.so

// See sha-bang for how to run this file.

*(*println! val).

*(*ext/integer/create) -> 1.
*(*ext/integer/set self val) -> 1.
*(*ext/integer/get self) -> 2.

*(*scall args fn) -> 2.

*(smain args) -> 2:
  (*ext/integer/create)
    200 (*ext/integer/set)
    (*ext/integer/get)

  // Unwrap the default value
  (*ext/integer/create)
    (*ext/integer/get)

  local i1 v1 i2 v2:
    v1 v2
  end
  .

*(main args) -> 2:
  "I C some C 😱" (*println!)
  args <& (smain) (*scall)
    (!pop) (!unpack 2)
  .
