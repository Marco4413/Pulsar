#ifndef _PULSAR_RUNTIME_H
#define _PULSAR_RUNTIME_H

#include <cinttypes>
#include <functional>
#include <string>
#include <vector>

#include "pulsar/runtime/function.h"
#include "pulsar/runtime/value.h"

namespace Pulsar
{
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

    typedef std::vector<Value> Stack;
    struct Frame
    {
        const FunctionDefinition* Function;
        std::vector<Value> Locals = {};
        Stack OperandStack = {};
        size_t InstructionIndex = 0;
    };

    struct Module; // Forward Declaration
    struct ExecutionContext
    {
        const Module* OwnerModule;
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
            return CallStack.size() < 1 || (
                CallStack.size() == 1 &&
                CallStack[0].InstructionIndex >= CallStack[0].Function->Code.size()
            );
        }
    };

    struct Module
    {
    public:
        ExecutionContext CreateExecutionContext() const { return {this, { }}; }
        RuntimeState CallFunction(int64_t funcIdx, Stack& stack, ExecutionContext& context) const;
        RuntimeState CallFunctionByName(const std::string& name, Stack& stack, ExecutionContext& context) const;
        RuntimeState CallFunctionByDefinition(const FunctionDefinition& def, Stack& stack, ExecutionContext& context) const;

        typedef std::function<RuntimeState(ExecutionContext&)> NativeFunction;
        size_t BindNativeFunction(const FunctionDefinition& def, NativeFunction func);

    public:
        std::vector<FunctionDefinition> Functions;
        std::vector<FunctionDefinition> NativeBindings;
        std::vector<NativeFunction> NativeFunctions;
    private:
        RuntimeState PrepareCallFrame(Stack& callerStack, Frame& callingFrame) const;
        RuntimeState ExecuteInstruction(Frame& frame, ExecutionContext& eContext) const;
    };
}

#endif // _PULSAR_RUNTIME_H
