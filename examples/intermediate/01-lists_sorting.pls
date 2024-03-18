*(sort l) -> 1:
  <- l
    (!empty?) if: .
    (!head) -> a
      (!empty?) if: [ <- a ] .
      (sort)
    // We can assume the tail is sorted.
    (!head) -> b
    a b (!compare) if <= 0:
      <- b (!prepend)
      <- a (!prepend)
    else:
      <- a (!prepend)
      <- b (!prepend)
      (sort) // The thing may not be sorted! Do it again!
    end
  .

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