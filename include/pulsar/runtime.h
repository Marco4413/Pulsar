#ifndef _PULSAR_RUNTIME_H
#define _PULSAR_RUNTIME_H

#include <cinttypes>
#include <functional>
#include <string>
#include <vector>

namespace Pulsar
{
    enum class InstructionCode {
        PushInt,
        PushDbl,
        PushLocal,
        PopIntoLocal,
        Call,
        CallNative,
        Return,
        DynSum,
        DynSub,
        DynMul,
        Compare,
        Jump,
        JumpIfZero,
        JumpIfNotZero,
        JumpIfGreaterThanZero,
        JumpIfGreaterThanOrEqualToZero,
        JumpIfLessThanZero,
        JumpIfLessThanOrEqualToZero,
    };

    inline bool ShouldJump(InstructionCode jmpInstr, double val)
    {
        switch (jmpInstr) {
        case InstructionCode::Jump:
            return true;
        case InstructionCode::JumpIfZero:
            return val == 0.0;
        case InstructionCode::JumpIfNotZero:
            return val != 0.0;
        case InstructionCode::JumpIfGreaterThanZero:
            return val > 0.0;
        case InstructionCode::JumpIfGreaterThanOrEqualToZero:
            return val >= 0.0;
        case InstructionCode::JumpIfLessThanZero:
            return val < 0.0;
        case InstructionCode::JumpIfLessThanOrEqualToZero:
            return val <= 0.0;
        default:
            return false;
        }
    }

    inline bool ShouldJump(InstructionCode jmpInstr, int64_t val)
    {
        switch (jmpInstr) {
        case InstructionCode::Jump:
            return true;
        case InstructionCode::JumpIfZero:
            return val == 0;
        case InstructionCode::JumpIfNotZero:
            return val != 0;
        case InstructionCode::JumpIfGreaterThanZero:
            return val > 0;
        case InstructionCode::JumpIfGreaterThanOrEqualToZero:
            return val >= 0;
        case InstructionCode::JumpIfLessThanZero:
            return val < 0;
        case InstructionCode::JumpIfLessThanOrEqualToZero:
            return val <= 0;
        default:
            return false;
        }
    }

    struct Instruction
    {
        InstructionCode Code;
        int64_t Arg0 = 0;
    };

    struct DebugSymbol
    {
        Pulsar::Token Token;
    };

    struct BlockDebugSymbol : public DebugSymbol
    {
        BlockDebugSymbol(const Pulsar::Token& token, size_t startIdx)
            : DebugSymbol{token}, StartIdx(startIdx) { }
        size_t StartIdx;
    };

    struct FunctionDefinition
    {
        std::string Name;
        size_t Arity;
        size_t Returns;
        
        size_t LocalsCount = Arity;
        std::vector<Instruction> Code = {};
        
        DebugSymbol FunctionDebugSymbol{Token(TokenType::None)};
        std::vector<BlockDebugSymbol> CodeDebugSymbols = {};

        bool HasDebugSymbol() const { return FunctionDebugSymbol.Token.Type != TokenType::None; }
        bool HasCodeDebugSymbols() const { return CodeDebugSymbols.size() > 0; }

        bool MatchesDeclaration(const FunctionDefinition& other) const
        {
            return Arity == other.Arity
                && LocalsCount == other.LocalsCount
                && Returns == other.Returns
                && Name == other.Name;
        }
    };

    enum class ValueType
    {
        Void, Integer, Double
    };

    bool IsNumericValueType(ValueType vtype);

    struct Value
    {
        Value()
            : Type(ValueType::Void), AsInteger(0) { }
        Value(int64_t val)
            : Type(ValueType::Integer), AsInteger(val) { }
        Value(double val)
            : Type(ValueType::Double), AsDouble(val) { }

        ValueType Type;
        union
        {
            int64_t AsInteger;
            double AsDouble;
        };
    };

    typedef std::vector<Value> Stack;

    struct Frame
    {
        const FunctionDefinition& Function;
        std::vector<Value> Locals = {};
        Stack OperandStack = {};
        size_t InstructionIndex = 0;
    };

    struct Module; // Forward Declaration
    struct ExecutionContext
    {
        const Module& OwnerModule;
        std::vector<Frame> CallStack;

        Frame* GetCallingFrame() {
            if (CallStack.size() <= 1)
                return nullptr;
            return &CallStack[CallStack.size()-2];
        }

        const Frame* GetCallingFrame() const {
            if (CallStack.size() <= 1)
                return nullptr;
            return &CallStack[CallStack.size()-2];
        }

        Frame& GetCurrentFrame()             { return CallStack.back(); }
        const Frame& GetCurrentFrame() const { return CallStack.back(); }
        bool IsAtEnd() const {
            return CallStack.size() < 1
                || (CallStack.size() == 1 && CallStack[0].InstructionIndex >= CallStack[0].Function.Code.size());
        }
    };

    enum class RuntimeState
    {
        OK = 0,
        Error = 1,
        TypeError,
        StackOverflow,
        StackUnderflow,
        OutOfBoundsLocalIndex,
        OutOfBoundsFunctionIndex,
        CallStackUnderflow,
        NativeFunctionBindingsMismatch,
        UnboundNativeFunction,
        FunctionNotFound
    };

    const char* RuntimeStateToString(RuntimeState rstate);

    struct Module
    {
    public:
        RuntimeState CallFunction(int64_t funcIdx, Stack& stack) const;
        RuntimeState CallFunctionByName(const std::string& name, Stack& stack) const;
        RuntimeState CallFunctionByDefinition(const FunctionDefinition& def, Stack& stack) const;

        typedef std::function<RuntimeState(ExecutionContext&)> NativeFunction;
        size_t BindNativeFunction(const FunctionDefinition& def, NativeFunction func);

    public:
        std::vector<FunctionDefinition> Functions;
        std::vector<FunctionDefinition> NativeBindings;
        std::vector<NativeFunction> NativeFunctions;
    private:
        RuntimeState ExecuteInstruction(Frame& frame, ExecutionContext& eContext) const;
    };
}

#endif // _PULSAR_RUNTIME_H
