#ifndef _CPULSAR_RUNTIME_H
#define _CPULSAR_RUNTIME_H

#include "cpulsar/core.h"

#include "cpulsar/runtime/customtype.h"
#include "cpulsar/cbuffer.h"
#include "cpulsar/runtime/locals.h"
#include "cpulsar/runtime/stack.h"

typedef enum {
    CPulsar_RuntimeState_OK    = 0,
    CPulsar_RuntimeState_Error = 1,
} CPulsar_RuntimeState;

typedef struct CPulsar_Module_S* CPulsar_Module;

typedef struct CPulsar_Frame_S* CPulsar_Frame;
typedef struct CPulsar_ExecutionContext_S* CPulsar_ExecutionContext;

typedef struct {
    const char* Name;
    size_t StackArity;
    size_t Arity;
    size_t Returns;
} CPulsar_FunctionSignature;

typedef CPulsar_RuntimeState(CPULSAR_CALL *CPulsar_NativeFunction)(CPulsar_ExecutionContext, void*);

#ifdef CPULSAR_CPP
extern "C" {
#endif

CPULSAR_API const char* CPULSAR_CALL CPulsar_RuntimeState_ToString(CPulsar_RuntimeState runtimeState);

CPULSAR_API CPulsar_Module CPULSAR_CALL CPulsar_Module_Create(void);
CPULSAR_API void           CPULSAR_CALL CPulsar_Module_Delete(CPulsar_Module self);

// `nativeFnArgs.Data` will be passed to `nativeFn` on each call.
// The lifecycle of `nativeFnArgs` will be managed by Pulsar.
// Returns how many functions were bound.
CPULSAR_API size_t CPULSAR_CALL CPulsar_Module_BindNativeFunction(
        CPulsar_Module self, CPulsar_FunctionSignature fnSig,
        CPulsar_NativeFunction nativeFn, CPulsar_CBuffer nativeFnArgs);
// `nativeFnArgs.Data` will be passed to `nativeFn` on each call.
// The lifecycle of `nativeFnArgs` will be managed by Pulsar.
// Returns the index at which the function was declared
CPULSAR_API int64_t CPULSAR_CALL CPulsar_Module_DeclareAndBindNativeFunction(
        CPulsar_Module self, CPulsar_FunctionSignature fnSig,
        CPulsar_NativeFunction nativeFn, CPulsar_CBuffer nativeFnArgs);

// Calls the above functions depending on declareAndBind
CPULSAR_API void CPULSAR_CALL CPulsar_Module_BindNativeFunctionEx(
        CPulsar_Module self, CPulsar_FunctionSignature fnSig,
        CPulsar_NativeFunction nativeFn, CPulsar_CBuffer nativeFnArgs,
        int declareAndBind);

CPULSAR_API uint64_t CPULSAR_CALL CPulsar_Module_BindCustomType(CPulsar_Module self, const char* typeName, CPulsar_CustomType_DataFactoryFn dataFactory);
// HACK: Returns 0 if the type could not be found. It's safe but not documented within Pulsar.
CPULSAR_API uint64_t CPULSAR_CALL CPulsar_Module_FindCustomType(const CPulsar_Module self, const char* typeName);

CPULSAR_API CPulsar_Locals CPULSAR_CALL CPulsar_Frame_GetLocals(CPulsar_Frame self);
CPULSAR_API CPulsar_Stack  CPULSAR_CALL CPulsar_Frame_GetStack(CPulsar_Frame self);

CPULSAR_API CPulsar_ExecutionContext CPULSAR_CALL CPulsar_ExecutionContext_Create(const CPulsar_Module module);
CPULSAR_API void                     CPULSAR_CALL CPulsar_ExecutionContext_Delete(CPulsar_ExecutionContext self);

// If the returned reference is not NULL, ownership of the reference is given to the caller.
CPULSAR_API CPulsar_CustomTypeData_Ref CPULSAR_CALL CPulsar_ExecutionContext_GetCustomTypeData(CPulsar_ExecutionContext self, uint64_t typeId);
// Helper method to directly get a CBuffer from type data. Prefer the above method if you're worried about its lifecycle.
CPULSAR_API CPulsar_CBuffer*           CPULSAR_CALL CPulsar_ExecutionContext_GetCustomTypeDataBuffer(CPulsar_ExecutionContext self, uint64_t typeId);

CPULSAR_API CPulsar_Stack        CPULSAR_CALL CPulsar_ExecutionContext_GetStack(CPulsar_ExecutionContext self);
CPULSAR_API CPulsar_Frame        CPULSAR_CALL CPulsar_ExecutionContext_CurrentFrame(CPulsar_ExecutionContext self);
CPULSAR_API CPulsar_RuntimeState CPULSAR_CALL CPulsar_ExecutionContext_CallFunctionByName(CPulsar_ExecutionContext self, const char* fnName);

CPULSAR_API CPulsar_RuntimeState CPULSAR_CALL CPulsar_ExecutionContext_Run(CPulsar_ExecutionContext self);
CPULSAR_API CPulsar_RuntimeState CPULSAR_CALL CPulsar_ExecutionContext_GetState(const CPulsar_ExecutionContext self);

#ifdef CPULSAR_CPP
}
#endif

#endif // _CPULSAR_RUNTIME_H
