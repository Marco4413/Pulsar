#define _CPULSAR_IMPLEMENTATION
#include "cpulsar/runtime.h"
#include "cpulsar/_opaque.hpp"

#include "pulsar/runtime.h"

static_assert(sizeof(CPulsar_RuntimeState) >= sizeof(Pulsar::RuntimeState));

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

static Pulsar::RuntimeState CRuntimeStateToCppRuntimeState(CPulsar_RuntimeState cstate);

extern "C"
{

CPULSAR_API const char* CPULSAR_CALL CPulsar_RuntimeState_ToString(CPulsar_RuntimeState runtimeState)
{
    return RuntimeStateToString(CRuntimeStateToCppRuntimeState(runtimeState));
}

CPULSAR_API CPulsar_Module* CPULSAR_CALL CPulsar_Module_Create(void)
{
    return CPULSAR_WRAP(*PULSAR_NEW(Pulsar::Module));
}

CPULSAR_API void CPULSAR_CALL CPulsar_Module_Delete(CPulsar_Module* _self)
{
    PULSAR_DELETE(Pulsar::Module, &CPULSAR_UNWRAP(_self));
}

CPULSAR_API size_t CPULSAR_CALL CPulsar_Module_BindNativeFunction(
        CPulsar_Module* _self, CPulsar_FunctionSignature fnSig,
        CPulsar_NativeFunction nativeFn, CPulsar_CBuffer _nativeFnArgs)
{
    auto nativeFnArgs = Pulsar::SharedRef<CBufferWrapper>::New(_nativeFnArgs);
    return CPULSAR_UNWRAP(_self)
        .BindNativeFunction(Pulsar::FunctionSignature{
            .Name = fnSig.Name,
            .Arity = fnSig.Arity,
            .Returns = fnSig.Returns,
            .StackArity = fnSig.StackArity,
        }, [nativeFn, nativeFnArgs](Pulsar::ExecutionContext& context) {
            return CRuntimeStateToCppRuntimeState(nativeFn(
                CPULSAR_WRAP(context),
                nativeFnArgs->GetBuffer().Data
            ));
        });
}

CPULSAR_API int64_t CPULSAR_CALL CPulsar_Module_DeclareAndBindNativeFunction(
        CPulsar_Module* _self, CPulsar_FunctionSignature fnSig,
        CPulsar_NativeFunction nativeFn, CPulsar_CBuffer _nativeFnArgs)
{
    auto nativeFnArgs = Pulsar::SharedRef<CBufferWrapper>::New(_nativeFnArgs);
    return CPULSAR_UNWRAP(_self)
        .DeclareAndBindNativeFunction(Pulsar::FunctionSignature{
            .Name = fnSig.Name,
            .Arity = fnSig.Arity,
            .Returns = fnSig.Returns,
            .StackArity = fnSig.StackArity,
        }, [nativeFn, nativeFnArgs](Pulsar::ExecutionContext& context) {
            return CRuntimeStateToCppRuntimeState(nativeFn(
                CPULSAR_WRAP(context),
                nativeFnArgs->GetBuffer().Data
            ));
        });
}

CPULSAR_API void CPULSAR_CALL CPulsar_Module_BindNativeFunctionEx(
        CPulsar_Module* self, CPulsar_FunctionSignature fnSig,
        CPulsar_NativeFunction nativeFn, CPulsar_CBuffer nativeFnArgs,
        int declareAndBind)
{
    if (declareAndBind) {
        CPulsar_Module_DeclareAndBindNativeFunction(self, fnSig, nativeFn, nativeFnArgs);
    } else {
        CPulsar_Module_BindNativeFunction(self, fnSig, nativeFn, nativeFnArgs);
    }
}

CPULSAR_API uint64_t CPULSAR_CALL CPulsar_Module_BindCustomType(CPulsar_Module* _self, const char* typeName, CPulsar_CustomType_DataFactoryFn dataFactory)
{
    return CPULSAR_UNWRAP(_self)
        .BindCustomType(
            typeName,
            [dataFactory]() {
                CPulsar_CustomTypeGlobalData_Ref* cref = dataFactory();
                Pulsar::CustomTypeGlobalData::Ref ref = CPULSAR_UNWRAP(cref);
                CPulsar_CustomTypeGlobalData_Ref_Delete(cref);
                return ref;
            }
        );
}

CPULSAR_API uint64_t CPULSAR_CALL CPulsar_Module_FindCustomType(const CPulsar_Module* _self, const char* typeName)
{
    const Pulsar::Module& self = CPULSAR_UNWRAP(_self);
    uint64_t typeId = 0;
    // HACK: There should be a method to do this. It's being done enough times to warrant it.
    self.CustomTypes.ForEach([typeName, &typeId](const auto& bucket) {
        if (!typeId && bucket.Value().Name == typeName) {
            typeId = bucket.Key();
        }
    });
    return typeId;
}

CPULSAR_API CPulsar_Locals* CPULSAR_CALL CPulsar_Frame_GetLocals(CPulsar_Frame* _self)
{
    return CPULSAR_WRAP(CPULSAR_UNWRAP(_self).Locals);
}

CPULSAR_API CPulsar_Stack* CPULSAR_CALL CPulsar_Frame_GetStack(CPulsar_Frame* _self)
{
    return CPULSAR_WRAP(CPULSAR_UNWRAP(_self).Stack);
}

CPULSAR_API CPulsar_ExecutionContext* CPULSAR_CALL CPulsar_ExecutionContext_Create(const CPulsar_Module* _module)
{
    return CPULSAR_WRAP(*PULSAR_NEW(Pulsar::ExecutionContext, CPULSAR_UNWRAP(_module)));
}

CPULSAR_API void CPULSAR_CALL CPulsar_ExecutionContext_Delete(CPulsar_ExecutionContext* _self)
{
    PULSAR_DELETE(Pulsar::ExecutionContext, &CPULSAR_UNWRAP(_self));
}

CPULSAR_API CPulsar_CustomTypeGlobalData_Ref* CPULSAR_CALL CPulsar_ExecutionContext_GetCustomTypeGlobalData(CPulsar_ExecutionContext* _self, uint64_t typeId)
{
    Pulsar::ExecutionContext& self = CPULSAR_UNWRAP(_self);
    auto typeData = self.GetCustomTypeGlobalData<Pulsar::CustomTypeGlobalData>(typeId);
    if (!typeData) return NULL;
    return CPULSAR_WRAP(*PULSAR_NEW(Pulsar::CustomTypeGlobalData::Ref, typeData));
}

CPULSAR_API CPulsar_CBuffer* CPULSAR_CALL CPulsar_ExecutionContext_GetCustomTypeGlobalDataBuffer(CPulsar_ExecutionContext* self, uint64_t typeId)
{
    CPulsar_CustomTypeGlobalData_Ref* ref = CPulsar_ExecutionContext_GetCustomTypeGlobalData(self, typeId);
    CPulsar_CBuffer* buf = CPulsar_CustomTypeGlobalData_Ref_GetBuffer(ref);
    CPulsar_CustomTypeGlobalData_Ref_Delete(ref);
    return buf;
}

CPULSAR_API CPulsar_Stack* CPULSAR_CALL CPulsar_ExecutionContext_GetStack(CPulsar_ExecutionContext* _self)
{
    return CPULSAR_WRAP(CPULSAR_UNWRAP(_self).GetStack());
}

CPULSAR_API CPulsar_Frame* CPULSAR_CALL CPulsar_ExecutionContext_CurrentFrame(CPulsar_ExecutionContext* _self)
{
    return CPULSAR_WRAP(CPULSAR_UNWRAP(_self).CurrentFrame());
}

CPULSAR_API CPulsar_RuntimeState CPULSAR_CALL CPulsar_ExecutionContext_CallFunctionByName(CPulsar_ExecutionContext* _self, const char* fnName)
{
    return (CPulsar_RuntimeState)CPULSAR_UNWRAP(_self).CallFunction(fnName);
}

CPULSAR_API CPulsar_RuntimeState CPULSAR_CALL CPulsar_ExecutionContext_Run(CPulsar_ExecutionContext* _self)
{
    return (CPulsar_RuntimeState)CPULSAR_UNWRAP(_self).Run();
}

CPULSAR_API CPulsar_RuntimeState CPULSAR_CALL CPulsar_ExecutionContext_GetState(const CPulsar_ExecutionContext* _self)
{
    return (CPulsar_RuntimeState)CPULSAR_UNWRAP(_self).GetState();
}

}

static Pulsar::RuntimeState CRuntimeStateToCppRuntimeState(CPulsar_RuntimeState cstate)
{
    switch (cstate) {
    case CPulsar_RuntimeState_OK:
        return Pulsar::RuntimeState::OK;
    case CPulsar_RuntimeState_Error:
        return Pulsar::RuntimeState::Error;
    case CPulsar_RuntimeState_TypeError:
        return Pulsar::RuntimeState::TypeError;
    case CPulsar_RuntimeState_StackOverflow:
        return Pulsar::RuntimeState::StackOverflow;
    case CPulsar_RuntimeState_StackUnderflow:
        return Pulsar::RuntimeState::StackUnderflow;
    case CPulsar_RuntimeState_OutOfBoundsConstantIndex:
        return Pulsar::RuntimeState::OutOfBoundsConstantIndex;
    case CPulsar_RuntimeState_OutOfBoundsLocalIndex:
        return Pulsar::RuntimeState::OutOfBoundsLocalIndex;
    case CPulsar_RuntimeState_OutOfBoundsGlobalIndex:
        return Pulsar::RuntimeState::OutOfBoundsGlobalIndex;
    case CPulsar_RuntimeState_WritingOnConstantGlobal:
        return Pulsar::RuntimeState::WritingOnConstantGlobal;
    case CPulsar_RuntimeState_OutOfBoundsFunctionIndex:
        return Pulsar::RuntimeState::OutOfBoundsFunctionIndex;
    case CPulsar_RuntimeState_CallStackUnderflow:
        return Pulsar::RuntimeState::CallStackUnderflow;
    case CPulsar_RuntimeState_NativeFunctionBindingsMismatch:
        return Pulsar::RuntimeState::NativeFunctionBindingsMismatch;
    case CPulsar_RuntimeState_UnboundNativeFunction:
        return Pulsar::RuntimeState::UnboundNativeFunction;
    case CPulsar_RuntimeState_FunctionNotFound:
        return Pulsar::RuntimeState::FunctionNotFound;
    case CPulsar_RuntimeState_ListIndexOutOfBounds:
        return Pulsar::RuntimeState::ListIndexOutOfBounds;
    case CPulsar_RuntimeState_StringIndexOutOfBounds:
        return Pulsar::RuntimeState::StringIndexOutOfBounds;
    case CPulsar_RuntimeState_NoCustomTypeGlobalData:
        return Pulsar::RuntimeState::NoCustomTypeGlobalData;
    case CPulsar_RuntimeState_InvalidCustomTypeHandle:
        return Pulsar::RuntimeState::InvalidCustomTypeHandle;
    case CPulsar_RuntimeState_InvalidCustomTypeReference:
        return Pulsar::RuntimeState::InvalidCustomTypeReference;
    default:
        return Pulsar::RuntimeState::Error;
    }
}
