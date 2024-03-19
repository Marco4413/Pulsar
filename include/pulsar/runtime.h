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
        ListIndexOutOfBounds,
        StringIndexOutOfBounds
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

    // Extend this class to store your custom type's data
    class CustomTypeData
    {
    public:
        typedef std::shared_ptr<Pulsar::CustomTypeData> Ptr_T;
        virtual ~CustomTypeData() = default;
    };

    struct CustomType
    {
        typedef std::function<CustomTypeData::Ptr_T()> DataFactory_T;
        String Name;
        // Method that generates a new instance of a derived class from CustomTypeData
        DataFactory_T DataFactory = nullptr;
    };

    class Module; // Forward Declaration
    struct ExecutionContext
    {
        const Module* OwnerModule;
        Pulsar::CallStack CallStack;
        List<GlobalInstance> Globals;
        // Don't access this directly, use the GetCustomTypeData method
        HashMap<uint64_t, Pulsar::CustomTypeData::Ptr_T> CustomTypeData;

        // Retrieves and casts to the correct type an instance of CustomTypeData
        template<typename T>
        std::shared_ptr<T> GetCustomTypeData(uint64_t type)
        {
            auto typeDataPair = CustomTypeData.Find(type);
            if (!typeDataPair)
                return nullptr;
            return std::static_pointer_cast<T>(*typeDataPair.Value);
        }

        String GetCallTrace(size_t callIdx) const;
        String GetStackTrace(size_t maxDepth) const;

        bool IsAtEnd() const {
            return CallStack.IsEmpty() || (
                CallStack.Size() == 1 &&
                CallStack[0].InstructionIndex >= CallStack[0].Function->Code.Size()
            );
        }
    };

    class Module
    {
    public:
        Module() = default;
        ~Module() = default;

        Module(const Module&) = default;
        Module(Module&&) = default;

        Module& operator=(const Module&) = default;
        Module& operator=(Module&&) = default;

        ExecutionContext CreateExecutionContext() const;
        RuntimeState CallFunction(int64_t funcIdx, ValueStack& stack, ExecutionContext& context) const;
        RuntimeState CallFunctionByName(const String& name, ValueStack& stack, ExecutionContext& context) const;
        RuntimeState CallFunctionByDefinition(const FunctionDefinition& def, ValueStack& stack, ExecutionContext& context) const;

        RuntimeState ExecuteFunction(const FunctionDefinition& def, ValueStack& stack, ExecutionContext& context) const;

        typedef std::function<RuntimeState(ExecutionContext&)> NativeFunction;
        size_t BindNativeFunction(const FunctionDefinition& def, NativeFunction func);
        size_t DeclareAndBindNativeFunction(FunctionDefinition def, NativeFunction func);
        uint64_t BindCustomType(const String& name, CustomType::DataFactory_T dataFactory = nullptr);
        
        // Try not to access CustomTypes directly because it may change in the future.
        CustomType& GetCustomType(uint64_t typeId)             { return CustomTypes[(size_t)typeId]; }
        const CustomType& GetCustomType(uint64_t typeId) const { return CustomTypes[(size_t)typeId]; }
        bool HasCustomType(uint64_t typeId) const              { return (size_t)typeId < CustomTypes.Size(); }

        bool HasSourceDebugSymbols() const { return !SourceDebugSymbols.IsEmpty(); }

    public:
        List<FunctionDefinition> Functions;
        List<FunctionDefinition> NativeBindings;
        List<GlobalDefinition> Globals;
        List<Value> Constants;

        List<SourceDebugSymbol> SourceDebugSymbols;

        List<NativeFunction> NativeFunctions;
        List<CustomType> CustomTypes;

    private:
        RuntimeState PrepareCallFrame(ValueStack& callerStack, Frame& callingFrame) const;
        RuntimeState ExecuteInstruction(Frame& frame, ExecutionContext& eContext) const;
    };
}

#endif // _PULSAR_RUNTIME_H
