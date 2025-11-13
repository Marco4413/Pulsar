#ifndef __CPULSAR__OPAQUE_HPP
#define __CPULSAR__OPAQUE_HPP

#include "cpulsar/core.h"
#include "cpulsar/opaque.h"

// This file is to be used within the cpulsar implementation to wrap and unwrap opaque types.

#ifndef _CPULSAR_IMPLEMENTATION
#  error Included "cpulsar/_opaque.hpp" in non-cpulsar implementation file.
#endif // _CPULSAR_IMPLEMENTATION

#ifndef CPULSAR_CPP
#  error Included "cpulsar/_opaque.hpp" in non-C++ file.
#endif // CPULSAR_CPP

// Forward declarations for Pulsar

namespace Pulsar
{
    template<typename T>
    class List;

    template<typename T>
    class LinkedList;

    template<typename T>
    class SharedRef;

    class Parser;
    class Module;

    class  Value;
    struct CustomData;
    class  CustomDataHolder;
    class  CustomTypeGlobalData;

    class  Stack;
    struct Frame;
    class  ExecutionContext;
}

#define CPULSAR_UNWRAP_IMPL(TWrapped, TUnwrapped) \
    inline const TUnwrapped& Unwrap(const TWrapped* wrapped) { return *reinterpret_cast<const TUnwrapped*>(wrapped); } \
    inline TUnwrapped& Unwrap(TWrapped* wrapped) { return *reinterpret_cast<TUnwrapped*>(wrapped); }

#define CPULSAR_WRAP_IMPL(TWrapped, TUnwrapped) \
    inline const TWrapped* Wrap(const TUnwrapped& unwrapped) { return reinterpret_cast<const TWrapped*>(&unwrapped); } \
    inline TWrapped* Wrap(TUnwrapped& unwrapped) { return reinterpret_cast<TWrapped*>(&unwrapped); }

#define CPULSAR_OPAQUE_IMPL(TWrapped, TUnwrapped) \
    CPULSAR_UNWRAP_IMPL(TWrapped, TUnwrapped) \
    CPULSAR_WRAP_IMPL(TWrapped, TUnwrapped)

#define CPULSAR_UNWRAP(value) CPulsar::Opaque::Unwrap((value))
#define CPULSAR_WRAP(value)   CPulsar::Opaque::Wrap((value))

namespace CPulsar::Opaque
{
    CPULSAR_OPAQUE_IMPL(CPulsar_Parser,           Pulsar::Parser)
    CPULSAR_OPAQUE_IMPL(CPulsar_Module,           Pulsar::Module)
    CPULSAR_OPAQUE_IMPL(CPulsar_Locals,           Pulsar::List<Pulsar::Value>)
    CPULSAR_OPAQUE_IMPL(CPulsar_Value,            Pulsar::Value)
    CPULSAR_OPAQUE_IMPL(CPulsar_ValueList,        Pulsar::LinkedList<Pulsar::Value>)
    CPULSAR_OPAQUE_IMPL(CPulsar_CustomData,       Pulsar::CustomData)
    CPULSAR_OPAQUE_IMPL(CPulsar_Stack,            Pulsar::Stack)
    CPULSAR_OPAQUE_IMPL(CPulsar_Frame,            Pulsar::Frame)
    CPULSAR_OPAQUE_IMPL(CPulsar_ExecutionContext, Pulsar::ExecutionContext)

    CPULSAR_OPAQUE_IMPL(CPulsar_CustomDataHolder_Ref,     Pulsar::SharedRef<Pulsar::CustomDataHolder>)
    CPULSAR_OPAQUE_IMPL(CPulsar_CustomTypeGlobalData_Ref, Pulsar::SharedRef<Pulsar::CustomTypeGlobalData>)
}

#endif // __CPULSAR__OPAQUE_HPP
