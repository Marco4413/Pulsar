#include "cpulsar/runtime.h"

#include <stdio.h>

CPulsar_RuntimeState NativePrintln(CPulsar_ExecutionContext context, void* args)
{
    (void)args;

    CPulsar_Frame  frame  = CPulsar_ExecutionContext_CurrentFrame(context);
    CPulsar_Locals locals = CPulsar_Frame_GetLocals(frame);
    CPulsar_Value  msg    = CPulsar_Locals_Get(locals, 0);
    if (!CPulsar_Value_IsString(msg)) {
        return CPulsar_RuntimeState_Error;
    }

    printf("%s\n", CPulsar_Value_AsString(msg));
    return CPulsar_RuntimeState_OK;
}

CPULSAR_EXPORT void BindFunctions(CPulsar_Module module, int declareAndBind)
{
    CPulsar_Module_BindNativeFunctionEx(
        module,
        (CPulsar_FunctionSignature){
            .Name = "println!",
            .Arity = 1,
            .Returns = 0,
        },
        NativePrintln,
        CPULSAR_CBUFFER_NULL,
        declareAndBind
    );
}
