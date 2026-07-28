#define _CPULSAR_IMPLEMENTATION
#include "cpulsar/runtime/customtype.h"
#include "cpulsar/_opaque.hpp"

#include "pulsar/runtime.h"

class CustomTypeGlobalDataBuffer final :
    public Pulsar::CustomTypeGlobalData
{
public:
    CustomTypeGlobalDataBuffer(CPulsar_CBuffer buffer)
        : m_BufferOwner(buffer) {}
    ~CustomTypeGlobalDataBuffer() = default;

    Ref Fork() const override
    {
        auto fork = Pulsar::SharedRef<CustomTypeGlobalDataBuffer>::New(CPULSAR_CBUFFER_NULL);
        if (!m_BufferOwner.TryCopy(fork->m_BufferOwner)) return nullptr;
        return fork;
    }

    CPulsar_CBuffer& GetBuffer() { return *m_BufferOwner; }

private:
    CPulsar::CBufferOwner m_BufferOwner;
};

class CustomDataHolderBuffer final :
    public Pulsar::CustomDataHolder
{
public:
    CustomDataHolderBuffer(CPulsar_CBuffer buffer)
        : m_BufferOwner(buffer) {}
    ~CustomDataHolderBuffer() = default;

    CPulsar_CBuffer& GetBuffer() { return *m_BufferOwner; }

private:
    CPulsar::CBufferOwner m_BufferOwner;
};

extern "C"
{

CPULSAR_API CPulsar_CustomTypeGlobalData_Ref* CPULSAR_CALL CPulsar_CustomTypeGlobalData_Ref_FromBuffer(CPulsar_CBuffer buffer)
{
    auto ref = Pulsar::SharedRef<CustomTypeGlobalDataBuffer>::New(buffer);
    return CPULSAR_WRAP(*PULSAR_NEW(Pulsar::CustomTypeGlobalData::Ref, ref));
}

CPULSAR_API CPulsar_CBuffer* CPULSAR_CALL CPulsar_CustomTypeGlobalData_Ref_GetBuffer(CPulsar_CustomTypeGlobalData_Ref* _self)
{
    auto bufferData = CPULSAR_UNWRAP(_self).CastTo<CustomTypeGlobalDataBuffer>();
    return bufferData ? &bufferData->GetBuffer() : NULL;
}

CPULSAR_API void CPULSAR_CALL CPulsar_CustomTypeGlobalData_Ref_Delete(CPulsar_CustomTypeGlobalData_Ref* _self)
{
    PULSAR_DELETE(Pulsar::CustomTypeGlobalData::Ref, &CPULSAR_UNWRAP(_self));
}

CPULSAR_API CPulsar_CustomDataHolder_Ref* CPULSAR_CALL CPulsar_CustomDataHolder_Ref_FromBuffer(CPulsar_CBuffer buffer)
{
    auto ref = Pulsar::SharedRef<CustomDataHolderBuffer>::New(buffer);
    return CPULSAR_WRAP(*PULSAR_NEW(Pulsar::CustomDataHolder::Ref, ref));
}

CPULSAR_API CPulsar_CBuffer* CPULSAR_CALL CPulsar_CustomDataHolder_Ref_GetBuffer(CPulsar_CustomDataHolder_Ref* _self)
{
    auto bufferHolder = CPULSAR_UNWRAP(_self).CastTo<CustomDataHolderBuffer>();
    return bufferHolder ? &bufferHolder->GetBuffer() : NULL;
}

CPULSAR_API void CPULSAR_CALL CPulsar_CustomDataHolder_Ref_Delete(CPulsar_CustomDataHolder_Ref* _self)
{
    PULSAR_DELETE(Pulsar::CustomDataHolder::Ref, &CPULSAR_UNWRAP(_self));
}

}
