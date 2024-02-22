#include "pulsar/runtime.h"

#include <memory>

size_t Pulsar::Module::BindNativeFunction(const FunctionDefinition& def, NativeFunction func)
{
    if (NativeFunctions.Size() != NativeBindings.Size())
        return 0;
    size_t bound = 0;
    for (size_t i = 0; i < NativeBindings.Size(); i++) {
        const FunctionDefinition& nDef = NativeBindings[i];
        if (!nDef.MatchesDeclaration(def))
            continue;
        bound++;
        NativeFunctions[i] = func;
    }
    return bound;
}

Pulsar::RuntimeState Pulsar::Module::CallFunctionByName(const String& name, ValueStack& stack, ExecutionContext& context) const
{
    for (int64_t i = Functions.Size()-1; i >= 0; i--) {
        const FunctionDefinition& other = Functions[i];
        if (other.Name != name)
            continue;
        return CallFunction(i, stack, context);
    }
    return RuntimeState::FunctionNotFound;
}

Pulsar::RuntimeState Pulsar::Module::CallFunctionByDefinition(const FunctionDefinition& def, ValueStack& stack, ExecutionContext& context) const
{
    for (int64_t i = Functions.Size()-1; i >= 0; i--) {
        const FunctionDefinition& other = Functions[i];
        if (!other.MatchesDeclaration(def))
            continue;
        return CallFunction(i, stack, context);
    }
    return RuntimeState::FunctionNotFound;
}

Pulsar::RuntimeState Pulsar::Module::CallFunction(int64_t funcIdx, ValueStack& stack, ExecutionContext& context) const
{
    if (funcIdx < 0 || (size_t)funcIdx >= Functions.Size())
        return RuntimeState::OutOfBoundsFunctionIndex;

    { // Create Frame
        context.CallStack.EmplaceBack(&Functions[funcIdx]);
        Frame& thisFrame = context.CallStack[0];
        auto res = PrepareCallFrame(stack, thisFrame);
        if (res != RuntimeState::OK)
            return res;
    }

    for (;;) {
        for (;;) {
            Frame& frame = context.CallStack.CurrentFrame();
            if (frame.InstructionIndex >= frame.Function->Code.Size())
                break;
            auto state = ExecuteInstruction(frame, context);
            if (state != RuntimeState::OK)
                return state;
        }

        if (context.IsAtEnd())
            break;
        if (!context.CallStack.HasCaller())
            return RuntimeState::CallStackUnderflow;
        Frame& callingFrame = context.CallStack.CallingFrame();

        Frame& retFrame = context.CallStack.CurrentFrame();
        if (retFrame.Stack.Size() < retFrame.Function->Returns)
            return RuntimeState::StackUnderflow;
        
        for (size_t i = 0; i < retFrame.Function->Returns; i++) {
            callingFrame.Stack.PushBack(std::move(
                retFrame.Stack[retFrame.Stack.Size()-retFrame.Function->Returns+i]
            ));
        }
        context.CallStack.PopBack();
    }

    if (context.CallStack.Size() == 0)
        return RuntimeState::CallStackUnderflow;
    { // Move Return Values
        Frame& thisFrame = context.CallStack[0];
        if (thisFrame.Stack.Size() < thisFrame.Function->Returns)
            return RuntimeState::StackUnderflow;
        
        for (size_t i = 0; i < thisFrame.Function->Returns; i++) {
            stack.PushBack(std::move(
                thisFrame.Stack[thisFrame.Stack.Size()-thisFrame.Function->Returns+i]
            ));
        }
    }
    return RuntimeState::OK;
}

Pulsar::RuntimeState Pulsar::Module::PrepareCallFrame(ValueStack& callerStack, Frame& callingFrame) const
{
    if (callerStack.Size() < callingFrame.Function->Arity)
        return RuntimeState::StackUnderflow;
    callingFrame.Locals.Resize(callingFrame.Function->LocalsCount);
    for (size_t i = 0; i < callingFrame.Function->Arity; i++) {
        callingFrame.Locals[callingFrame.Function->Arity-i-1] = std::move(callerStack.Back());
        callerStack.PopBack();
    }
    return RuntimeState::OK;
}

Pulsar::RuntimeState Pulsar::Module::ExecuteInstruction(Frame& frame, ExecutionContext& eContext) const
{
    const Instruction& instr = frame.Function->Code[frame.InstructionIndex++];
    switch (instr.Code) {
    case InstructionCode::PushInt:
        frame.Stack.EmplaceBack().SetInteger(instr.Arg0);
        break;
    case InstructionCode::PushDbl: {
        static_assert(sizeof(double) == sizeof(int64_t));
        void* arg0 = (void*)&instr.Arg0;
        double val = *(double*)arg0;
        frame.Stack.EmplaceBack().SetDouble(val);
        // Don't want to rely on std::bit_cast since my g++ does not have it.
    } break;
    case InstructionCode::PushFunctionReference:
        frame.Stack.EmplaceBack().SetFunctionReference(instr.Arg0);
        break;
    case InstructionCode::PushNativeFunctionReference:
        frame.Stack.EmplaceBack().SetNativeFunctionReference(instr.Arg0);
        break;
    case InstructionCode::PushConst:
        if (instr.Arg0 < 0 || (size_t)instr.Arg0 >= eContext.OwnerModule->Constants.Size())
            return RuntimeState::OutOfBoundsConstantIndex;
        frame.Stack.EmplaceBack(eContext.OwnerModule->Constants[(size_t)instr.Arg0]);
        break;
    case InstructionCode::PushLocal:
        if (instr.Arg0 < 0 || (size_t)instr.Arg0 >= frame.Locals.Size())
            return RuntimeState::OutOfBoundsLocalIndex;
        frame.Stack.PushBack(frame.Locals[instr.Arg0]);
        break;
    case InstructionCode::MoveLocal:
        if (instr.Arg0 < 0 || (size_t)instr.Arg0 >= frame.Locals.Size())
            return RuntimeState::OutOfBoundsLocalIndex;
        frame.Stack.PushBack(std::move(frame.Locals[instr.Arg0]));
        break;
    case InstructionCode::PopIntoLocal: {
        if (frame.Stack.Size() < 1)
            return RuntimeState::StackUnderflow;
        if (instr.Arg0 < 0 || (size_t)instr.Arg0 >= frame.Locals.Size())
            return RuntimeState::OutOfBoundsLocalIndex;
        frame.Locals[instr.Arg0] = std::move(frame.Stack.Back());
        frame.Stack.PopBack();
    } break;
    case InstructionCode::CopyIntoLocal:
        if (frame.Stack.Size() < 1)
            return RuntimeState::StackUnderflow;
        if (instr.Arg0 < 0 || (size_t)instr.Arg0 >= frame.Locals.Size())
            return RuntimeState::OutOfBoundsLocalIndex;
        frame.Locals[instr.Arg0] = frame.Stack.Back();
        break;
    case InstructionCode::Call: {
        int64_t funcIdx = instr.Arg0;
        if (funcIdx < 0 || (size_t)funcIdx >= Functions.Size())
            return RuntimeState::OutOfBoundsFunctionIndex;

        Frame callFrame{ &Functions[funcIdx] };
        auto res = PrepareCallFrame(frame.Stack, callFrame);
        if (res != RuntimeState::OK)
            return res;
        eContext.CallStack.PushBack(std::move(callFrame));
    } break;
    case InstructionCode::CallNative: {
        if (NativeBindings.Size() != NativeFunctions.Size())
            return RuntimeState::NativeFunctionBindingsMismatch;
        int64_t funcIdx = instr.Arg0;
        if (funcIdx < 0 || (size_t)funcIdx >= NativeBindings.Size())
            return RuntimeState::OutOfBoundsFunctionIndex;
        if (!NativeFunctions[funcIdx])
            return RuntimeState::UnboundNativeFunction;

        Frame callFrame{ &NativeBindings[funcIdx] };
        auto res = PrepareCallFrame(frame.Stack, callFrame);
        if (res != RuntimeState::OK)
            return res;
        eContext.CallStack.PushBack(std::move(callFrame));
        return NativeFunctions[funcIdx](eContext);
    }
    case InstructionCode::Return:
        frame.InstructionIndex = frame.Function->Code.Size();
        break;
    case InstructionCode::ICall: {
        if (frame.Stack.Size() < 1)
            return RuntimeState::StackUnderflow;
        Value funcIdxValue = std::move(frame.Stack.Back());
        frame.Stack.PopBack();
        if (funcIdxValue.Type() == ValueType::FunctionReference) {
            int64_t funcIdx = funcIdxValue.AsInteger();
            if (funcIdx < 0 || (size_t)funcIdx >= Functions.Size())
                return RuntimeState::OutOfBoundsFunctionIndex;
            Frame callFrame{ &Functions[funcIdx] };
            auto res = PrepareCallFrame(frame.Stack, callFrame);
            if (res != RuntimeState::OK)
                return res;
            eContext.CallStack.PushBack(std::move(callFrame));
            break;
        } else if (funcIdxValue.Type() == ValueType::NativeFunctionReference) {
            if (NativeBindings.Size() != NativeFunctions.Size())
                return RuntimeState::NativeFunctionBindingsMismatch;
            int64_t funcIdx = funcIdxValue.AsInteger();
            if (funcIdx < 0 || (size_t)funcIdx >= NativeBindings.Size())
                return RuntimeState::OutOfBoundsFunctionIndex;
            if (!NativeFunctions[funcIdx])
                return RuntimeState::UnboundNativeFunction;

            Frame callFrame{ &NativeBindings[funcIdx] };
            auto res = PrepareCallFrame(frame.Stack, callFrame);
            if (res != RuntimeState::OK)
                return res;
            eContext.CallStack.PushBack(std::move(callFrame));
            return NativeFunctions[funcIdx](eContext);
        }
        return RuntimeState::TypeError;
    }
    case InstructionCode::DynSum: {
        if (frame.Stack.Size() < 2)
            return RuntimeState::StackUnderflow;
        Value b = std::move(frame.Stack.Back());
        frame.Stack.PopBack();
        Value& a = frame.Stack.Back();
        if (!IsNumericValueType(a.Type()) || !IsNumericValueType(b.Type()))
            return RuntimeState::TypeError;
        if (a.Type() == ValueType::Double || b.Type() == ValueType::Double) {
            double aVal = a.Type() == ValueType::Double ? a.AsDouble() : (double)a.AsInteger();
            double bVal = b.Type() == ValueType::Double ? b.AsDouble() : (double)b.AsInteger();
            a.SetDouble(aVal + bVal);
        } else a.SetInteger(a.AsInteger() + b.AsInteger());
    } break;
    case InstructionCode::Compare:
    case InstructionCode::DynSub: {
        if (frame.Stack.Size() < 2)
            return RuntimeState::StackUnderflow;
        Value b = std::move(frame.Stack.Back());
        frame.Stack.PopBack();
        Value& a = frame.Stack.Back();
        if (!IsNumericValueType(a.Type()) || !IsNumericValueType(b.Type()))
            return RuntimeState::TypeError;
        if (a.Type() == ValueType::Double || b.Type() == ValueType::Double) {
            double aVal = a.Type() == ValueType::Double ? a.AsDouble() : (double)a.AsInteger();
            double bVal = b.Type() == ValueType::Double ? b.AsDouble() : (double)b.AsInteger();
            a.SetDouble(aVal - bVal);
        } else a.SetInteger(a.AsInteger() - b.AsInteger());
    } break;
    case InstructionCode::DynMul: {
        if (frame.Stack.Size() < 2)
            return RuntimeState::StackUnderflow;
        Value b = std::move(frame.Stack.Back());
        frame.Stack.PopBack();
        Value& a = frame.Stack.Back();
        if (!IsNumericValueType(a.Type()) || !IsNumericValueType(b.Type()))
            return RuntimeState::TypeError;
        if (a.Type() == ValueType::Double || b.Type() == ValueType::Double) {
            double aVal = a.Type() == ValueType::Double ? a.AsDouble() : (double)a.AsInteger();
            double bVal = b.Type() == ValueType::Double ? b.AsDouble() : (double)b.AsInteger();
            a.SetDouble(aVal * bVal);
        } else a.SetInteger(a.AsInteger() * b.AsInteger());
    } break;
    case InstructionCode::DynDiv: {
        if (frame.Stack.Size() < 2)
            return RuntimeState::StackUnderflow;
        Value b = std::move(frame.Stack.Back());
        frame.Stack.PopBack();
        Value& a = frame.Stack.Back();
        if (!IsNumericValueType(a.Type()) || !IsNumericValueType(b.Type()))
            return RuntimeState::TypeError;
        if (a.Type() == ValueType::Double || b.Type() == ValueType::Double) {
            double aVal = a.Type() == ValueType::Double ? a.AsDouble() : (double)a.AsInteger();
            double bVal = b.Type() == ValueType::Double ? b.AsDouble() : (double)b.AsInteger();
            a.SetDouble(aVal / bVal);
        } else a.SetInteger(a.AsInteger() / b.AsInteger());
    } break;
    case InstructionCode::Mod: {
        if (frame.Stack.Size() < 2)
            return RuntimeState::StackUnderflow;
        Value b = std::move(frame.Stack.Back());
        frame.Stack.PopBack();
        Value& a = frame.Stack.Back();
        if (a.Type() != ValueType::Integer || b.Type() != ValueType::Integer)
            return RuntimeState::TypeError;
        a.SetInteger(a.AsInteger() % b.AsInteger());
    } break;
    case InstructionCode::Jump:
        --frame.InstructionIndex += instr.Arg0;
        break;
    case InstructionCode::JumpIfZero:
    case InstructionCode::JumpIfNotZero:
    case InstructionCode::JumpIfGreaterThanZero:
    case InstructionCode::JumpIfGreaterThanOrEqualToZero:
    case InstructionCode::JumpIfLessThanZero:
    case InstructionCode::JumpIfLessThanOrEqualToZero: {
        if (frame.Stack.Size() < 1)
            return RuntimeState::StackUnderflow;
        Value truthValue = std::move(frame.Stack.Back());
        if (!IsNumericValueType(truthValue.Type()))
            return RuntimeState::TypeError;
        frame.Stack.PopBack();
        // TODO: Check for bounds
        if (truthValue.Type() == ValueType::Double) {
            if (ShouldJump(instr.Code, truthValue.AsDouble()))
                --frame.InstructionIndex += instr.Arg0;
        } else if (ShouldJump(instr.Code, truthValue.AsInteger()))
            --frame.InstructionIndex += instr.Arg0;
    } break;
    case InstructionCode::Length: {
        if (frame.Stack.Size() < 1)
            return RuntimeState::StackUnderflow;
        const Value& list = frame.Stack.Back();
        if (list.Type() == ValueType::List) {
            Value len;
            len.SetInteger((int64_t)list.AsList().Length());
            frame.Stack.EmplaceBack(std::move(len));
        } else if (list.Type() == ValueType::String) {
            Value len;
            len.SetInteger((int64_t)list.AsString().Length());
            frame.Stack.EmplaceBack(std::move(len));
        } else return RuntimeState::TypeError;
    } break;
    case InstructionCode::PushEmptyList:
        frame.Stack.EmplaceBack().SetList(ValueList());
        break;
    case InstructionCode::Prepend: {
        if (frame.Stack.Size() < 2)
            return RuntimeState::StackUnderflow;
        Value toPrepend = std::move(frame.Stack.Back());
        frame.Stack.PopBack();
        Value& list = frame.Stack.Back();
        if (list.Type() == ValueType::List) {
            list.AsList().Prepend()->Value() = std::move(toPrepend);
        } else if (list.Type() == ValueType::String) {
            if (toPrepend.Type() != ValueType::String)
                return RuntimeState::TypeError;
            toPrepend.AsString() += list.AsString();
            list.AsString() = std::move(toPrepend.AsString());
        } else return RuntimeState::TypeError;
    } break;
    case InstructionCode::Append: {
        if (frame.Stack.Size() < 2)
            return RuntimeState::StackUnderflow;
        Value toAppend = std::move(frame.Stack.Back());
        frame.Stack.PopBack();
        Value& list = frame.Stack.Back();
        if (list.Type() == ValueType::List) {
            list.AsList().Append()->Value() = std::move(toAppend);
        } else if (list.Type() == ValueType::String) {
            if (toAppend.Type() != ValueType::String)
                return RuntimeState::TypeError;
            list.AsString() += toAppend.AsString();
        } else return RuntimeState::TypeError;
    } break;
    case InstructionCode::Concat: {
        if (frame.Stack.Size() < 2)
            return RuntimeState::StackUnderflow;
        Value toConcat = std::move(frame.Stack.Back());
        frame.Stack.PopBack();
        Value& list = frame.Stack.Back();
        if (toConcat.Type() == ValueType::List && list.Type() == ValueType::List) {
            list.AsList().Concat(toConcat.AsList());
        } else return RuntimeState::TypeError;
    } break;
    case InstructionCode::Head: {
        if (frame.Stack.Size() < 1)
            return RuntimeState::StackUnderflow;
        Value& list = frame.Stack.Back();
        if (list.Type() == ValueType::List) {
            ValueList::NodeType* node = list.AsList().Front();
            if (!node) return RuntimeState::ListIndexOutOfBounds;
            Value val(std::move(node->Value()));
            list.AsList().RemoveFront(1);
            frame.Stack.PushBack(std::move(val));
        } else return RuntimeState::TypeError;
    } break;
    case InstructionCode::Tail: {
        if (frame.Stack.Size() < 1)
            return RuntimeState::StackUnderflow;
        Value& list = frame.Stack.Back();
        if (list.Type() == ValueType::List) {
            list.AsList().RemoveFront(1);
        } else return RuntimeState::TypeError;
    } break;
    }

    return RuntimeState::OK;
}

const char* Pulsar::RuntimeStateToString(RuntimeState rstate)
{
    switch (rstate) {
    case RuntimeState::OK:
        return "OK";
    case RuntimeState::Error:
        return "Error";
    case RuntimeState::TypeError:
        return "TypeError";
    case RuntimeState::StackOverflow:
        return "StackOverflow";
    case RuntimeState::StackUnderflow:
        return "StackUnderflow";
    case RuntimeState::OutOfBoundsConstantIndex:
        return "OutOfBoundsConstantIndex";
    case RuntimeState::OutOfBoundsLocalIndex:
        return "OutOfBoundsLocalIndex";
    case RuntimeState::OutOfBoundsFunctionIndex:
        return "OutOfBoundsFunctionIndex";
    case RuntimeState::CallStackUnderflow:
        return "CallStackUnderflow";
    case RuntimeState::NativeFunctionBindingsMismatch:
        return "NativeFunctionBindingsMismatch";
    case RuntimeState::UnboundNativeFunction:
        return "UnboundNativeFunction";
    case RuntimeState::FunctionNotFound:
        return "FunctionNotFound";
    case RuntimeState::ListIndexOutOfBounds:
        return "ListIndexOutOfBounds";
    }
    return "Unknown";
}
