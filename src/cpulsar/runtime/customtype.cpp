#define _CPULSAR_IMPLEMENTATION
#include "cpulsar/runtime/customtype.h"

#include "pulsar/runtime.h"

class CustomTypeDataBuffer final :
    public Pulsar::CustomTypeData
{
public:
    CustomTypeDataBuffer(CPulsar_CBuffer buffer) :
        m_Buffer(buffer)
    {}

    ~CustomTypeDataBuffer()
    {
        if (m_Buffer.Free) {
            m_Buffer.Free(m_Buffer.Data);
        }
    }

    CPulsar_CBuffer& GetBuffer() { return m_Buffer; }

private:
    CPulsar_CBuffer m_Buffer;
};

class CustomDataHolderBuffer final :
    public Pulsar::CustomDataHolder
{
public:
    CustomDataHolderBuffer(CPulsar_CBuffer buffer) :
        m_Buffer(buffer)
    {}

    ~CustomDataHolderBuffer()
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

CPULSAR_API CPulsar_CustomTypeData_Ref CPulsar_CustomTypeData_Ref_FromBuffer(CPulsar_CBuffer buffer)
{
    auto ref = Pulsar::SharedRef<CustomTypeDataBuffer>::New(buffer);
    return CPULSAR_REF(CPulsar_CustomTypeData_Ref_S, *PULSAR_NEW(Pulsar::CustomTypeData::Ref, ref));
}

CPULSAR_API CPulsar_CBuffer* CPulsar_CustomTypeData_Ref_GetBuffer(CPulsar_CustomTypeData_Ref _self)
{
    auto bufferData = CPULSAR_DEREF(Pulsar::CustomTypeData::Ref, _self).CastTo<CustomTypeDataBuffer>();
    return bufferData ? &bufferData->GetBuffer() : NULL;
}

CPULSAR_API void CPulsar_CustomTypeData_Ref_Delete(CPulsar_CustomTypeData_Ref _self)
{
    PULSAR_DELETE(Pulsar::CustomTypeData::Ref, &CPULSAR_DEREF(Pulsar::CustomTypeData::Ref, _self));
}

CPULSAR_API CPulsar_CustomDataHolder_Ref CPulsar_CustomDataHolder_Ref_FromBuffer(CPulsar_CBuffer buffer)
{
    auto ref = Pulsar::SharedRef<CustomDataHolderBuffer>::New(buffer);
    return CPULSAR_REF(CPulsar_CustomDataHolder_Ref_S, *PULSAR_NEW(Pulsar::CustomDataHolder::Ref, ref));
}

CPULSAR_API CPulsar_CBuffer* CPulsar_CustomDataHolder_Ref_GetBuffer(CPulsar_CustomDataHolder_Ref _self)
{
    auto bufferHolder = CPULSAR_DEREF(Pulsar::CustomDataHolder::Ref, _self).CastTo<CustomDataHolderBuffer>();
    return bufferHolder ? &bufferHolder->GetBuffer() : NULL;
}

CPULSAR_API void CPulsar_CustomDataHolder_Ref_Delete(CPulsar_CustomDataHolder_Ref _self)
{
    PULSAR_DELETE(Pulsar::CustomDataHolder::Ref, &CPULSAR_DEREF(Pulsar::CustomDataHolder::Ref, _self));
}

}
