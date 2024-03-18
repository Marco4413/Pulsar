*(*time/micros) -> 1.

global 1 -> random/seed
global const -> random/max-int-as-double:
  0 ~
    1 >>
    1.0 +
  .

// Xorshift 64bit RNG
*(random/int) -> 1:
  random/seed
    (!dup) 13 << ^
    (!dup)  7 >> ^
    (!dup) 17 << ^
    <-> random/seed
  .

// Random double between -1.0 and 1.0
*(random/double) -> 1:
  (random/int) random/max-int-as-double /
  .

*(main args) -> 1:
  (*time/micros) -> random/seed
  10 -> n
  [] 0 -> i while i < n:
    (random/double) (!append)
    i 1 + -> i
  end
  .
