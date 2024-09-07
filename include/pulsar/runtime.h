#ifndef _PULSAR_RUNTIME_H
#define _PULSAR_RUNTIME_H

#include "pulsar/core.h"

#include "pulsar/runtime/debug.h"
#include "pulsar/runtime/function.h"
#include "pulsar/runtime/global.h"
#include "pulsar/runtime/value.h"
#include "pulsar/structures/list.h"
#include "pulsar/structures/ref.h"

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
        StringIndexOutOfBounds,
        NoCustomTypeData,
        InvalidCustomTypeHandle,
        InvalidCustomTypeReference,
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

    class CallStack
    {
    public:
        CallStack() = default;
        ~CallStack() = default;

        bool HasCaller() const            { return m_Frames.Size() > 1; }
        Frame& CallingFrame()             { return m_Frames[Size()-2]; }
        const Frame& CallingFrame() const { return m_Frames[Size()-2]; }
        Frame& CurrentFrame()             { return m_Frames.Back(); }
        const Frame& CurrentFrame() const { return m_Frames.Back(); }

        /*
        Calling a function:
        {
            Frame frame = callStack.CreateFrame(&func);
            RuntimeState state = callStack.PrepareFrame(frame);
            if (state != RuntimeState::OK)
                return state;
            callStack.PushFrame(std::move(frame));
        }
        */

        Frame CreateFrame(const FunctionDefinition* def, bool native=false);
        Frame& CreateAndPushFrame(const FunctionDefinition* def, bool native=false);
        RuntimeState PrepareFrame(Frame& frame);
        RuntimeState PrepareFrame(Frame& frame, ValueStack& callerStack);

        Frame& PushFrame(Frame&& frame) { return m_Frames.EmplaceBack(std::move(frame)); }
        void PopFrame() { m_Frames.PopBack(); }

        bool IsEmpty() const { return m_Frames.IsEmpty(); }
        size_t Size() const { return m_Frames.Size(); }
        Frame& operator[](size_t i) { return m_Frames[i]; }
        const Frame& operator[](size_t i) const { return m_Frames[i]; }

    private:
        List<Frame> m_Frames;
    };

    // Extend this class to store your custom type's data
    class CustomTypeData
    {
    public:
        typedef SharedRef<Pulsar::CustomTypeData> Ref_T;
        virtual ~CustomTypeData() = default;
        // This method should create a copy of the data.
        // It may return nullptr meaning that it is not implemented (or not needed).
        // It should be used when creating a snapshot of the ExecutionContext to create a sandbox.
        virtual Ref_T Copy() const { return nullptr; }
    };

    struct CustomType
    {
        typedef std::function<CustomTypeData::Ref_T()> DataFactory_T;
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
        HashMap<uint64_t, Pulsar::CustomTypeData::Ref_T> CustomTypeData;

        // Retrieves and casts to the correct type an instance of CustomTypeData
        template<typename T>
        SharedRef<T> GetCustomTypeData(uint64_t type)
        {
            auto typeDataPair = CustomTypeData.Find(type);
            if (!typeDataPair)
                return nullptr;
            return typeDataPair.Value->CastTo<T>();
        }

        String GetCallTrace(size_t callIdx) const;
        String GetStackTrace(size_t maxDepth) const;

        bool IsAtEnd() const
        {
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

        ExecutionContext CreateExecutionContext(bool initGlobals=true, bool initTypeData=true) const;
        RuntimeState CallFunction(int64_t funcIdx, ValueStack& stack, ExecutionContext& context) const;
        RuntimeState CallFunctionByName(const String& name, ValueStack& stack, ExecutionContext& context) const;
        RuntimeState CallFunctionByDefinition(const FunctionDefinition& def, ValueStack& stack, ExecutionContext& context) const;
        /* Matches only Name, Arity and Returns */
        RuntimeState CallFunctionBySignature(const FunctionDefinition& sig, ValueStack& stack, ExecutionContext& context) const;

        RuntimeState ExecuteFunction(const FunctionDefinition& def, ValueStack& stack, ExecutionContext& context) const;

        typedef std::function<RuntimeState(ExecutionContext&)> NativeFunction;
        // Returns how many definitions were bound.
        size_t BindNativeFunction(const FunctionDefinition& def, NativeFunction func);
        // Returns the index of the newly declared function.
        int64_t DeclareAndBindNativeFunction(FunctionDefinition def, NativeFunction func);
        uint64_t BindCustomType(const String& name, CustomType::DataFactory_T dataFactory = nullptr);

        // Be sure to check if the type exists first (unless you know for sure it exists)
        CustomType& GetCustomType(uint64_t typeId)             { return *CustomTypes.Find(typeId).Value; }
        const CustomType& GetCustomType(uint64_t typeId) const { return *CustomTypes.Find(typeId).Value; }
        bool HasCustomType(uint64_t typeId) const              { return CustomTypes.Find(typeId); }

        bool HasSourceDebugSymbols() const { return !SourceDebugSymbols.IsEmpty(); }

    public:
        // Access these member variables only for:
        // - Inspecting the Module.
        // - Creating your own language which runs on the Pulsar VM.
        List<FunctionDefinition> Functions;
        List<FunctionDefinition> NativeBindings;
        List<GlobalDefinition> Globals;
        List<Value> Constants;

        List<SourceDebugSymbol> SourceDebugSymbols;

        // These are managed by the Bind* methods.
        List<NativeFunction> NativeFunctions;
        HashMap<uint64_t, CustomType> CustomTypes;

    private:
        RuntimeState ExecuteInstruction(Frame& frame, ExecutionContext& eContext) const;

    private:
        uint64_t m_LastTypeId = 0;
    };
}

#endif // _PULSAR_RUNTIME_H
