#ifndef _PULSAR_RUNTIME_H
#define _PULSAR_RUNTIME_H

#include "pulsar/core.h"

#include "pulsar/runtime/debug.h"
#include "pulsar/runtime/function.h"
#include "pulsar/runtime/global.h"
#include "pulsar/runtime/value.h"
#include "pulsar/structures/list.h"

namespace Pulsar
{
    enum class RuntimeState
    {
        OK = 0,
        Error = 1,
        TypeError,
        StackOverflow,
        StackUnderflow,
        OutOfBoundsConstantIndex,
        OutOfBoundsLocalIndex,
        OutOfBoundsGlobalIndex,
        WritingOnConstantGlobal,
        OutOfBoundsFunctionIndex,
        CallStackUnderflow,
        NativeFunctionBindingsMismatch,
        UnboundNativeFunction,
        FunctionNotFound,
        ListIndexOutOfBounds
    };

    const char* RuntimeStateToString(RuntimeState rstate);

    typedef List<Value> ValueStack;
    struct Frame
    {
        const FunctionDefinition* Function;
        bool IsNative = false;
        List<Value> Locals = List<Value>();
        ValueStack Stack = ValueStack();
        size_t InstructionIndex = 0;
    };

    class CallStack : public List<Frame>
    {
    public:
        CallStack()
            : List<Frame>() { }
        
        bool HasCaller() const            { return Size() > 1; }
        Frame& CallingFrame()             { return (*this)[Size()-2]; }
        const Frame& CallingFrame() const { return (*this)[Size()-2]; }
        Frame& CurrentFrame()             { return (*this).Back(); }
        const Frame& CurrentFrame() const { return (*this).Back(); }
    };

    struct Module; // Forward Declaration
    struct ExecutionContext
    {
        const Module* OwnerModule;
        Pulsar::CallStack CallStack;
        List<GlobalInstance> Globals;

        String GetCallTrace(size_t callIdx) const;
        String GetStackTrace(size_t maxDepth) const;

        bool IsAtEnd() const {
            return CallStack.IsEmpty() || (
                CallStack.Size() == 1 &&
                CallStack[0].InstructionIndex >= CallStack[0].Function->Code.Size()
            );
        }
    };

    struct Module
    {
    public:
        ExecutionContext CreateExecutionContext() const;
        RuntimeState CallFunction(int64_t funcIdx, ValueStack& stack, ExecutionContext& context) const;
        RuntimeState CallFunctionByName(const String& name, ValueStack& stack, ExecutionContext& context) const;
        RuntimeState CallFunctionByDefinition(const FunctionDefinition& def, ValueStack& stack, ExecutionContext& context) const;

        RuntimeState ExecuteFunction(const FunctionDefinition& def, ValueStack& stack, ExecutionContext& context) const;

        typedef std::function<RuntimeState(ExecutionContext&)> NativeFunction;
        size_t BindNativeFunction(const FunctionDefinition& def, NativeFunction func);
        size_t DeclareAndBindNativeFunction(FunctionDefinition def, NativeFunction func);
        uint64_t BindCustomType(const String& name) { uint64_t idx = (uint64_t)CustomTypes.Size(); CustomTypes.EmplaceBack(name); return idx; }

        bool HasSourceDebugSymbols() const { return !SourceDebugSymbols.IsEmpty(); }

    public:
        List<FunctionDefinition> Functions;
        List<FunctionDefinition> NativeBindings;
        List<GlobalDefinition> Globals;
        List<NativeFunction> NativeFunctions;
        List<Value> Constants;
        List<String> CustomTypes;

        List<SourceDebugSymbol> SourceDebugSymbols;
    private:
        RuntimeState PrepareCallFrame(ValueStack& callerStack, Frame& callingFrame) const;
        RuntimeState ExecuteInstruction(Frame& frame, ExecutionContext& eContext) const;
    };
}

#endif // _PULSAR_RUNTIME_H
