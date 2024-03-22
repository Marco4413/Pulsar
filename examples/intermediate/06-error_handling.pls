#include "05-random.pls"

*(*error!).
*(*scall args fn) -> 2.
*(*tcall args fn) -> 2.

*(my-func) -> 1:
  (random/int)
  (*error!)
  .

*(main args):
  // scall makes a copy of globals:
  // - Changes to globals don't affect the main context
  // - Handles lose their reference
  // - Handles created within the call are freed
  random/seed -> seed
  [] <& (my-func) (*scall) if not: (*error!) end
  if random/seed != seed: (*error!) end

  // tcall inherits the whole context:
  // - Changes to globals affect the main context
  // - Handles can be passed
  // - Handles created within the call ARE NOT freed
  random/seed -> seed
  [] <& (my-func) (*tcall) if not: (*error!) end
  if random/seed = seed: (*error!) end
  .
