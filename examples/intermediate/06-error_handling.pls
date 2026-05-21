#include "05-random.pls"

*(*error!).
*(*scall args fn) -> 2.
*(*tcall args fn) -> 2.

*(my-func) -> 1:
  (random/int)
  (*error!)
  .

*(main args):
  // scall forks globals:
  // - Changes to globals don't affect the main context
  // - CustomTypeGlobalData is NOT shared with the main context.
  //   If supported, it is forked from its current state.
  //   Otherwise, the data is meant to be shared between contexts.
  random/seed -> seed
  [] <& (my-func) (*scall) if not: (*error!) end
  // random/seed MUST not have changed
  if random/seed != seed: (*error!) end

  // tcall shares the context:
  // - Changes to globals affect the main context
  // - CustomTypeGlobalData is shared with the main context.
  random/seed -> seed
  [] <& (my-func) (*tcall) if not: (*error!) end
  // random/seed MUST have changed
  if random/seed = seed: (*error!) end
  .
