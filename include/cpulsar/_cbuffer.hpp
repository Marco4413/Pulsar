#ifndef __CPULSAR__CBUFFER_HPP
#define __CPULSAR__CBUFFER_HPP

#include "cpulsar/core.h"
#include "cpulsar/cbuffer.h"

// This file is to be used within the cpulsar implementation.

#ifndef _CPULSAR_IMPLEMENTATION
#  error Included "cpulsar/_cbuffer.hpp" in non-cpulsar implementation file.
#endif // _CPULSAR_IMPLEMENTATION

#ifndef CPULSAR_CPP
#  error Included "cpulsar/_cbuffer.hpp" in non-C++ file.
#endif // CPULSAR_CPP

// Forward declarations for Pulsar

namespace CPulsar
{
    class CBufferOwner final
    {
    public:
        CBufferOwner(CPulsar_CBuffer buf)
            : m_Buffer(buf) {}

        ~CBufferOwner()
        {
            CPulsar_CBuffer_Free(&m_Buffer);
        }

        bool TryCopy(CBufferOwner& out) const
        {
            return CPulsar_CBuffer_TryCopy(m_Buffer, &out.m_Buffer);
        }

        CPulsar_CBuffer& Get()             { return m_Buffer; }
        const CPulsar_CBuffer& Get() const { return m_Buffer; }

        CPulsar_CBuffer* operator->()             { return &m_Buffer; }
        const CPulsar_CBuffer* operator->() const { return &m_Buffer; }

        CPulsar_CBuffer& operator*()             { return m_Buffer; }
        const CPulsar_CBuffer& operator*() const { return m_Buffer; }

    private:
        CPulsar_CBuffer m_Buffer;
    };
}

#endif // __CPULSAR__CBUFFER_HPP
