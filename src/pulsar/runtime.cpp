#include "pulsar/runtime.h"

Pulsar::ExecutionContext::ExecutionContext(const Module& module, bool init)
    : m_Module(module)
{
    if (init) Init();
}

void Pulsar::ExecutionContext::Init()
{
    InitGlobals();
    InitCustomTypeData();
}

void Pulsar::ExecutionContext::InitGlobals()
{
    for (size_t i = 0; i < m_Module.Globals.Size(); i++)
        m_Globals.EmplaceBack(m_Module.Globals[i].CreateInstance());
}

void Pulsar::ExecutionContext::InitCustomTypeData()
{
    m_CustomTypeData.Reserve(m_Module.CustomTypes.Count());
    m_Module.CustomTypes.ForEach([this](const HashMapBucket<uint64_t, CustomType>& b) {
        if (b.Value().DataFactory)
            this->m_CustomTypeData.Emplace(b.Key(), b.Value().DataFactory());
    });
}

Pulsar::String Pulsar::ExecutionContext::GetCallTrace(size_t callIdx) const
{
    const Frame& frame = m_CallStack[callIdx];
    String trace;
    trace += "at (";
    if (frame.IsNative)
        trace += '*';
    trace += frame.Function->Name + ')';
    if (frame.Function->HasDebugSymbol() && m_Module.HasSourceDebugSymbols()) {
        const Pulsar::String& filePath = m_Module.SourceDebugSymbols[frame.Function->DebugSymbol.SourceIdx].Path;

        size_t codeSymbolIdx = 0;
        if (frame.InstructionIndex > 0 && frame.Function->FindCodeDebugSymbolFor(frame.InstructionIndex-1, codeSymbolIdx)) {
            const auto& token = frame.Function->CodeDebugSymbols[codeSymbolIdx].Token;
            trace += " '" + filePath;
            trace += ":" + UIntToString(token.SourcePos.Line+1);
            trace += ":" + UIntToString(token.SourcePos.Char+1);
            trace += "'";
        } else {
            const auto& token = frame.Function->DebugSymbol.Token;
            trace += " defined at '" + filePath;
            trace += ":" + UIntToString(token.SourcePos.Line+1);
            trace += ":" + UIntToString(token.SourcePos.Char+1);
            trace += "'";
        }
    }
    return trace;
}

Pulsar::String Pulsar::ExecutionContext::GetStackTrace(size_t maxDepth) const
{
    if (m_CallStack.Size() == 0 || maxDepth == 0)
        return "";

    String trace("    ");
    trace += GetCallTrace(m_CallStack.Size()-1);
    if (m_CallStack.Size() == 1 || maxDepth == 1)
        return trace;

    if (maxDepth > m_CallStack.Size())
        maxDepth = m_CallStack.Size();

    // Show half of the top-most calls and half of the bottom ones.
    // (maxDepth+1) is to give "priority" to the top-most calls in case of odd numbers.
    for (size_t i = 1; i < (maxDepth+1)/2; i++) {
        trace += "\n    ";
        trace += GetCallTrace(i);
    }

    if (maxDepth < m_CallStack.Size()) {
        trace += "\n    ... other ";
        trace += UIntToString((uint64_t)(m_CallStack.Size()-maxDepth));
        trace += " calls";
    }

    for (size_t i = (maxDepth+1)/2; i < maxDepth; i++) {
        trace += "\n    ";
        trace += GetCallTrace(maxDepth-i-1);
    }

    return trace;
}

Pulsar::Frame Pulsar::CallStack::CreateFrame(const FunctionDefinition* def, bool native)
{
    return {
        .Function = def,
        .IsNative = native,
        .Locals = List<Value>(def->Arity),
        .Stack = ValueStack(),
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

Pulsar::RuntimeState Pulsar::CallStack::PrepareFrame(Frame& frame, ValueStack& callerStack)
{
    if (callerStack.Size() < (frame.Function->StackArity + frame.Function->Arity))
        return RuntimeState::StackUnderflow;

    frame.Locals.Resize(frame.Function->LocalsCount);
    for (size_t i = 0; i < frame.Function->Arity; i++) {
        frame.Locals[frame.Function->Arity-i-1] = std::move(callerStack.Back());
        callerStack.PopBack();
    }

    // TODO: Figure out if this tanks performance when StackArity is 0
    frame.Stack.Resize(frame.Function->StackArity);
    for (size_t i = 0; i < frame.Function->StackArity; i++) {
        frame.Stack[frame.Function->StackArity-i-1] = std::move(callerStack.Back());
        callerStack.PopBack();
    }

    return RuntimeState::OK;
}

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

int64_t Pulsar::Module::DeclareAndBindNativeFunction(FunctionDefinition def, NativeFunction func)
{
    NativeBindings.EmplaceBack(std::move(def));
    NativeFunctions.Resize(NativeBindings.Size());
    NativeFunctions.Back() = func;
    return (int64_t)(NativeBindings.Size())-1;
}

uint64_t Pulsar::Module::BindCustomType(const String& name, CustomType::DataFactoryFn dataFactory)
{
    while (CustomTypes.Find(++m_LastTypeId));
    CustomTypes.Emplace(m_LastTypeId, name, dataFactory);
    return m_LastTypeId;
}

Pulsar::RuntimeState Pulsar::ExecutionContext::CallFunction(const String& funcName)
{
    for (size_t i = m_Module.Functions.Size(); i > 0; --i) {
        const FunctionDefinition& funcDef = m_Module.Functions[i-1];
        if (funcDef.Name != funcName)
            continue;
        return CallFunction(i-1);
    }
    return m_State = RuntimeState::FunctionNotFound;
}

Pulsar::RuntimeState Pulsar::ExecutionContext::CallFunction(int64_t funcIdx)
{
    if (funcIdx < 0 || (size_t)funcIdx >= m_Module.Functions.Size())
        return m_State = RuntimeState::OutOfBoundsFunctionIndex;
    return CallFunction(m_Module.Functions[(size_t)funcIdx]);
}

Pulsar::RuntimeState Pulsar::ExecutionContext::CallFunction(const FunctionDefinition& funcDef)
{
    ValueStack& callerStack = !m_CallStack.IsEmpty()
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

    ValueStack& callerStack = m_CallStack.HasCaller()
        ? m_CallStack.CallingFrame().Stack : m_Stack;

    Frame& returningFrame = m_CallStack.CurrentFrame();
    if (returningFrame.Stack.Size() < returningFrame.Function->Returns) {
        m_State = RuntimeState::StackUnderflow;
        return;
    }

    for (size_t i = 0; i < returningFrame.Function->Returns; i++) {
        size_t popIdx = returningFrame.Stack.Size()-returningFrame.Function->Returns+i;
        callerStack.EmplaceBack(std::move(returningFrame.Stack[popIdx]));
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
        if (instr.Arg0 < 0 || (size_t)instr.Arg0 >= m_Module.Constants.Size())
            return RuntimeState::OutOfBoundsConstantIndex;
        frame.Stack.EmplaceBack(m_Module.Constants[(size_t)instr.Arg0]);
        break;
    case InstructionCode::PushLocal:
        if (instr.Arg0 < 0 || (size_t)instr.Arg0 >= frame.Locals.Size())
            return RuntimeState::OutOfBoundsLocalIndex;
        frame.Stack.PushBack(frame.Locals[(size_t)instr.Arg0]);
        break;
    case InstructionCode::MoveLocal:
        if (instr.Arg0 < 0 || (size_t)instr.Arg0 >= frame.Locals.Size())
            return RuntimeState::OutOfBoundsLocalIndex;
        frame.Stack.PushBack(std::move(frame.Locals[(size_t)instr.Arg0]));
        break;
    case InstructionCode::PopIntoLocal:
        if (frame.Stack.Size() < 1)
            return RuntimeState::StackUnderflow;
        if (instr.Arg0 < 0 || (size_t)instr.Arg0 >= frame.Locals.Size())
            return RuntimeState::OutOfBoundsLocalIndex;
        frame.Locals[(size_t)instr.Arg0] = std::move(frame.Stack.Back());
        frame.Stack.PopBack();
        break;
    case InstructionCode::CopyIntoLocal:
        if (frame.Stack.Size() < 1)
            return RuntimeState::StackUnderflow;
        if (instr.Arg0 < 0 || (size_t)instr.Arg0 >= frame.Locals.Size())
            return RuntimeState::OutOfBoundsLocalIndex;
        frame.Locals[(size_t)instr.Arg0] = frame.Stack.Back();
        break;
    case InstructionCode::PushGlobal:
        if (instr.Arg0 < 0 || (size_t)instr.Arg0 >= m_Globals.Size())
            return RuntimeState::OutOfBoundsGlobalIndex;
        frame.Stack.PushBack(m_Globals[(size_t)instr.Arg0].Value);
        break;
    case InstructionCode::MoveGlobal: {
        if (instr.Arg0 < 0 || (size_t)instr.Arg0 >= m_Globals.Size())
            return RuntimeState::OutOfBoundsGlobalIndex;
        GlobalInstance& global = m_Globals[(size_t)instr.Arg0];
        if (global.IsConstant)
            return RuntimeState::WritingOnConstantGlobal;
        frame.Stack.PushBack(std::move(global.Value));
    } break;
    case InstructionCode::PopIntoGlobal: {
        if (frame.Stack.Size() < 1)
            return RuntimeState::StackUnderflow;
        if (instr.Arg0 < 0 || (size_t)instr.Arg0 >= m_Globals.Size())
            return RuntimeState::OutOfBoundsGlobalIndex;
        GlobalInstance& global = m_Globals[(size_t)instr.Arg0];
        if (global.IsConstant)
            return RuntimeState::WritingOnConstantGlobal;
        global.Value = std::move(frame.Stack.Back());
        frame.Stack.PopBack();
    } break;
    case InstructionCode::CopyIntoGlobal: {
        if (frame.Stack.Size() < 1)
            return RuntimeState::StackUnderflow;
        if (instr.Arg0 < 0 || (size_t)instr.Arg0 >= m_Globals.Size())
            return RuntimeState::OutOfBoundsGlobalIndex;
        GlobalInstance& global = m_Globals[(size_t)instr.Arg0];
        if (global.IsConstant)
            return RuntimeState::WritingOnConstantGlobal;
        global.Value = frame.Stack.Back();
    } break;
    case InstructionCode::Pack: {
        ValueList list;
        // If Arg0 <= 0, push an empty list
        if (instr.Arg0 > 0) {
            size_t packing = (size_t)instr.Arg0;
            if (frame.Stack.Size() < packing)
                return RuntimeState::StackUnderflow;
            for (size_t i = 0; i < packing; i++) {
                list.Prepend(std::move(frame.Stack.Back()));
                frame.Stack.PopBack();
            }
        }
        frame.Stack.EmplaceBack()
            .SetList(std::move(list));
    } break;
    case InstructionCode::Pop: {
        size_t popCount = (size_t)(instr.Arg0 > 0 ? instr.Arg0 : 1);
        if (frame.Stack.Size() < popCount)
            return RuntimeState::StackUnderflow;
        for (size_t i = 0; i < popCount; i++)
            frame.Stack.PopBack();
    } break;
    case InstructionCode::Swap: {
        if (frame.Stack.Size() < 2)
            return RuntimeState::StackUnderflow;
        Value back(std::move(frame.Stack.Back()));
        size_t secondToLastIdx = frame.Stack.Size()-2;
        frame.Stack.Back() = std::move(frame.Stack[secondToLastIdx]);
        frame.Stack[secondToLastIdx] = std::move(back);
    } break;
    case InstructionCode::Dup: {
        if (frame.Stack.Size() < 1)
            return RuntimeState::StackUnderflow;
        size_t dupCount = (size_t)(instr.Arg0 > 0 ? instr.Arg0 : 1);
        Value val(frame.Stack.Back());
        for (size_t i = 0; i < dupCount-1; i++)
            frame.Stack.PushBack(val);
        frame.Stack.EmplaceBack(std::move(val));
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
        frame.InstructionIndex = frame.Function->Code.Size();
        break;
    case InstructionCode::ICall: {
        if (frame.Stack.Size() < 1)
            return RuntimeState::StackUnderflow;
        Value funcIdxValue = std::move(frame.Stack.Back());
        frame.Stack.PopBack();
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
    case InstructionCode::BitAnd: {
        if (frame.Stack.Size() < 2)
            return RuntimeState::StackUnderflow;
        Value b = std::move(frame.Stack.Back());
        frame.Stack.PopBack();
        Value& a = frame.Stack.Back();
        if (a.Type() != ValueType::Integer || b.Type() != ValueType::Integer)
            return RuntimeState::TypeError;
        a.SetInteger(a.AsInteger() & b.AsInteger());
    } break;
    case InstructionCode::BitOr: {
        if (frame.Stack.Size() < 2)
            return RuntimeState::StackUnderflow;
        Value b = std::move(frame.Stack.Back());
        frame.Stack.PopBack();
        Value& a = frame.Stack.Back();
        if (a.Type() != ValueType::Integer || b.Type() != ValueType::Integer)
            return RuntimeState::TypeError;
        a.SetInteger(a.AsInteger() | b.AsInteger());
    } break;
    case InstructionCode::BitNot: {
        if (frame.Stack.Size() < 1)
            return RuntimeState::StackUnderflow;
        Value& a = frame.Stack.Back();
        if (a.Type() != ValueType::Integer)
            return RuntimeState::TypeError;
        a.SetInteger(~a.AsInteger());
    } break;
    case InstructionCode::BitXor: {
        if (frame.Stack.Size() < 2)
            return RuntimeState::StackUnderflow;
        Value b = std::move(frame.Stack.Back());
        frame.Stack.PopBack();
        Value& a = frame.Stack.Back();
        if (a.Type() != ValueType::Integer || b.Type() != ValueType::Integer)
            return RuntimeState::TypeError;
        a.SetInteger(a.AsInteger() ^ b.AsInteger());
    } break;
    case InstructionCode::BitShiftLeft: {
        if (frame.Stack.Size() < 2)
            return RuntimeState::StackUnderflow;
        Value b = std::move(frame.Stack.Back());
        frame.Stack.PopBack();
        Value& a = frame.Stack.Back();
        if (a.Type() != ValueType::Integer || b.Type() != ValueType::Integer)
            return RuntimeState::TypeError;
        if (b.AsInteger() < 0)
            a.SetInteger((int64_t)((uint64_t)a.AsInteger() >> -b.AsInteger()));
        else a.SetInteger((int64_t)((uint64_t)a.AsInteger() << b.AsInteger()));
    } break;
    case InstructionCode::BitShiftRight: {
        if (frame.Stack.Size() < 2)
            return RuntimeState::StackUnderflow;
        Value b = std::move(frame.Stack.Back());
        frame.Stack.PopBack();
        Value& a = frame.Stack.Back();
        if (a.Type() != ValueType::Integer || b.Type() != ValueType::Integer)
            return RuntimeState::TypeError;
        if (b.AsInteger() < 0)
            a.SetInteger((int64_t)((uint64_t)a.AsInteger() << -b.AsInteger()));
        else a.SetInteger((int64_t)((uint64_t)a.AsInteger() >> b.AsInteger()));
    } break;
    // TODO: Add floor/double, ceil/double and truncate instructions.
    case InstructionCode::Floor: {
        if (frame.Stack.Size() < 1)
            return RuntimeState::StackUnderflow;
        Value& val = frame.Stack.Back();
        if (!IsNumericValueType(val.Type()))
            return RuntimeState::TypeError;
        if (val.Type() == ValueType::Double)
            val.SetInteger((int64_t)std::floor(val.AsDouble()));
    } break;
    case InstructionCode::Ceil: {
        if (frame.Stack.Size() < 1)
            return RuntimeState::StackUnderflow;
        Value& val = frame.Stack.Back();
        if (!IsNumericValueType(val.Type()))
            return RuntimeState::TypeError;
        if (val.Type() == ValueType::Double)
            val.SetInteger((int64_t)std::ceil(val.AsDouble()));
    } break;
    case InstructionCode::Compare: {
        if (frame.Stack.Size() < 2)
            return RuntimeState::StackUnderflow;
        Value b = std::move(frame.Stack.Back());
        frame.Stack.PopBack();
        Value& a = frame.Stack.Back();

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
        Value b = std::move(frame.Stack.Back());
        frame.Stack.PopBack();
        Value& a = frame.Stack.Back();
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
        Value truthValue = std::move(frame.Stack.Back());
        if (!IsNumericValueType(truthValue.Type()))
            return RuntimeState::TypeError;
        frame.Stack.PopBack();
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
    case InstructionCode::IsEmpty: {
        if (frame.Stack.Size() < 1)
            return RuntimeState::StackUnderflow;
        const Value& list = frame.Stack.Back();
        Value isEmpty;
        if (list.Type() == ValueType::List) {
            isEmpty.SetInteger(list.AsList().Front() ? 0 : 1);
        } else if (list.Type() == ValueType::String) {
            isEmpty.SetInteger(list.AsString().Length() == 0 ? 1 : 0);
        } else return RuntimeState::TypeError;
        frame.Stack.EmplaceBack(std::move(isEmpty));
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
        Value toAppend = std::move(frame.Stack.Back());
        frame.Stack.PopBack();
        Value& list = frame.Stack.Back();
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
        Value toConcat = std::move(frame.Stack.Back());
        frame.Stack.PopBack();
        Value& list = frame.Stack.Back();
        if (toConcat.Type() == ValueType::List && list.Type() == ValueType::List) {
            list.AsList().Concat(std::move(toConcat.AsList()));
        } else return RuntimeState::TypeError;
    } break;
    case InstructionCode::Head: {
        if (frame.Stack.Size() < 1)
            return RuntimeState::StackUnderflow;
        Value& list = frame.Stack.Back();
        if (list.Type() == ValueType::List) {
            ValueList::Node* node = list.AsList().Front();
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
    case InstructionCode::Unpack: {
        if (frame.Stack.Size() < 1)
            return RuntimeState::StackUnderflow;
        if (frame.Stack.Back().Type() != ValueType::List)
            return RuntimeState::TypeError;

        ValueList listToUnpack = std::move(frame.Stack.Back()).AsList();
        frame.Stack.PopBack();

        // If Arg0 <= 0, pop list
        if (instr.Arg0 > 0) {
            size_t unpackCount = (size_t)instr.Arg0;

            ValueList::Node* node = listToUnpack.Back();
            for (size_t i = 1; i < unpackCount && node; i++) {
                node = node->Prev();
            }

            for (size_t i = 0; i < unpackCount; i++) {
                if (!node) return RuntimeState::ListIndexOutOfBounds;
                frame.Stack.PushBack(std::move(node->Value()));
                node = node->Next();
            }
        }
    } break;
    case InstructionCode::Index: {
        if (frame.Stack.Size() < 2)
            return RuntimeState::StackUnderflow;
        Value& list = frame.Stack[frame.Stack.Size()-2];
        Value& index = frame.Stack[frame.Stack.Size()-1];
        if (index.Type() != ValueType::Integer)
            return RuntimeState::TypeError;

        if (list.Type() == ValueType::List) {
            if (index.AsInteger() < 0)
                return RuntimeState::ListIndexOutOfBounds;
            ValueList::Node* node = list.AsList().Front();
            for (size_t i = 0; i < (size_t)index.AsInteger() && node; i++)
                node = node->Next();
            if (!node)
                return RuntimeState::ListIndexOutOfBounds;
            index = node->Value();
        } else if (list.Type() == ValueType::String) {
            if (index.AsInteger() < 0 || (size_t)index.AsInteger() >= list.AsString().Length())
                return RuntimeState::StringIndexOutOfBounds;
            int64_t ch = list.AsString()[(size_t)index.AsInteger()];
            index.SetInteger(ch);
        } else return RuntimeState::TypeError;
    } break;
    case InstructionCode::Prefix: {
        if (frame.Stack.Size() < 2)
            return RuntimeState::StackUnderflow;
        Value& str = frame.Stack[frame.Stack.Size()-2];
        Value& length = frame.Stack[frame.Stack.Size()-1];
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
        Value& str = frame.Stack[frame.Stack.Size()-2];
        Value& length = frame.Stack[frame.Stack.Size()-1];
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
        Value endIdx = frame.Stack.Back();
        if (endIdx.Type() != ValueType::Integer)
            return RuntimeState::TypeError;
        frame.Stack.PopBack();
        Value& startIdx = frame.Stack[frame.Stack.Size()-1];
        if (startIdx.Type() != ValueType::Integer)
            return RuntimeState::TypeError;
        Value& str = frame.Stack[frame.Stack.Size()-2];

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
        ValueType valueType = frame.Stack.Back().Type();
        frame.Stack.EmplaceBack()
            .SetInteger(_InstrTypeCheck(instr.Code, valueType) ? 1 : 0);
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
    case RuntimeState::OutOfBoundsGlobalIndex:
        return "OutOfBoundsGlobalIndex";
    case RuntimeState::WritingOnConstantGlobal:
        return "WritingOnConstantGlobal";
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
    case RuntimeState::StringIndexOutOfBounds:
        return "StringIndexOutOfBounds";
    case RuntimeState::NoCustomTypeData:
        return "NoCustomTypeData";
    case RuntimeState::InvalidCustomTypeHandle:
        return "InvalidCustomTypeHandle";
    case RuntimeState::InvalidCustomTypeReference:
        return "InvalidCustomTypeReference";
    }
    return "Unknown";
}
