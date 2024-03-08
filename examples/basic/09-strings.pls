*(str-list-join l sep) -> 1:
  <- l
    (!length) if 0: "".
    (!length) if 1: (!head).
    (!head) -> head
    sep (str-list-join)
  <- head sep (!append)
    (!prepend)
  .

*(main args) -> 1:
  <- args (!tail) " " (str-list-join)
  .
