#define _CPULSAR_IMPLEMENTATION
#include "cpulsar/runtime/customtype.h"
#include "cpulsar/_opaque.hpp"

#include "pulsar/runtime.h"

class CustomTypeGlobalDataBuffer final :
    public Pulsar::CustomTypeGlobalData
{
public:
    CustomTypeGlobalDataBuffer(CPulsar_CBuffer buffer) :
        m_Buffer(buffer)
    {}

    ~CustomTypeGlobalDataBuffer()
    {
        if (m_Buffer.Free) {
            m_Buffer.Free(m_Buffer.Data);
        }
    }

    Ref Fork() const override
    {
        if (!m_Buffer.Copy)
            return nullptr;
        return Pulsar::SharedRef<CustomTypeGlobalDataBuffer>::New(CPulsar_CBuffer{
            .Data = m_Buffer.Copy(m_Buffer.Data),
            .Free = m_Buffer.Free,
            .Copy = m_Buffer.Copy,
        });
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
