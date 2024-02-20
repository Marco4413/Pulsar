#include "pulsar/runtime.h"

#include <memory>

size_t Pulsar::Module::BindNativeFunction(const FunctionDefinition& def, NativeFunction func)
{
    if (NativeFunctions.size() != NativeBindings.size())
        return 0;
    size_t bound = 0;
    for (size_t i = 0; i < NativeBindings.size(); i++) {
        const FunctionDefinition& nDef = NativeBindings[i];
        if (!nDef.MatchesDeclaration(def))
            continue;
        bound++;
        NativeFunctions[i] = func;
    }
    return bound;
}

Pulsar::RuntimeState Pulsar::Module::CallFunctionByName(const String& name, Stack& stack, ExecutionContext& context) const
{
    for (int64_t i = Functions.size()-1; i >= 0; i--) {
        const FunctionDefinition& other = Functions[i];
        if (other.Name != name)
            continue;
        return CallFunction(i, stack, context);
    }
    return RuntimeState::FunctionNotFound;
}

Pulsar::RuntimeState Pulsar::Module::CallFunctionByDefinition(const FunctionDefinition& def, Stack& stack, ExecutionContext& context) const
{
    for (int64_t i = Functions.size()-1; i >= 0; i--) {
        const FunctionDefinition& other = Functions[i];
        if (!other.MatchesDeclaration(def))
            continue;
        return CallFunction(i, stack, context);
    }
    return RuntimeState::FunctionNotFound;
}

Pulsar::RuntimeState Pulsar::Module::CallFunction(int64_t funcIdx, Stack& stack, ExecutionContext& context) const
{
    if (funcIdx < 0 || (size_t)funcIdx >= Functions.size())
        return RuntimeState::OutOfBoundsFunctionIndex;

    { // Create Frame
        context.CallStack.emplace_back(&Functions[funcIdx]);
        Frame& thisFrame = context.CallStack[0];
        auto res = PrepareCallFrame(stack, thisFrame);
        if (res != RuntimeState::OK)
            return res;
    }

    for (;;) {
        for (;;) {
            Frame& frame = context.GetCurrentFrame();
            if (frame.InstructionIndex >= frame.Function->Code.size())
                break;
            auto state = ExecuteInstruction(frame, context);
            if (state != RuntimeState::OK)
                return state;
        }

        if (context.IsAtEnd())
            break;
        Frame* callingFrame = context.GetCallingFrame();
        if (!callingFrame)
            return RuntimeState::CallStackUnderflow;

        Frame& retFrame = context.GetCurrentFrame();
        if (retFrame.OperandStack.size() < retFrame.Function->Returns)
            return RuntimeState::StackUnderflow;
        
        for (size_t i = 0; i < retFrame.Function->Returns; i++) {
            callingFrame->OperandStack.push_back(std::move(
                retFrame.OperandStack[retFrame.OperandStack.size()-retFrame.Function->Returns+i]
            ));
        }
        context.CallStack.pop_back();
    }

    if (context.CallStack.size() == 0)
        return RuntimeState::CallStackUnderflow;
    { // Move Return Values
        Frame& thisFrame = context.CallStack[0];
        if (thisFrame.OperandStack.size() < thisFrame.Function->Returns)
            return RuntimeState::StackUnderflow;
        
        for (size_t i = 0; i < thisFrame.Function->Returns; i++) {
            stack.push_back(std::move(
                thisFrame.OperandStack[thisFrame.OperandStack.size()-thisFrame.Function->Returns+i]
            ));
        }
    }
    return RuntimeState::OK;
}

Pulsar::RuntimeState Pulsar::Module::PrepareCallFrame(Stack& callerStack, Frame& callingFrame) const
{
    if (callerStack.size() < callingFrame.Function->Arity)
        return RuntimeState::StackUnderflow;
    callingFrame.Locals.resize(callingFrame.Function->LocalsCount);
    for (size_t i = 0; i < callingFrame.Function->Arity; i++) {
        callingFrame.Locals[callingFrame.Function->Arity-i-1] = std::move(callerStack.back());
        callerStack.pop_back();
    }
    return RuntimeState::OK;
}

Pulsar::RuntimeState Pulsar::Module::ExecuteInstruction(Frame& frame, ExecutionContext& eContext) const
{
    const Instruction& instr = frame.Function->Code[frame.InstructionIndex++];
    switch (instr.Code) {
    case InstructionCode::PushInt:
        frame.OperandStack.emplace_back().SetInteger(instr.Arg0);
        break;
    case InstructionCode::PushDbl:
        frame.OperandStack.emplace_back().SetDouble(*(double*)&instr.Arg0);
        break;
    case InstructionCode::PushFunctionReference:
        frame.OperandStack.emplace_back().SetFunctionReference(instr.Arg0);
        break;
    case InstructionCode::PushNativeFunctionReference:
        frame.OperandStack.emplace_back().SetNativeFunctionReference(instr.Arg0);
        break;
    case InstructionCode::PushLocal:
        if (instr.Arg0 < 0 || (size_t)instr.Arg0 >= frame.Locals.size())
            return RuntimeState::OutOfBoundsLocalIndex;
        frame.OperandStack.push_back(frame.Locals[instr.Arg0]);
        break;
    case InstructionCode::MoveLocal:
        if (instr.Arg0 < 0 || (size_t)instr.Arg0 >= frame.Locals.size())
            return RuntimeState::OutOfBoundsLocalIndex;
        frame.OperandStack.push_back(std::move(frame.Locals[instr.Arg0]));
        break;
    case InstructionCode::PopIntoLocal: {
        if (frame.OperandStack.size() < 1)
            return RuntimeState::StackUnderflow;
        if (instr.Arg0 < 0 || (size_t)instr.Arg0 >= frame.Locals.size())
            return RuntimeState::OutOfBoundsLocalIndex;
        frame.Locals[instr.Arg0] = std::move(frame.OperandStack.back());
        frame.OperandStack.pop_back();
    } break;
    case InstructionCode::CopyIntoLocal:
        if (frame.OperandStack.size() < 1)
            return RuntimeState::StackUnderflow;
        if (instr.Arg0 < 0 || (size_t)instr.Arg0 >= frame.Locals.size())
            return RuntimeState::OutOfBoundsLocalIndex;
        frame.Locals[instr.Arg0] = frame.OperandStack.back();
        break;
    case InstructionCode::Call: {
        int64_t funcIdx = instr.Arg0;
        if (funcIdx < 0 || (size_t)funcIdx >= Functions.size())
            return RuntimeState::OutOfBoundsFunctionIndex;

        Frame callFrame{ &Functions[funcIdx] };
        auto res = PrepareCallFrame(frame.OperandStack, callFrame);
        if (res != RuntimeState::OK)
            return res;
        eContext.CallStack.push_back(std::move(callFrame));
    } break;
    case InstructionCode::CallNative: {
        if (NativeBindings.size() != NativeFunctions.size())
            return RuntimeState::NativeFunctionBindingsMismatch;
        int64_t funcIdx = instr.Arg0;
        if (funcIdx < 0 || (size_t)funcIdx >= NativeBindings.size())
            return RuntimeState::OutOfBoundsFunctionIndex;
        if (!NativeFunctions[funcIdx])
            return RuntimeState::UnboundNativeFunction;

        Frame callFrame{ &NativeBindings[funcIdx] };
        auto res = PrepareCallFrame(frame.OperandStack, callFrame);
        if (res != RuntimeState::OK)
            return res;
        eContext.CallStack.push_back(std::move(callFrame));
        return NativeFunctions[funcIdx](eContext);
    }
    case InstructionCode::Return:
        frame.InstructionIndex = frame.Function->Code.size();
        break;
    case InstructionCode::ICall: {
        if (frame.OperandStack.size() < 1)
            return RuntimeState::StackUnderflow;
        Value funcIdxValue = std::move(frame.OperandStack.back());
        frame.OperandStack.pop_back();
        if (funcIdxValue.Type() == ValueType::FunctionReference) {
            int64_t funcIdx = funcIdxValue.AsInteger();
            if (funcIdx < 0 || (size_t)funcIdx >= Functions.size())
                return RuntimeState::OutOfBoundsFunctionIndex;
            Frame callFrame{ &Functions[funcIdx] };
            auto res = PrepareCallFrame(frame.OperandStack, callFrame);
            if (res != RuntimeState::OK)
                return res;
            eContext.CallStack.push_back(std::move(callFrame));
            break;
        } else if (funcIdxValue.Type() == ValueType::NativeFunctionReference) {
            if (NativeBindings.size() != NativeFunctions.size())
                return RuntimeState::NativeFunctionBindingsMismatch;
            int64_t funcIdx = funcIdxValue.AsInteger();
            if (funcIdx < 0 || (size_t)funcIdx >= NativeBindings.size())
                return RuntimeState::OutOfBoundsFunctionIndex;
            if (!NativeFunctions[funcIdx])
                return RuntimeState::UnboundNativeFunction;

            Frame callFrame{ &NativeBindings[funcIdx] };
            auto res = PrepareCallFrame(frame.OperandStack, callFrame);
            if (res != RuntimeState::OK)
                return res;
            eContext.CallStack.push_back(std::move(callFrame));
            return NativeFunctions[funcIdx](eContext);
        }
        return RuntimeState::TypeError;
    }
    case InstructionCode::DynSum: {
        if (frame.OperandStack.size() < 2)
            return RuntimeState::StackUnderflow;
        Value b = std::move(frame.OperandStack.back());
        frame.OperandStack.pop_back();
        Value& a = frame.OperandStack.back();
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
        if (frame.OperandStack.size() < 2)
            return RuntimeState::StackUnderflow;
        Value b = std::move(frame.OperandStack.back());
        frame.OperandStack.pop_back();
        Value& a = frame.OperandStack.back();
        if (!IsNumericValueType(a.Type()) || !IsNumericValueType(b.Type()))
            return RuntimeState::TypeError;
        if (a.Type() == ValueType::Double || b.Type() == ValueType::Double) {
            double aVal = a.Type() == ValueType::Double ? a.AsDouble() : (double)a.AsInteger();
            double bVal = b.Type() == ValueType::Double ? b.AsDouble() : (double)b.AsInteger();
            a.SetDouble(aVal - bVal);
        } else a.SetInteger(a.AsInteger() - b.AsInteger());
    } break;
    case InstructionCode::DynMul: {
        if (frame.OperandStack.size() < 2)
            return RuntimeState::StackUnderflow;
        Value b = std::move(frame.OperandStack.back());
        frame.OperandStack.pop_back();
        Value& a = frame.OperandStack.back();
        if (!IsNumericValueType(a.Type()) || !IsNumericValueType(b.Type()))
            return RuntimeState::TypeError;
        if (a.Type() == ValueType::Double || b.Type() == ValueType::Double) {
            double aVal = a.Type() == ValueType::Double ? a.AsDouble() : (double)a.AsInteger();
            double bVal = b.Type() == ValueType::Double ? b.AsDouble() : (double)b.AsInteger();
            a.SetDouble(aVal * bVal);
        } else a.SetInteger(a.AsInteger() * b.AsInteger());
    } break;
    case InstructionCode::DynDiv: {
        if (frame.OperandStack.size() < 2)
            return RuntimeState::StackUnderflow;
        Value b = std::move(frame.OperandStack.back());
        frame.OperandStack.pop_back();
        Value& a = frame.OperandStack.back();
        if (!IsNumericValueType(a.Type()) || !IsNumericValueType(b.Type()))
            return RuntimeState::TypeError;
        if (a.Type() == ValueType::Double || b.Type() == ValueType::Double) {
            double aVal = a.Type() == ValueType::Double ? a.AsDouble() : (double)a.AsInteger();
            double bVal = b.Type() == ValueType::Double ? b.AsDouble() : (double)b.AsInteger();
            a.SetDouble(aVal / bVal);
        } else a.SetInteger(a.AsInteger() / b.AsInteger());
    } break;
    case InstructionCode::Mod: {
        if (frame.OperandStack.size() < 2)
            return RuntimeState::StackUnderflow;
        Value b = std::move(frame.OperandStack.back());
        frame.OperandStack.pop_back();
        Value& a = frame.OperandStack.back();
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
        if (frame.OperandStack.size() < 1)
            return RuntimeState::StackUnderflow;
        Value truthValue = std::move(frame.OperandStack.back());
        if (!IsNumericValueType(truthValue.Type()))
            return RuntimeState::TypeError;
        frame.OperandStack.pop_back();
        // TODO: Check for bounds
        if (truthValue.Type() == ValueType::Double) {
            if (ShouldJump(instr.Code, truthValue.AsDouble()))
                --frame.InstructionIndex += instr.Arg0;
        } else if (ShouldJump(instr.Code, truthValue.AsInteger()))
            --frame.InstructionIndex += instr.Arg0;
    } break;
    case InstructionCode::Length: {
        if (frame.OperandStack.size() < 1)
            return RuntimeState::StackUnderflow;
        const Value& list = frame.OperandStack.back();
        if (list.Type() == ValueType::List) {
            Value len;
            len.SetInteger((int64_t)list.AsList().Length());
            frame.OperandStack.emplace_back(std::move(len));
        } else if (list.Type() == ValueType::String) {
            Value len;
            len.SetInteger((int64_t)list.AsString().Length());
            frame.OperandStack.emplace_back(std::move(len));
        } else return RuntimeState::TypeError;
    } break;
    case InstructionCode::PushEmptyList:
        frame.OperandStack.emplace_back().SetList(ValueList());
        break;
    case InstructionCode::Prepend: {
        if (frame.OperandStack.size() < 2)
            return RuntimeState::StackUnderflow;
        Value toPrepend = std::move(frame.OperandStack.back());
        frame.OperandStack.pop_back();
        Value& list = frame.OperandStack.back();
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
        if (frame.OperandStack.size() < 2)
            return RuntimeState::StackUnderflow;
        Value toAppend = std::move(frame.OperandStack.back());
        frame.OperandStack.pop_back();
        Value& list = frame.OperandStack.back();
        if (list.Type() == ValueType::List) {
            list.AsList().Append()->Value() = std::move(toAppend);
        } else if (list.Type() == ValueType::String) {
            if (toAppend.Type() != ValueType::String)
                return RuntimeState::TypeError;
            list.AsString() += toAppend.AsString();
        } else return RuntimeState::TypeError;
    } break;
    case InstructionCode::Concat: {
        if (frame.OperandStack.size() < 2)
            return RuntimeState::StackUnderflow;
        Value toConcat = std::move(frame.OperandStack.back());
        frame.OperandStack.pop_back();
        Value& list = frame.OperandStack.back();
        if (toConcat.Type() == ValueType::List && list.Type() == ValueType::List) {
            list.AsList().Concat(toConcat.AsList());
        } else return RuntimeState::TypeError;
    } break;
    case InstructionCode::Head: {
        if (frame.OperandStack.size() < 1)
            return RuntimeState::StackUnderflow;
        Value& list = frame.OperandStack.back();
        if (list.Type() == ValueType::List) {
            ValueList::NodeType* node = list.AsList().Front();
            if (!node) return RuntimeState::ListIndexOutOfBounds;
            Value val(std::move(node->Value()));
            list.AsList().RemoveFront(1);
            frame.OperandStack.push_back(std::move(val));
        } else return RuntimeState::TypeError;
    } break;
    case InstructionCode::Tail: {
        if (frame.OperandStack.size() < 1)
            return RuntimeState::StackUnderflow;
        Value& list = frame.OperandStack.back();
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
