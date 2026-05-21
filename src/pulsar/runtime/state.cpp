#include "pulsar/runtime/state.h"

const char* Pulsar::RuntimeStateToString(RuntimeState rstate)
{
    switch (rstate) {
    case RuntimeState::OK:                             return "OK";
    case RuntimeState::Error:                          return "Error";
    case RuntimeState::TypeError:                      return "TypeError";
    case RuntimeState::StackOverflow:                  return "StackOverflow";
    case RuntimeState::StackUnderflow:                 return "StackUnderflow";
    case RuntimeState::OutOfBoundsConstantIndex:       return "OutOfBoundsConstantIndex";
    case RuntimeState::OutOfBoundsLocalIndex:          return "OutOfBoundsLocalIndex";
    case RuntimeState::OutOfBoundsGlobalIndex:         return "OutOfBoundsGlobalIndex";
    case RuntimeState::WritingOnConstantGlobal:        return "WritingOnConstantGlobal";
    case RuntimeState::OutOfBoundsFunctionIndex:       return "OutOfBoundsFunctionIndex";
    case RuntimeState::CallStackUnderflow:             return "CallStackUnderflow";
    case RuntimeState::NativeFunctionBindingsMismatch: return "NativeFunctionBindingsMismatch";
    case RuntimeState::UnboundNativeFunction:          return "UnboundNativeFunction";
    case RuntimeState::FunctionNotFound:               return "FunctionNotFound";
    case RuntimeState::ListIndexOutOfBounds:           return "ListIndexOutOfBounds";
    case RuntimeState::StringIndexOutOfBounds:         return "StringIndexOutOfBounds";
    case RuntimeState::NoCustomTypeGlobalData:         return "NoCustomTypeGlobalData";
    case RuntimeState::InvalidCustomTypeHandle:        return "InvalidCustomTypeHandle";
    case RuntimeState::InvalidCustomTypeReference:     return "InvalidCustomTypeReference";
    }
    return "Unknown";
}
