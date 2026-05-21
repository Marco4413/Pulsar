#include "pulsar/runtime.h"

Pulsar::ExecutionContext::ExecutionContext(const Module& module, bool init)
    : m_Module(module)
{
    if (init) Init();
}

void Pulsar::ExecutionContext::Init()
{
    InitGlobals();
    InitCustomTypeGlobalData();
}

void Pulsar::ExecutionContext::InitGlobals()
{
    for (size_t i = 0; i < m_Module.Globals.Size(); i++)
        m_Globals.EmplaceBack(m_Module.Globals[i].CreateInstance());
}

void Pulsar::ExecutionContext::InitCustomTypeGlobalData()
{
    m_CustomTypeGlobalData.Reserve(m_Module.CustomTypes.Count());
    m_Module.CustomTypes.ForEach([this](const HashMapBucket<uint64_t, CustomType>& b) {
        if (b.Value().GlobalDataFactory)
            this->m_CustomTypeGlobalData.Emplace(b.Key(), b.Value().GlobalDataFactory());
    });
}

Pulsar::ExecutionContext Pulsar::ExecutionContext::Fork() const
{
    ExecutionContext fork(this->GetModule(), false);

    fork.GetGlobals() = this->GetGlobals();
    this->GetAllCustomTypeGlobalData().ForEach([&fork](const auto& b) {
        PULSAR_ASSERT(b.Value(), "Reference to CustomTypeGlobalData is nullptr.");
        uint64_t typeId = b.Key();
        CustomTypeGlobalData::Ref typeDataFork = b.Value()->Fork();
        fork.m_CustomTypeGlobalData.Insert(typeId, typeDataFork ? typeDataFork : b.Value());
    });

    return fork;
}

Pulsar::String Pulsar::ExecutionContext::GetCallTrace(size_t callIdx, const PositionConverterFn& positionConverter) const
{
    const Frame& frame = m_CallStack[callIdx];
    String trace;
    trace += "at (";
    if (frame.IsNative)
        trace += '*';
    trace += frame.Function->Name + ')';
    if (frame.Function->HasDebugSymbol() && m_Module.HasSourceDebugSymbols()) {
        const auto& sourceDebugSymbol = m_Module.SourceDebugSymbols[frame.Function->DebugSymbol.SourceIdx];
        const Pulsar::String& filePath = sourceDebugSymbol.Path;

        size_t codeSymbolIdx = 0;
        if (frame.InstructionIndex > 0 && frame.Function->FindCodeDebugSymbolFor(frame.InstructionIndex-1, codeSymbolIdx)) {
            const auto& token = frame.Function->CodeDebugSymbols[codeSymbolIdx].Token;
            auto sourcePos = token.SourcePos;
            if (positionConverter) {
                sourcePos = positionConverter(sourceDebugSymbol, token.SourcePos);
            } else {
                ++sourcePos.Line;
                ++sourcePos.Char;
            }
            trace += " '" + filePath;
            trace += ":" + UIntToString(sourcePos.Line);
            trace += ":" + UIntToString(sourcePos.Char);
            trace += "'";
        } else {
            const auto& token = frame.Function->DebugSymbol.Token;
            auto sourcePos = token.SourcePos;
            if (positionConverter) {
                sourcePos = positionConverter(sourceDebugSymbol, token.SourcePos);
            } else {
                ++sourcePos.Line;
                ++sourcePos.Char;
            }
            trace += " defined at '" + filePath;
            trace += ":" + UIntToString(sourcePos.Line);
            trace += ":" + UIntToString(sourcePos.Char);
            trace += "'";
        }
    }
    return trace;
}

Pulsar::String Pulsar::ExecutionContext::GetStackTrace(size_t maxDepth, const PositionConverterFn& positionConverter) const
{
    // TODO: I think that in this case we should return the number of calls.
    // NOTE: Be careful when implementing the TODO above!!
    //       There are places where we assume that 0 returns an empty string.
    if (maxDepth == 0)
        return "";

    if (m_CallStack.Size() == 0)
        return "    <empty>";

    String trace("    ");
    trace += GetCallTrace(m_CallStack.Size()-1, positionConverter);
    if (m_CallStack.Size() == 1)
        return trace;

    if (maxDepth > m_CallStack.Size())
        maxDepth = m_CallStack.Size();

    // Show half of the top-most calls and half of the bottom ones.
    // (maxDepth+1) is to give "priority" to the top-most calls in case of odd numbers.
    size_t halfMaxDepth = (maxDepth+1)/2;
    // Starting from 1 since the top-most call was already serialized.
    for (size_t i = 1; i < halfMaxDepth; i++) {
        trace += "\n    ";
        trace += GetCallTrace(m_CallStack.Size()-i-1, positionConverter);
    }

    if (maxDepth < m_CallStack.Size()) {
        trace += "\n    ... other ";
        trace += UIntToString((uint64_t)(m_CallStack.Size()-maxDepth));
        trace += " calls";
    }

    // This piece of code is not much readable.
    // Iterating from halfMaxDepth to 0 with unsigned numbers.
    for (size_t i = halfMaxDepth; i < maxDepth; i++) {
        trace += "\n    ";
        trace += GetCallTrace(maxDepth-i-1, positionConverter);
    }

    return trace;
}

Pulsar::Frame Pulsar::CallStack::CreateFrame(const FunctionDefinition* def, bool native)
{
    return {
        .Function = def,
        .IsNative = native,
        .Locals   = List<Value>(def->Arity),
        .Stack    = Stack(),
        .InstructionIndex = 0,
    };
}

Pulsar::Frame& Pulsar::CallStack::CreateAndPushFrame(const FunctionDefinition* def, bool native)
{
    return m_Frames.EmplaceBack(CreateFrame(def, native));
}

Pulsar::RuntimeState Pulsar::CallStack::PrepareFrame(Frame& frame)
{
    PULSAR_ASSERT(!IsEmpty(), "Stack not provided for new function with no candidate caller.");
    return PrepareFrame(frame, CurrentFrame().Stack);
}

Pulsar::RuntimeState Pulsar::CallStack::PrepareFrame(Frame& frame, Stack& callerStack)
{
    if (callerStack.Size() < (frame.Function->StackArity + frame.Function->Arity))
        return RuntimeState::StackUnderflow;

    frame.Locals.Resize(frame.Function->LocalsCount);
    for (size_t i = 0; i < frame.Function->Arity; i++) {
        frame.Locals[frame.Function->Arity-i-1] = callerStack.Pop();
    }

    // TODO: Figure out if this tanks performance when StackArity is 0
    frame.Stack.Resize(frame.Function->StackArity);
    for (size_t i = 0; i < frame.Function->StackArity; i++) {
        frame.Stack[frame.Function->StackArity-i-1] = callerStack.Pop();
    }

    return RuntimeState::OK;
}

Pulsar::RuntimeState Pulsar::ExecutionContext::CallFunction(const String& funcName)
{
    size_t fnIdx = m_Module.FindFunctionByName(funcName);
    if (fnIdx == m_Module.INVALID_INDEX)
        return m_State = RuntimeState::FunctionNotFound;
    return CallFunction(fnIdx);
}

Pulsar::RuntimeState Pulsar::ExecutionContext::CallFunction(FunctionSignature funcSig)
{
    size_t fnIdx = m_Module.FindFunctionBySignature(funcSig);
    if (fnIdx == m_Module.INVALID_INDEX)
        return m_State = RuntimeState::FunctionNotFound;
    return CallFunction(fnIdx);
}

Pulsar::RuntimeState Pulsar::ExecutionContext::CallFunction(int64_t funcIdx)
{
    if (funcIdx < 0 || (size_t)funcIdx >= m_Module.Functions.Size())
        return m_State = RuntimeState::OutOfBoundsFunctionIndex;
    return CallFunction(m_Module.Functions[(size_t)funcIdx]);
}

Pulsar::RuntimeState Pulsar::ExecutionContext::CallFunction(size_t funcIdx)
{
    if (funcIdx >= m_Module.Functions.Size())
        return m_State = RuntimeState::OutOfBoundsFunctionIndex;
    return CallFunction(m_Module.Functions[funcIdx]);
}

Pulsar::RuntimeState Pulsar::ExecutionContext::CallFunction(const FunctionDefinition& funcDef)
{
    // Do not allow calls on errors
    if (m_State != RuntimeState::OK) return m_State;

    Stack& callerStack = !m_CallStack.IsEmpty()
        ? m_CallStack.CurrentFrame().Stack : m_Stack;
    Frame frame = m_CallStack.CreateFrame(&funcDef, false);
    if ((m_State = m_CallStack.PrepareFrame(frame, callerStack)) != RuntimeState::OK)
        return m_State;
    m_CallStack.PushFrame(std::move(frame));
    return RuntimeState::OK;
}

void Pulsar::ExecutionContext::InternalStep()
{
    Frame& frame = m_CallStack.CurrentFrame();
    if (frame.InstructionIndex < frame.Function->Code.Size()) {
        m_State = ExecuteInstruction(frame);
        return;
    }

    Stack& callerStack = m_CallStack.HasCaller()
        ? m_CallStack.CallingFrame().Stack : m_Stack;

    Frame& returningFrame = m_CallStack.CurrentFrame();
    if (returningFrame.Stack.Size() < returningFrame.Function->Returns) {
        m_State = RuntimeState::StackUnderflow;
        return;
    }

    for (size_t i = 0; i < returningFrame.Function->Returns; i++) {
        size_t popIdx = returningFrame.Stack.Size()-returningFrame.Function->Returns+i;
        callerStack.Push(std::move(returningFrame.Stack[popIdx]));
    }

    m_CallStack.PopFrame();
    return;
}

// Used within Pulsar::Module::ExecuteInstruction to implement type-checking instructions
inline bool _InstrTypeCheck(Pulsar::InstructionCode instrCode, Pulsar::ValueType type)
{
    switch (instrCode) {
    case Pulsar::InstructionCode::IsVoid:
        return type == Pulsar::ValueType::Void;
    case Pulsar::InstructionCode::IsInteger:
        return type == Pulsar::ValueType::Integer;
    case Pulsar::InstructionCode::IsDouble:
        return type == Pulsar::ValueType::Double;
    case Pulsar::InstructionCode::IsNumber:
        return Pulsar::IsNumericValueType(type);
    case Pulsar::InstructionCode::IsFunctionReference:
        return type == Pulsar::ValueType::FunctionReference;
    case Pulsar::InstructionCode::IsNativeFunctionReference:
        return type == Pulsar::ValueType::NativeFunctionReference;
    case Pulsar::InstructionCode::IsAnyFunctionReference:
        return Pulsar::IsReferenceValueType(type);
    case Pulsar::InstructionCode::IsList:
        return type == Pulsar::ValueType::List;
    case Pulsar::InstructionCode::IsString:
        return type == Pulsar::ValueType::String;
    case Pulsar::InstructionCode::IsCustom:
        return type == Pulsar::ValueType::Custom;
    default:
        return false;
    }
}

Pulsar::RuntimeState Pulsar::ExecutionContext::ExecuteInstruction(Frame& frame)
{
    const Instruction& instr = frame.Function->Code[frame.InstructionIndex++];
    switch (instr.Code) {
    case InstructionCode::PushInt:
        frame.Stack.EmplaceInteger(instr.Arg0);
        break;
    case InstructionCode::PushDbl: {
        static_assert(sizeof(double) == sizeof(int64_t));
        const void* arg0AsVoid     = reinterpret_cast<const void*>(&instr.Arg0);
        const double* arg0AsDouble = reinterpret_cast<const double*>(arg0AsVoid);
        frame.Stack.EmplaceDouble(*arg0AsDouble);
        // Don't want to rely on std::bit_cast since my g++ does not have it.
    } break;
    case InstructionCode::PushFunctionReference:
        frame.Stack.EmplaceFunctionReference(instr.Arg0);
        break;
    case InstructionCode::PushNativeFunctionReference:
        frame.Stack.EmplaceNativeFunctionReference(instr.Arg0);
        break;
    case InstructionCode::PushConst:
        if (instr.Arg0 < 0 || (size_t)instr.Arg0 >= m_Module.Constants.Size())
            return RuntimeState::OutOfBoundsConstantIndex;
        frame.Stack.Push(m_Module.Constants[(size_t)instr.Arg0]);
        break;
    case InstructionCode::PushLocal:
        if (instr.Arg0 < 0 || (size_t)instr.Arg0 >= frame.Locals.Size())
            return RuntimeState::OutOfBoundsLocalIndex;
        frame.Stack.Push(frame.Locals[(size_t)instr.Arg0]);
        break;
    case InstructionCode::MoveLocal:
        if (instr.Arg0 < 0 || (size_t)instr.Arg0 >= frame.Locals.Size())
            return RuntimeState::OutOfBoundsLocalIndex;
        frame.Stack.Push(std::move(frame.Locals[(size_t)instr.Arg0]));
        break;
    case InstructionCode::PopIntoLocal:
        if (frame.Stack.Size() < 1)
            return RuntimeState::StackUnderflow;
        if (instr.Arg0 < 0 || (size_t)instr.Arg0 >= frame.Locals.Size())
            return RuntimeState::OutOfBoundsLocalIndex;
        frame.Locals[(size_t)instr.Arg0] = frame.Stack.Pop();
        break;
    case InstructionCode::CopyIntoLocal:
        if (frame.Stack.Size() < 1)
            return RuntimeState::StackUnderflow;
        if (instr.Arg0 < 0 || (size_t)instr.Arg0 >= frame.Locals.Size())
            return RuntimeState::OutOfBoundsLocalIndex;
        frame.Locals[(size_t)instr.Arg0] = frame.Stack.Top();
        break;
    case InstructionCode::PushGlobal:
        if (instr.Arg0 < 0 || (size_t)instr.Arg0 >= m_Globals.Size())
            return RuntimeState::OutOfBoundsGlobalIndex;
        frame.Stack.Push(m_Globals[(size_t)instr.Arg0].Value);
        break;
    case InstructionCode::MoveGlobal: {
        if (instr.Arg0 < 0 || (size_t)instr.Arg0 >= m_Globals.Size())
            return RuntimeState::OutOfBoundsGlobalIndex;
        GlobalInstance& global = m_Globals[(size_t)instr.Arg0];
        if (global.IsConstant)
            return RuntimeState::WritingOnConstantGlobal;
        frame.Stack.Push(std::move(global.Value));
    } break;
    case InstructionCode::PopIntoGlobal: {
        if (frame.Stack.Size() < 1)
            return RuntimeState::StackUnderflow;
        if (instr.Arg0 < 0 || (size_t)instr.Arg0 >= m_Globals.Size())
            return RuntimeState::OutOfBoundsGlobalIndex;
        GlobalInstance& global = m_Globals[(size_t)instr.Arg0];
        if (global.IsConstant)
            return RuntimeState::WritingOnConstantGlobal;
        global.Value = frame.Stack.Pop();
    } break;
    case InstructionCode::CopyIntoGlobal: {
        if (frame.Stack.Size() < 1)
            return RuntimeState::StackUnderflow;
        if (instr.Arg0 < 0 || (size_t)instr.Arg0 >= m_Globals.Size())
            return RuntimeState::OutOfBoundsGlobalIndex;
        GlobalInstance& global = m_Globals[(size_t)instr.Arg0];
        if (global.IsConstant)
            return RuntimeState::WritingOnConstantGlobal;
        global.Value = frame.Stack.Top();
    } break;
    case InstructionCode::Pack: {
        Value::List list;
        // If Arg0 <= 0, push an empty list
        if (instr.Arg0 > 0) {
            size_t packing = (size_t)instr.Arg0;
            if (frame.Stack.Size() < packing)
                return RuntimeState::StackUnderflow;
            for (size_t i = 0; i < packing; i++) {
                list.Prepend(frame.Stack.Pop());
            }
        }
        frame.Stack.EmplaceList(std::move(list));
    } break;
    case InstructionCode::Pop: {
        size_t popCount = (size_t)(instr.Arg0 > 0 ? instr.Arg0 : 1);
        if (frame.Stack.Size() < popCount)
            return RuntimeState::StackUnderflow;
        for (size_t i = 0; i < popCount; i++)
            frame.Stack.Pop();
    } break;
    case InstructionCode::Swap: {
        if (frame.Stack.Size() < 2)
            return RuntimeState::StackUnderflow;
        Value tmp(frame.Stack[-1]);
        frame.Stack[-1] = std::move(frame.Stack[-2]);
        frame.Stack[-2] = std::move(tmp);
    } break;
    case InstructionCode::Dup: {
        if (frame.Stack.Size() < 1)
            return RuntimeState::StackUnderflow;
        size_t dupCount = (size_t)(instr.Arg0 > 0 ? instr.Arg0 : 1);
        Value val(frame.Stack.Top());
        for (size_t i = 0; i < dupCount-1; i++)
            frame.Stack.Push(val);
        frame.Stack.Push(std::move(val));
    } break;
    case InstructionCode::Call: {
        int64_t funcIdx = instr.Arg0;
        if (funcIdx < 0 || (size_t)funcIdx >= m_Module.Functions.Size())
            return RuntimeState::OutOfBoundsFunctionIndex;

        Frame callFrame = m_CallStack.CreateFrame(&m_Module.Functions[(size_t)funcIdx], false);
        auto res = m_CallStack.PrepareFrame(callFrame, frame.Stack);
        if (res != RuntimeState::OK)
            return res;
        m_CallStack.PushFrame(std::move(callFrame));
    } break;
    case InstructionCode::CallNative: {
        if (m_Module.NativeBindings.Size() != m_Module.NativeFunctions.Size())
            return RuntimeState::NativeFunctionBindingsMismatch;
        int64_t funcIdx = instr.Arg0;
        if (funcIdx < 0 || (size_t)funcIdx >= m_Module.NativeBindings.Size())
            return RuntimeState::OutOfBoundsFunctionIndex;
        if (!m_Module.NativeFunctions[(size_t)funcIdx])
            return RuntimeState::UnboundNativeFunction;

        Frame callFrame = m_CallStack.CreateFrame(&m_Module.NativeBindings[(size_t)funcIdx], true);
        auto res = m_CallStack.PrepareFrame(callFrame, frame.Stack);
        if (res != RuntimeState::OK)
            return res;
        m_CallStack.PushFrame(std::move(callFrame));
        return m_Module.NativeFunctions[(size_t)funcIdx](*this);
    }
    case InstructionCode::Return:
        // Because we use frame.InstructionIndex to find where an error occurred,
        //  WE MUST check if we can actually return here, otherwise the error will
        //  appear at the end of the function.
        if (frame.Stack.Size() < frame.Function->Returns)
            return RuntimeState::StackUnderflow;
        frame.InstructionIndex = frame.Function->Code.Size();
        break;
    case InstructionCode::ICall: {
        if (frame.Stack.Size() < 1)
            return RuntimeState::StackUnderflow;
        Value funcIdxValue = frame.Stack.Pop();
        if (funcIdxValue.Type() == ValueType::FunctionReference) {
            int64_t funcIdx = funcIdxValue.AsInteger();
            if (funcIdx < 0 || (size_t)funcIdx >= m_Module.Functions.Size())
                return RuntimeState::OutOfBoundsFunctionIndex;
            Frame callFrame = m_CallStack.CreateFrame(&m_Module.Functions[(size_t)funcIdx], false);
            auto res = m_CallStack.PrepareFrame(callFrame, frame.Stack);
            if (res != RuntimeState::OK)
                return res;
            m_CallStack.PushFrame(std::move(callFrame));
            break;
        } else if (funcIdxValue.Type() == ValueType::NativeFunctionReference) {
            if (m_Module.NativeBindings.Size() != m_Module.NativeFunctions.Size())
                return RuntimeState::NativeFunctionBindingsMismatch;
            int64_t funcIdx = funcIdxValue.AsInteger();
            if (funcIdx < 0 || (size_t)funcIdx >= m_Module.NativeBindings.Size())
                return RuntimeState::OutOfBoundsFunctionIndex;
            if (!m_Module.NativeFunctions[(size_t)funcIdx])
                return RuntimeState::UnboundNativeFunction;

            Frame callFrame = m_CallStack.CreateFrame(&m_Module.NativeBindings[(size_t)funcIdx], true);
            auto res = m_CallStack.PrepareFrame(callFrame, frame.Stack);
            if (res != RuntimeState::OK)
                return res;
            m_CallStack.PushFrame(std::move(callFrame));
            return m_Module.NativeFunctions[(size_t)funcIdx](*this);
        }
        return RuntimeState::TypeError;
    }
    case InstructionCode::DynSum: {
        if (frame.Stack.Size() < 2)
            return RuntimeState::StackUnderflow;
        Value  b = frame.Stack.Pop();
        Value& a = frame.Stack.Top();
        if (!IsNumericValueType(a.Type()) || !IsNumericValueType(b.Type()))
            return RuntimeState::TypeError;
        if (a.Type() == ValueType::Double || b.Type() == ValueType::Double) {
            double aVal = a.Type() == ValueType::Double ? a.AsDouble() : (double)a.AsInteger();
            double bVal = b.Type() == ValueType::Double ? b.AsDouble() : (double)b.AsInteger();
            a.SetDouble(aVal + bVal);
        } else a.SetInteger(a.AsInteger() + b.AsInteger());
    } break;
    case InstructionCode::DynSub: {
        if (frame.Stack.Size() < 2)
            return RuntimeState::StackUnderflow;
        Value  b = frame.Stack.Pop();
        Value& a = frame.Stack.Top();
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
        Value  b = frame.Stack.Pop();
        Value& a = frame.Stack.Top();
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
        Value  b = frame.Stack.Pop();
        Value& a = frame.Stack.Top();
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
        Value  b = frame.Stack.Pop();
        Value& a = frame.Stack.Top();
        if (a.Type() != ValueType::Integer || b.Type() != ValueType::Integer)
            return RuntimeState::TypeError;
        a.SetInteger(a.AsInteger() % b.AsInteger());
    } break;
    case InstructionCode::BitAnd: {
        if (frame.Stack.Size() < 2)
            return RuntimeState::StackUnderflow;
        Value  b = frame.Stack.Pop();
        Value& a = frame.Stack.Top();
        if (a.Type() != ValueType::Integer || b.Type() != ValueType::Integer)
            return RuntimeState::TypeError;
        a.SetInteger(a.AsInteger() & b.AsInteger());
    } break;
    case InstructionCode::BitOr: {
        if (frame.Stack.Size() < 2)
            return RuntimeState::StackUnderflow;
        Value  b = frame.Stack.Pop();
        Value& a = frame.Stack.Top();
        if (a.Type() != ValueType::Integer || b.Type() != ValueType::Integer)
            return RuntimeState::TypeError;
        a.SetInteger(a.AsInteger() | b.AsInteger());
    } break;
    case InstructionCode::BitNot: {
        if (frame.Stack.Size() < 1)
            return RuntimeState::StackUnderflow;
        Value& a = frame.Stack.Top();
        if (a.Type() != ValueType::Integer)
            return RuntimeState::TypeError;
        a.SetInteger(~a.AsInteger());
    } break;
    case InstructionCode::BitXor: {
        if (frame.Stack.Size() < 2)
            return RuntimeState::StackUnderflow;
        Value  b = frame.Stack.Pop();
        Value& a = frame.Stack.Top();
        if (a.Type() != ValueType::Integer || b.Type() != ValueType::Integer)
            return RuntimeState::TypeError;
        a.SetInteger(a.AsInteger() ^ b.AsInteger());
    } break;
    case InstructionCode::BitShiftLeft: {
        if (frame.Stack.Size() < 2)
            return RuntimeState::StackUnderflow;
        Value  b = frame.Stack.Pop();
        Value& a = frame.Stack.Top();
        if (a.Type() != ValueType::Integer || b.Type() != ValueType::Integer)
            return RuntimeState::TypeError;
        constexpr int64_t BITS_OF_INT = 8*sizeof(b.AsInteger());
        if (b.AsInteger() <= -BITS_OF_INT || b.AsInteger() >= BITS_OF_INT) {
            // Handle C++ undefined behaviour.
            // See "Built-in bitwise shift operators":
            //   https://en.cppreference.com/w/cpp/language/operator_arithmetic
            a.SetInteger(0);
        } else if (b.AsInteger() < 0) {
            a.SetInteger((int64_t)((uint64_t)a.AsInteger() >> -b.AsInteger()));
        } else {
            a.SetInteger((int64_t)((uint64_t)a.AsInteger() << b.AsInteger()));
        }
    } break;
    case InstructionCode::BitShiftRight: {
        if (frame.Stack.Size() < 2)
            return RuntimeState::StackUnderflow;
        Value  b = frame.Stack.Pop();
        Value& a = frame.Stack.Top();
        if (a.Type() != ValueType::Integer || b.Type() != ValueType::Integer)
            return RuntimeState::TypeError;
        constexpr int64_t BITS_OF_INT = 8*sizeof(b.AsInteger());
        if (b.AsInteger() <= -BITS_OF_INT || b.AsInteger() >= BITS_OF_INT) {
            // Handle C++ undefined behaviour.
            // See "Built-in bitwise shift operators":
            //   https://en.cppreference.com/w/cpp/language/operator_arithmetic
            a.SetInteger(0);
        } else if (b.AsInteger() < 0) {
            a.SetInteger((int64_t)((uint64_t)a.AsInteger() << -b.AsInteger()));
        } else {
            a.SetInteger((int64_t)((uint64_t)a.AsInteger() >> b.AsInteger()));
        }
    } break;
    // TODO: Add floor/double, ceil/double and truncate instructions.
    case InstructionCode::Floor: {
        if (frame.Stack.Size() < 1)
            return RuntimeState::StackUnderflow;
        Value& val = frame.Stack.Top();
        if (!IsNumericValueType(val.Type()))
            return RuntimeState::TypeError;
        if (val.Type() == ValueType::Double)
            val.SetInteger((int64_t)std::floor(val.AsDouble()));
    } break;
    case InstructionCode::Ceil: {
        if (frame.Stack.Size() < 1)
            return RuntimeState::StackUnderflow;
        Value& val = frame.Stack.Top();
        if (!IsNumericValueType(val.Type()))
            return RuntimeState::TypeError;
        if (val.Type() == ValueType::Double)
            val.SetInteger((int64_t)std::ceil(val.AsDouble()));
    } break;
    case InstructionCode::Compare: {
        if (frame.Stack.Size() < 2)
            return RuntimeState::StackUnderflow;
        Value  b = frame.Stack.Pop();
        Value& a = frame.Stack.Top();

        if (IsNumericValueType(a.Type()) && IsNumericValueType(b.Type())) {
            if (a.Type() == ValueType::Double || b.Type() == ValueType::Double) {
                double aVal = a.Type() == ValueType::Double ? a.AsDouble() : (double)a.AsInteger();
                double bVal = b.Type() == ValueType::Double ? b.AsDouble() : (double)b.AsInteger();
                a.SetDouble(aVal - bVal);
            } else a.SetInteger(a.AsInteger() - b.AsInteger());
            break;
        }

        if (a.Type() != b.Type() || a.Type() != ValueType::String)
            return RuntimeState::TypeError;
        a.SetInteger(a.AsString().Compare(b.AsString()));
    } break;
    case InstructionCode::Equals: {
        if (frame.Stack.Size() < 2)
            return RuntimeState::StackUnderflow;
        Value  b = frame.Stack.Pop();
        Value& a = frame.Stack.Top();
        a.SetInteger(a == b ? 1 : 0);
    } break;
    case InstructionCode::J:
        frame.InstructionIndex = (size_t)((frame.InstructionIndex-1) + instr.Arg0);
        break;
    case InstructionCode::JZ:
    case InstructionCode::JNZ:
    case InstructionCode::JGZ:
    case InstructionCode::JGEZ:
    case InstructionCode::JLZ:
    case InstructionCode::JLEZ: {
        if (frame.Stack.Size() < 1)
            return RuntimeState::StackUnderflow;
        Value truthValue = frame.Stack.Pop();
        if (!IsNumericValueType(truthValue.Type()))
            return RuntimeState::TypeError;
        // TODO: Check for bounds (maybe not)
        if (truthValue.Type() == ValueType::Double) {
            if (ShouldJump(instr.Code, truthValue.AsDouble()))
                frame.InstructionIndex = (size_t)((frame.InstructionIndex-1) + instr.Arg0);
        } else if (ShouldJump(instr.Code, truthValue.AsInteger()))
            frame.InstructionIndex = (size_t)((frame.InstructionIndex-1) + instr.Arg0);
    } break;
    case InstructionCode::Length: {
        if (frame.Stack.Size() < 1)
            return RuntimeState::StackUnderflow;
        const Value& list = frame.Stack.Top();
        if (list.Type() == ValueType::List) {
            Value len;
            len.SetInteger((int64_t)list.AsList().Length());
            frame.Stack.Push(std::move(len));
        } else if (list.Type() == ValueType::String) {
            Value len;
            len.SetInteger((int64_t)list.AsString().Length());
            frame.Stack.Push(std::move(len));
        } else return RuntimeState::TypeError;
    } break;
    case InstructionCode::IsEmpty: {
        if (frame.Stack.Size() < 1)
            return RuntimeState::StackUnderflow;
        const Value& list = frame.Stack.Top();
        Value isEmpty;
        if (list.Type() == ValueType::List) {
            isEmpty.SetInteger(list.AsList().Front() ? 0 : 1);
        } else if (list.Type() == ValueType::String) {
            isEmpty.SetInteger(list.AsString().Length() == 0 ? 1 : 0);
        } else return RuntimeState::TypeError;
        frame.Stack.Push(std::move(isEmpty));
    } break;
    case InstructionCode::PushEmptyList:
        frame.Stack.EmplaceList();
        break;
    case InstructionCode::Prepend: {
        if (frame.Stack.Size() < 2)
            return RuntimeState::StackUnderflow;
        Value toPrepend = frame.Stack.Pop();
        Value& list = frame.Stack.Top();
        if (list.Type() == ValueType::List) {
            list.AsList().Prepend(std::move(toPrepend));
        } else if (list.Type() == ValueType::String) {
            if (toPrepend.Type() == ValueType::String) {
                toPrepend.AsString() += list.AsString();
                list.AsString() = std::move(toPrepend.AsString());
            } else if (toPrepend.Type() == ValueType::Integer) {
                Pulsar::String prepended;
                prepended.Reserve(list.AsString().Capacity()+1);
                prepended += (char)toPrepend.AsInteger();
                prepended += list.AsString();
                list.AsString() = std::move(prepended);
            } else return RuntimeState::TypeError;
        } else return RuntimeState::TypeError;
    } break;
    case InstructionCode::Append: {
        if (frame.Stack.Size() < 2)
            return RuntimeState::StackUnderflow;
        Value toAppend = frame.Stack.Pop();
        Value& list = frame.Stack.Top();
        if (list.Type() == ValueType::List) {
            list.AsList().Append(std::move(toAppend));
        } else if (list.Type() == ValueType::String) {
            if (toAppend.Type() == ValueType::String)
                list.AsString() += toAppend.AsString();
            else if (toAppend.Type() == ValueType::Integer)
                list.AsString() += (char)toAppend.AsInteger();
            else return RuntimeState::TypeError;
        } else return RuntimeState::TypeError;
    } break;
    case InstructionCode::Concat: {
        if (frame.Stack.Size() < 2)
            return RuntimeState::StackUnderflow;
        Value toConcat = frame.Stack.Pop();
        Value& list = frame.Stack.Top();
        if (toConcat.Type() == ValueType::List && list.Type() == ValueType::List) {
            list.AsList().Concat(std::move(toConcat.AsList()));
        } else return RuntimeState::TypeError;
    } break;
    case InstructionCode::Head: {
        if (frame.Stack.Size() < 1)
            return RuntimeState::StackUnderflow;
        Value& list = frame.Stack.Top();
        if (list.Type() == ValueType::List) {
            Value::List::Node* node = list.AsList().Front();
            if (!node) return RuntimeState::ListIndexOutOfBounds;
            Value val(std::move(node->Value()));
            list.AsList().RemoveFront(1);
            frame.Stack.Push(std::move(val));
        } else return RuntimeState::TypeError;
    } break;
    case InstructionCode::Tail: {
        if (frame.Stack.Size() < 1)
            return RuntimeState::StackUnderflow;
        Value& list = frame.Stack.Top();
        if (list.Type() == ValueType::List) {
            list.AsList().RemoveFront(1);
        } else return RuntimeState::TypeError;
    } break;
    case InstructionCode::Unpack: {
        if (frame.Stack.Size() < 1)
            return RuntimeState::StackUnderflow;
        Value listToUnpack = frame.Stack.Pop();
        if (listToUnpack.Type() != ValueType::List)
            return RuntimeState::TypeError;

        // If Arg0 <= 0, pop list
        if (instr.Arg0 > 0) {
            size_t unpackCount = (size_t)instr.Arg0;

            Value::List::Node* node = listToUnpack.AsList().Back();
            for (size_t i = 1; i < unpackCount && node; i++) {
                node = node->Prev();
            }

            for (size_t i = 0; i < unpackCount; i++) {
                if (!node) return RuntimeState::ListIndexOutOfBounds;
                frame.Stack.Push(std::move(node->Value()));
                node = node->Next();
            }
        }
    } break;
    case InstructionCode::Index: {
        if (frame.Stack.Size() < 2)
            return RuntimeState::StackUnderflow;
        Value& list = frame.Stack[-2];
        Value& index = frame.Stack[-1];
        if (index.Type() != ValueType::Integer)
            return RuntimeState::TypeError;

        if (list.Type() == ValueType::List) {
            if (index.AsInteger() < 0)
                return RuntimeState::ListIndexOutOfBounds;
            Value::List::Node* node = list.AsList().Front();
            for (size_t i = 0; i < (size_t)index.AsInteger() && node; i++)
                node = node->Next();
            if (!node)
                return RuntimeState::ListIndexOutOfBounds;
            index = node->Value();
        } else if (list.Type() == ValueType::String) {
            if (index.AsInteger() < 0 || (size_t)index.AsInteger() >= list.AsString().Length())
                return RuntimeState::StringIndexOutOfBounds;
            int64_t ch = (unsigned char)list.AsString()[(size_t)index.AsInteger()];
            index.SetInteger(ch);
        } else return RuntimeState::TypeError;
    } break;
    case InstructionCode::Prefix: {
        if (frame.Stack.Size() < 2)
            return RuntimeState::StackUnderflow;
        Value& str = frame.Stack[-2];
        Value& length = frame.Stack[-1];
        if (length.Type() != ValueType::Integer)
            return RuntimeState::TypeError;

        if (str.Type() == ValueType::String) {
            if (length.AsInteger() == 0) {
                length.SetString("");
                break;
            } else if (length.AsInteger() < 0 || (size_t)length.AsInteger() > str.AsString().Length())
                return RuntimeState::StringIndexOutOfBounds;
            size_t prefLen = (size_t)length.AsInteger();
            String prefix(str.AsString().CString(), prefLen);
            String postPrefix(&str.AsString().CString()[prefLen], str.AsString().Length()-prefLen);
            str.SetString(std::move(postPrefix));
            length.SetString(std::move(prefix));
        } else return RuntimeState::TypeError;
    } break;
    case InstructionCode::Suffix: {
        if (frame.Stack.Size() < 2)
            return RuntimeState::StackUnderflow;
        Value& str = frame.Stack[-2];
        Value& length = frame.Stack[-1];
        if (length.Type() != ValueType::Integer)
            return RuntimeState::TypeError;

        if (str.Type() == ValueType::String) {
            if (length.AsInteger() == 0) {
                length.SetString("");
                break;
            } else if (length.AsInteger() < 0 || (size_t)length.AsInteger() > str.AsString().Length())
                return RuntimeState::StringIndexOutOfBounds;
            size_t sufLen = (size_t)length.AsInteger();
            String suffix(&str.AsString().CString()[str.AsString().Length()-sufLen], sufLen);
            String preSuffix(str.AsString().CString(), str.AsString().Length()-sufLen);
            str.SetString(std::move(preSuffix));
            length.SetString(std::move(suffix));
        } else return RuntimeState::TypeError;
    } break;
    case InstructionCode::Substr: {
        if (frame.Stack.Size() < 3)
            return RuntimeState::StackUnderflow;
        Value endIdx = frame.Stack.Pop();
        if (endIdx.Type() != ValueType::Integer)
            return RuntimeState::TypeError;
        Value& startIdx = frame.Stack[-1];
        if (startIdx.Type() != ValueType::Integer)
            return RuntimeState::TypeError;
        Value& str = frame.Stack[-2];

        if (str.Type() == ValueType::String) {
            if (startIdx.AsInteger() < 0 || endIdx.AsInteger() < 0)
                return RuntimeState::StringIndexOutOfBounds;
            startIdx.SetString(str.AsString().SubString(
                (size_t)startIdx.AsInteger(), (size_t)endIdx.AsInteger()
            ));
        } else return RuntimeState::TypeError;
    } break;
    case InstructionCode::IsVoid:
    case InstructionCode::IsInteger:
    case InstructionCode::IsDouble:
    case InstructionCode::IsNumber:
    case InstructionCode::IsFunctionReference:
    case InstructionCode::IsNativeFunctionReference:
    case InstructionCode::IsAnyFunctionReference:
    case InstructionCode::IsList:
    case InstructionCode::IsString:
    case InstructionCode::IsCustom: {
        if (frame.Stack.Size() < 1)
            return RuntimeState::StackUnderflow;
        ValueType valueType = frame.Stack.Top().Type();
        frame.Stack.EmplaceInteger(_InstrTypeCheck(instr.Code, valueType) ? 1 : 0);
    } break;
    }

    return RuntimeState::OK;
}
