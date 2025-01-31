#define _CPULSAR_IMPLEMENTATION
#include "cpulsar/runtime.h"

#include "pulsar/runtime.h"

static_assert(sizeof(CPulsar_RuntimeState) >= sizeof(Pulsar::RuntimeState));

using Module           = Pulsar::Module;

using Frame            = Pulsar::Frame;
using ExecutionContext = Pulsar::ExecutionContext;

class CBufferWrapper
{
public:
    CBufferWrapper(CPulsar_CBuffer buf) :
        m_Buffer(buf)
    {}

    ~CBufferWrapper()
    {
        if (m_Buffer.Free) {
            m_Buffer.Free(m_Buffer.Data);
        }
    }

    CPulsar_CBuffer& GetBuffer() { return m_Buffer; }

private:
    CPulsar_CBuffer m_Buffer;
};

extern "C"
{

CPULSAR_API const char* CPulsar_RuntimeState_ToString(CPulsar_RuntimeState runtimeState)
{
    return RuntimeStateToString((Pulsar::RuntimeState)runtimeState);
}

CPULSAR_API CPulsar_Module CPulsar_Module_Create(void)
{
    return CPULSAR_REF(CPulsar_Module_S, *PULSAR_NEW(Module));
}

CPULSAR_API void CPulsar_Module_Delete(CPulsar_Module _self)
{
    PULSAR_DELETE(Module, &CPULSAR_DEREF(Module, _self));
}

CPULSAR_API size_t CPulsar_Module_BindNativeFunction(
        CPulsar_Module _self, CPulsar_FunctionSignature fnSig,
        CPulsar_NativeFunction nativeFn, CPulsar_CBuffer _nativeFnArgs)
{
    auto nativeFnArgs = Pulsar::SharedRef<CBufferWrapper>::New(_nativeFnArgs);
    return CPULSAR_DEREF(Module, _self)
        .BindNativeFunction({
            .Name = fnSig.Name,
            .Arity = fnSig.Arity,
            .Returns = fnSig.Returns,
            .StackArity = fnSig.StackArity,
        }, [nativeFn, nativeFnArgs](Pulsar::ExecutionContext& context) {
            return (Pulsar::RuntimeState)nativeFn(
                CPULSAR_REF(CPulsar_ExecutionContext_S, context),
                nativeFnArgs->GetBuffer().Data
            );
        });
}

CPULSAR_API int64_t CPulsar_Module_DeclareAndBindNativeFunction(
        CPulsar_Module _self, CPulsar_FunctionSignature fnSig,
        CPulsar_NativeFunction nativeFn, CPulsar_CBuffer _nativeFnArgs)
{
    auto nativeFnArgs = Pulsar::SharedRef<CBufferWrapper>::New(_nativeFnArgs);
    return CPULSAR_DEREF(Module, _self)
        .DeclareAndBindNativeFunction({
            .Name = fnSig.Name,
            .Arity = fnSig.Arity,
            .Returns = fnSig.Returns,
            .StackArity = fnSig.StackArity,
        }, [nativeFn, nativeFnArgs](Pulsar::ExecutionContext& context) {
            return (Pulsar::RuntimeState)nativeFn(
                CPULSAR_REF(CPulsar_ExecutionContext_S, context),
                nativeFnArgs->GetBuffer().Data
            );
        });
}

CPULSAR_API void CPulsar_Module_BindNativeFunctionEx(
        CPulsar_Module self, CPulsar_FunctionSignature fnSig,
        CPulsar_NativeFunction nativeFn, CPulsar_CBuffer nativeFnArgs,
        int declareAndBind)
{
    if (declareAndBind) {
        CPulsar_Module_DeclareAndBindNativeFunction(self, fnSig, nativeFn, nativeFnArgs);
    } else {
        CPulsar_Module_BindNativeFunction(self, fnSig, nativeFn, nativeFnArgs);
    }
}

CPULSAR_API uint64_t CPulsar_Module_BindCustomType(CPulsar_Module _self, const char* typeName, CPulsar_CustomType_DataFactoryFn dataFactory)
{
    return CPULSAR_DEREF(Module, _self)
        .BindCustomType(
            typeName,
            [dataFactory]() {
                CPulsar_CustomTypeData_Ref cref = dataFactory();
                Pulsar::CustomTypeData::Ref ref = CPULSAR_DEREF(Pulsar::CustomTypeData::Ref, cref);
                CPulsar_CustomTypeData_Ref_Delete(cref);
                return ref;
            }
        );
}

CPULSAR_API uint64_t CPulsar_Module_FindCustomType(const CPulsar_Module _self, const char* typeName)
{
    Module& self = CPULSAR_DEREF(Module, _self);
    uint64_t typeId = 0;
    // HACK: There should be a method to do this. It's being done enough times to warrant it.
    self.CustomTypes.ForEach([typeName, &typeId](const auto& bucket) {
        if (!typeId && bucket.Value().Name == typeName) {
            typeId = bucket.Key();
        }
    });
    return typeId;
}

CPULSAR_API CPulsar_Locals CPulsar_Frame_GetLocals(CPulsar_Frame _self)
{
    return CPULSAR_REF(CPulsar_Locals_S, CPULSAR_DEREF(Frame, _self).Locals);
}

CPULSAR_API CPulsar_Stack CPulsar_Frame_GetStack(CPulsar_Frame _self)
{
    return CPULSAR_REF(CPulsar_Stack_S, CPULSAR_DEREF(Frame, _self).Stack);
}

CPULSAR_API CPulsar_ExecutionContext CPulsar_ExecutionContext_Create(const CPulsar_Module _module)
{
    return CPULSAR_REF(CPulsar_ExecutionContext_S, *PULSAR_NEW(ExecutionContext, CPULSAR_DEREF(const Module, _module)));
}

CPULSAR_API void CPulsar_ExecutionContext_Delete(CPulsar_ExecutionContext _self)
{
    PULSAR_DELETE(ExecutionContext, &CPULSAR_DEREF(ExecutionContext, _self));
}

CPULSAR_API CPulsar_CustomTypeData_Ref CPulsar_ExecutionContext_GetCustomTypeData(CPulsar_ExecutionContext _self, uint64_t typeId)
{
    ExecutionContext& self = CPULSAR_DEREF(ExecutionContext, _self);
    auto typeData = self.GetCustomTypeData<Pulsar::CustomTypeData>(typeId);
    if (!typeData) return NULL;
    return CPULSAR_REF(CPulsar_CustomTypeData_Ref_S, *PULSAR_NEW(Pulsar::CustomTypeData::Ref, typeData));
}

CPULSAR_API CPulsar_CBuffer* CPulsar_ExecutionContext_GetCustomTypeDataBuffer(CPulsar_ExecutionContext self, uint64_t typeId)
{
    CPulsar_CustomTypeData_Ref ref = CPulsar_ExecutionContext_GetCustomTypeData(self, typeId);
    CPulsar_CBuffer* buf = CPulsar_CustomTypeData_Ref_GetBuffer(ref);
    CPulsar_CustomTypeData_Ref_Delete(ref);
    return buf;
}

CPULSAR_API CPulsar_Stack CPulsar_ExecutionContext_GetStack(CPulsar_ExecutionContext _self)
{
    return CPULSAR_REF(CPulsar_Stack_S, CPULSAR_DEREF(ExecutionContext, _self).GetStack());
}

CPULSAR_API CPulsar_Frame CPulsar_ExecutionContext_CurrentFrame(CPulsar_ExecutionContext _self)
{
    return CPULSAR_REF(CPulsar_Frame_S, CPULSAR_DEREF(ExecutionContext, _self).CurrentFrame());
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
