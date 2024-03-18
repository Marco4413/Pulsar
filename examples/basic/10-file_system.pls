*(*fs/exists?  path) -> 2.
*(*fs/read-all path) -> 1.

*(*println! val).

*(main args) -> 0:
  <- args (!tail)
    (!empty?) if:
      "No file provided."
        (*println!)
      .
  (!head)
    (*fs/exists?) if not:
      "The specified file does not exist."
        (*println!)
      .
  (*fs/read-all)
    (*println!)
  .
