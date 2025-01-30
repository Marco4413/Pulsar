#ifndef _CPULSAR_RUNTIME_H
#define _CPULSAR_RUNTIME_H

#include "cpulsar/core.h"

#include "cpulsar/runtime/locals.h"
#include "cpulsar/runtime/stack.h"

typedef enum {
    CPulsar_RuntimeState_OK    = 0,
    CPulsar_RuntimeState_Error = 1,
} CPulsar_RuntimeState;

typedef void* CPulsar_Module;

typedef void* CPulsar_Frame;
typedef void* CPulsar_ExecutionContext;

typedef struct {
    const char* Name;
    size_t StackArity;
    size_t Arity;
    size_t Returns;
} CPulsar_FunctionSignature;

typedef CPulsar_RuntimeState(*CPulsar_NativeFunction)(CPulsar_ExecutionContext);

#ifdef CPULSAR_CPP
extern "C" {
#endif

CPULSAR_API const char* CPulsar_RuntimeState_ToString(CPulsar_RuntimeState runtimeState);

CPULSAR_API CPulsar_Module CPulsar_Module_Create();
CPULSAR_API void CPulsar_Module_Delete(CPulsar_Module self);

// Returns how many functions were bound
CPULSAR_API size_t CPulsar_Module_BindNativeFunction(CPulsar_Module self, CPulsar_FunctionSignature fnSig, CPulsar_NativeFunction nativeFn);
// Returns the index at which the function was declared
CPULSAR_API int64_t CPulsar_Module_DeclareAndBindNativeFunction(CPulsar_Module self, CPulsar_FunctionSignature fnSig, CPulsar_NativeFunction nativeFn);
// Calls the above functions depending on declareAndBind
CPULSAR_API void CPulsar_Module_BindNativeFunctionEx(CPulsar_Module self, CPulsar_FunctionSignature fnSig, CPulsar_NativeFunction nativeFn, int declareAndBind);

CPULSAR_API CPulsar_Locals CPulsar_Frame_GetLocals(CPulsar_Frame self);
CPULSAR_API CPulsar_Stack CPulsar_Frame_GetStack(CPulsar_Frame self);

CPULSAR_API CPulsar_ExecutionContext CPulsar_ExecutionContext_Create(const CPulsar_Module module);
CPULSAR_API void CPulsar_ExecutionContext_Delete(CPulsar_ExecutionContext self);

CPULSAR_API CPulsar_Stack CPulsar_ExecutionContext_GetStack(CPulsar_ExecutionContext self);
CPULSAR_API CPulsar_Frame CPulsar_ExecutionContext_CurrentFrame(CPulsar_ExecutionContext self);
CPULSAR_API CPulsar_RuntimeState CPulsar_ExecutionContext_CallFunctionByName(CPulsar_ExecutionContext self, const char* fnName);

CPULSAR_API CPulsar_RuntimeState CPulsar_ExecutionContext_Run(CPulsar_ExecutionContext self);
CPULSAR_API CPulsar_RuntimeState CPulsar_ExecutionContext_GetState(const CPulsar_ExecutionContext self);

#ifdef CPULSAR_CPP
}
#endif

#endif // _CPULSAR_RUNTIME_H
