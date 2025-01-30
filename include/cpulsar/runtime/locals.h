#ifndef _CPULSAR_RUNTIME_LOCALS_H
#define _CPULSAR_RUNTIME_LOCALS_H

#include "cpulsar/core.h"

#include "cpulsar/runtime/value.h"

typedef struct CPulsar_Locals_S* CPulsar_Locals;

#ifdef CPULSAR_CPP
extern "C" {
#endif

CPULSAR_API CPulsar_Value CPulsar_Locals_Get(CPulsar_Locals self, size_t idx);
// Moves `value` into the list at `idx`. `value` must be deleted by its owner
CPULSAR_API void CPulsar_Locals_Set(CPulsar_Locals self, size_t idx, CPulsar_Value value);
// Copies `value` into the list at `idx`. `value` must be deleted by its owner
CPULSAR_API void CPulsar_Locals_SetCopy(CPulsar_Locals self, size_t idx, const CPulsar_Value value);

#ifdef CPULSAR_CPP
}
#endif

#endif // _CPULSAR_RUNTIME_LOCALS_H
