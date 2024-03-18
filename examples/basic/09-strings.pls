*(str-list-join l sep) -> 1:
  <- l
    (!empty?) if: "".
    (!head) -> head
      (!empty?) if: <- head .
    sep (str-list-join)
  <- head sep (!append)
    (!prepend)
  .

*(main args) -> 1:
  <- args (!tail) " " (str-list-join)
  .
