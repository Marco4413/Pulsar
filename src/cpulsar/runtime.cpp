#define _CPULSAR_IMPLEMENTATION
#include "cpulsar/runtime.h"

#include "pulsar/runtime.h"

static_assert(sizeof(CPulsar_RuntimeState) >= sizeof(Pulsar::RuntimeState));

using Module           = Pulsar::Module;

using Frame            = Pulsar::Frame;
using ExecutionContext = Pulsar::ExecutionContext;

extern "C"
{

CPULSAR_API const char* CPulsar_RuntimeState_ToString(CPulsar_RuntimeState runtimeState)
{
    return RuntimeStateToString((Pulsar::RuntimeState)runtimeState);
}

CPULSAR_API CPulsar_Module CPulsar_Module_Create()
{
    return PULSAR_NEW(Module);
}

CPULSAR_API void CPulsar_Module_Delete(CPulsar_Module _self)
{
    PULSAR_DELETE(Module, &CPULSAR_DEREF(Module, _self));
}

CPULSAR_API size_t CPulsar_Module_BindNativeFunction(CPulsar_Module _self, CPulsar_FunctionSignature fnSig, CPulsar_NativeFunction nativeFn)
{
    return CPULSAR_DEREF(Module, _self)
        .BindNativeFunction({
            .Name = fnSig.Name,
            .Arity = fnSig.Arity,
            .Returns = fnSig.Returns,
            .StackArity = fnSig.StackArity,
        }, [nativeFn](Pulsar::ExecutionContext& context) {
            return (Pulsar::RuntimeState)nativeFn(CPULSAR_REF(CPulsar_ExecutionContext, context));
        });
}

CPULSAR_API int64_t CPulsar_Module_DeclareAndBindNativeFunction(CPulsar_Module _self, CPulsar_FunctionSignature fnSig, CPulsar_NativeFunction nativeFn)
{
    return CPULSAR_DEREF(Module, _self)
        .DeclareAndBindNativeFunction({
            .Name = fnSig.Name,
            .Arity = fnSig.Arity,
            .Returns = fnSig.Returns,
            .StackArity = fnSig.StackArity,
        }, [nativeFn](Pulsar::ExecutionContext& context) {
            return (Pulsar::RuntimeState)nativeFn(CPULSAR_REF(CPulsar_ExecutionContext, context));
        });
}

CPULSAR_API void CPulsar_Module_BindNativeFunctionEx(CPulsar_Module self, CPulsar_FunctionSignature fnSig, CPulsar_NativeFunction nativeFn, int declareAndBind)
{
    if (declareAndBind) {
        CPulsar_Module_DeclareAndBindNativeFunction(self, fnSig, nativeFn);
    } else {
        CPulsar_Module_BindNativeFunction(self, fnSig, nativeFn);
    }
}

CPULSAR_API CPulsar_Locals CPulsar_Frame_GetLocals(CPulsar_Frame _self)
{
    return CPULSAR_REF(CPulsar_Locals, CPULSAR_DEREF(Frame, _self).Locals);
}

CPULSAR_API CPulsar_Stack CPulsar_Frame_GetStack(CPulsar_Frame _self)
{
    return CPULSAR_REF(CPulsar_Stack, CPULSAR_DEREF(Frame, _self).Stack);
}

CPULSAR_API CPulsar_ExecutionContext CPulsar_ExecutionContext_Create(const CPulsar_Module _module)
{
    return PULSAR_NEW(ExecutionContext, CPULSAR_DEREF(const Module, _module));
}

CPULSAR_API void CPulsar_ExecutionContext_Delete(CPulsar_ExecutionContext _self)
{
    PULSAR_DELETE(ExecutionContext, &CPULSAR_DEREF(ExecutionContext, _self));
}

CPULSAR_API CPulsar_Stack CPulsar_ExecutionContext_GetStack(CPulsar_ExecutionContext _self)
{
    return CPULSAR_REF(CPulsar_Stack, CPULSAR_DEREF(ExecutionContext, _self).GetStack());
}

CPULSAR_API CPulsar_Frame CPulsar_ExecutionContext_CurrentFrame(CPulsar_ExecutionContext _self)
{
    return CPULSAR_REF(CPulsar_Frame, CPULSAR_DEREF(ExecutionContext, _self).CurrentFrame());
}

CPULSAR_API CPulsar_RuntimeState CPulsar_ExecutionContext_CallFunctionByName(CPulsar_ExecutionContext _self, const char* fnName)
{
    return (CPulsar_RuntimeState)CPULSAR_DEREF(ExecutionContext, _self).CallFunction(fnName);
}

CPULSAR_API CPulsar_RuntimeState CPulsar_ExecutionContext_Run(CPulsar_ExecutionContext _self)
{
    return (CPulsar_RuntimeState)CPULSAR_DEREF(ExecutionContext, _self).Run();
}

CPULSAR_API CPulsar_RuntimeState CPulsar_ExecutionContext_GetState(const CPulsar_ExecutionContext _self)
{
    return (CPulsar_RuntimeState)CPULSAR_DEREF(const ExecutionContext, _self).GetState();
}

}
