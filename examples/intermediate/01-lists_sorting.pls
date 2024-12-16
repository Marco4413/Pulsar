*(split-array array len) -> 2:
  [] while:
    <- array
      (!empty?) if: (!swap) .
      len if <= 0:  (!swap) .
    (!head) (!swap) -> array
    (!append)
    len 1 - -> len
  end
  .

*(merge a b) -> 1:
  <- a (!length) -> a_len -> a
  <- b (!length) -> b_len -> b

  [] -> merged

  while:
    a_len if not: break
    b_len if not: break

    // Take the elements to compare
    <- a (!head) -> x -> a
    <- b (!head) -> y -> b

    x y (!compare) if < 0:
      <- merged <- x (!append)  -> merged
      <- b      <- y (!prepend) -> b
      a_len 1 - -> a_len // Element taken from a
    else:
      <- merged <- y (!append)  -> merged
      <- a      <- x (!prepend) -> a
      b_len 1 - -> b_len // Element taken from b
    end
  end

  <- merged
  <- a (!concat)
  <- b (!concat)
  .

*(merge-sort array) -> 1:
  <- array (!length) -> array_len
  array_len if <= 1: .

  array_len 2 /
    (split-array)
  
  (merge-sort) (!swap)
  (merge-sort) (merge)
  .

*(sort array) -> 1:
  <- array (merge-sort) .

*(main args) -> 1:
  [ // https://www.random.org/strings
    "eLJYx4IJJEu3szsHd0Fy",
    "tOFHg7xW9b5XnK1TxqqL",
    "cbJ8WlhxYfqXeULY8cpM",
    "8scIWhKATFI5L5m8vkWZ",
    "LcmVEA99iZzbpSuq0rVp",
    "W1XaD0lPz1Smw0Gz54p9",
    "zEDOpsPWal92t0TnMdgC",
    "N3LTCJW7suOV3PAZqYQO",
    "LJIFOk9BeUjOCXy5HCJq",
    "LJIFOk9BeUjOCXy5HCJq",
    "XvsF0pC0NLv5ebGTuxf8",
    "AXTLGe3ufGgUnC7ivYA6",
    "ricAGfUURUn3S8RuNysT",
    "J7G7vVlExYAVxfcuNDAq",
    "eHtnT8C4EJlwRDp8XDKU",
    "yPn3ee7BCJ7uzJ7R1X16",
    "T9PfTA2jlKoDbsPs6okN",
    "uiEn5WLg3GfgAt8lnC9w",
    "HTJSVJ7QAeZGBVXOkvoi",
    "ACBVOaioC1BX1sycCE08",
  ] (sort)
  .
