#define _CPULSAR_IMPLEMENTATION
#include "cpulsar/cbuffer.h"
#include "cpulsar/_opaque.hpp"

#include "pulsar/structures/ref.h"

extern "C"
{

CPULSAR_API bool CPULSAR_CALL CPulsar_CBuffer_TryCopy(CPulsar_CBuffer self, CPulsar_CBuffer* out)
{
    if (self.Copy == NULL) return false;
    if (out == NULL) return true;
    CPulsar_CBuffer_Free(out);
    *out = self;
    out->Data = self.Copy(self.Data);
    return true;
}

CPULSAR_API CPulsar_CBuffer CPULSAR_CALL CPulsar_CBuffer_Copy(CPulsar_CBuffer self)
{
    CPulsar_CBuffer buffer = CPULSAR_CBUFFER_NULL;
    CPulsar_CBuffer_TryCopy(self, &buffer);
    return buffer;
}

CPULSAR_API void CPULSAR_CALL CPulsar_CBuffer_Free(CPulsar_CBuffer* self)
{
    if (self == NULL || self->Free == NULL) return;
    self->Free(self->Data);
    *self = CPULSAR_CBUFFER_NULL;
}

CPULSAR_API CPulsar_CBuffer_Ref* CPULSAR_CALL CPulsar_CBuffer_Ref_Create(CPulsar_CBuffer buffer)
{
    auto bufferOwner = PULSAR_NEW(Pulsar::SharedRef<CPulsar::CBufferOwner>);
    *bufferOwner = Pulsar::SharedRef<CPulsar::CBufferOwner>::New(buffer);
    return CPULSAR_WRAP(*bufferOwner);
}

CPULSAR_API void CPULSAR_CALL CPulsar_CBuffer_Ref_Delete(CPulsar_CBuffer_Ref* self)
{
    PULSAR_DELETE(Pulsar::SharedRef<CPulsar::CBufferOwner>, &CPULSAR_UNWRAP(self));
}

CPULSAR_API CPulsar_CBuffer_Ref* CPULSAR_CALL CPulsar_CBuffer_Ref_Copy(CPulsar_CBuffer_Ref* self)
{
    auto copy = PULSAR_NEW(Pulsar::SharedRef<CPulsar::CBufferOwner>);
    *copy = CPULSAR_UNWRAP(self);
    return CPULSAR_WRAP(*copy);
}

CPULSAR_API CPulsar_CBuffer* CPULSAR_CALL CPulsar_CBuffer_Ref_GetBuffer(CPulsar_CBuffer_Ref* self)
{
    return &CPULSAR_UNWRAP(self)->Get();
}

}
