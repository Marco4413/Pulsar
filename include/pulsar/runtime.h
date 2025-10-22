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

    using ValueStack = List<Value>;
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

    // Extend this class to store your custom type's global data
    // TODO: Maybe rename to CustomTypeGlobalData
    class CustomTypeData
    {
    public:
        using Ref = SharedRef<Pulsar::CustomTypeData>;

        virtual ~CustomTypeData() = default;
        /**
         * When a new sandboxed ExecutionContext is created from an already existing one
         *  this function is called.
         * This function must return a "copy" of the data which won't affect the original
         *  context.
         * If this function returns nullptr, it means that the data held doesn't require
         *  to be sandboxed, this may be useful for stats tracking.
         * It's advised to make the data thread-safe if it's meant to be shared.
         */
        virtual Ref Fork() const = 0;
    };

    struct CustomType
    {
        using DataFactoryFn = std::function<CustomTypeData::Ref()>;

        String Name;
        // Method that generates a new instance of a derived class from CustomTypeData
        DataFactoryFn DataFactory = nullptr;
    };

    class ExecutionContext; // Forward Declaration

    /**
     * This class represents an executable Pulsar program.
     * See ExecutionContext for program execution.
     */
    class Module
    {
    public:
        // Returned by Find* functions to indicate that the item was not found.
        static constexpr size_t INVALID_INDEX = size_t(-1);

    public:
        Module() = default;
        ~Module() = default;

        Module(const Module&) = default;
        Module(Module&&) = default;

        Module& operator=(const Module&) = default;
        Module& operator=(Module&&) = default;

        using NativeFunction = std::function<RuntimeState(ExecutionContext&)>;
        // Returns how many definitions were bound.
        size_t BindNativeFunction(const FunctionDefinition& def, NativeFunction func);
        size_t BindNativeFunction(const FunctionSignature& sig, NativeFunction func);
        // Returns the index of the newly declared function.
        size_t DeclareAndBindNativeFunction(const FunctionDefinition& def, NativeFunction func);
        size_t DeclareAndBindNativeFunction(FunctionDefinition&& def, NativeFunction func);
        size_t DeclareAndBindNativeFunction(const FunctionSignature& sig, NativeFunction func);

        uint64_t BindCustomType(const String& name, CustomType::DataFactoryFn dataFactory = nullptr);

        // Be sure to check if the type exists first (unless you know for sure it exists)
        CustomType& GetCustomType(uint64_t typeId)             { return CustomTypes.Find(typeId)->Value(); }
        const CustomType& GetCustomType(uint64_t typeId) const { return CustomTypes.Find(typeId)->Value(); }
        bool HasCustomType(uint64_t typeId) const              { return CustomTypes.Find(typeId); }

        bool HasSourceDebugSymbols() const { return !SourceDebugSymbols.IsEmpty(); }

        template<typename T>
        size_t FindDefinitionByName(const List<T>& definitions, const String& name) const;

        size_t FindFunctionByName(const String& name) const { return FindDefinitionByName(Functions, name); }
        size_t FindNativeByName(const String& name)   const { return FindDefinitionByName(NativeBindings, name); }
        size_t FindGlobalByName(const String& name)   const { return FindDefinitionByName(Globals, name); }

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
        uint64_t m_LastTypeId = 0;
    };

    /**
     * This class is the Pulsar VM.
     * Its job is running a valid Pulsar Module.
     * If any member function that returns `RuntimeState` returns a value `!= RuntimeState::OK`, this context is no longer usable.
     * 
```cpp
ExecutionContext context(module);
ValueStack& stack = context.GetStack();
// Push arguments into stack
context.CallFunction("main");
RuntimeState state = context.Run();
if (state != RuntimeState::OK) // ERROR
// stack contains the return values
```
     */
    class ExecutionContext
    {
    public:
        // typeId -> typeData
        using CustomTypeDataMap = HashMap<uint64_t, CustomTypeData::Ref>;

        /**
         * Create a new ExecutionContext from a specified module.
         * If `init` is true, the `::Init()` method is called.
         */
        ExecutionContext(const Module& module, bool init=true);
        ~ExecutionContext() = default;

        /**
         * Initializes this context's data. It's the same as calling all Init* methods.
         * This function MUST NOT be called within native functions unless you know what you're doing.
         */
        void Init();

        void InitGlobals();
        void InitCustomTypeData();

        /**
         * Creates a new "child" ExecutionContext which inherits global data from this one.
         * Global data is CustomTypeData and Globals. Any change to the forked context
         *  won't affect the original one (except for CustomTypeData that explicitly wants that).
         */
        ExecutionContext Fork() const;

        /**
         * Retrieves and casts to the correct type an instance of CustomTypeData.
         * If the type does not exist, nullptr is returned.
         */
        template<typename T>
        SharedRef<T> GetCustomTypeData(uint64_t typeId)
        {
            auto typeDataPair = m_CustomTypeData.Find(typeId);
            if (!typeDataPair) return nullptr;
            return typeDataPair->Value().CastTo<T>();
        }

        /**
         * Sets type data for the provided type.
         * If the provided type does not exist, nothing is set.
         */
        bool SetCustomTypeData(uint64_t typeId, CustomTypeData::Ref typeData)
        {
            auto typeDataPair = m_CustomTypeData.Find(typeId);
            if (typeDataPair) typeDataPair->Value() = typeData;
            return (bool)typeDataPair;
        }

        // Do not insert new or remove keys from the returned map.
        CustomTypeDataMap& GetAllCustomTypeData()             { return m_CustomTypeData; }
        const CustomTypeDataMap& GetAllCustomTypeData() const { return m_CustomTypeData; }

        // Do not add or remove values from the returned list.
        List<GlobalInstance>& GetGlobals()             { return m_Globals; }
        const List<GlobalInstance>& GetGlobals() const { return m_Globals; }

        // Generates the trace for a single call in the CallStack.
        String GetCallTrace(size_t callIdx) const;
        // Generates the trace for `maxDepth` calls in the CallStack.
        // Set `maxDepth` to -1 for "infinite depth".
        String GetStackTrace(size_t maxDepth) const;

        RuntimeState CallFunction(const String& funcName);
        RuntimeState CallFunction(int64_t funcIdx);

        /**
         * Pushes the specified function into the CallStack and moves its arguments.
         * If RuntimeState::OK is returned, the next call to `::Step()` will start executing the function.
         */
        RuntimeState CallFunction(const FunctionDefinition& funcDef);

        const Module& GetModule() const { return m_Module; }

        ValueStack& GetStack()             { return m_Stack; }
        const ValueStack& GetStack() const { return m_Stack; }

        CallStack& GetCallStack()             { return m_CallStack; }
        const CallStack& GetCallStack() const { return m_CallStack; }

        // An alias to `::GetCallStack().CurrentFrame()`.
        // Use this only if you know that CallStack is not empty.
        // i.e. Inside a native function call.
        Frame& CurrentFrame()
        {
            PULSAR_ASSERT(!m_CallStack.IsEmpty(), "Trying to get current frame of an empty CallStack.");
            return m_CallStack.CurrentFrame();
        }

        bool IsRunning() const { return m_Running; }
        RuntimeState GetState() const { return m_State; }

        bool IsDone() const { return m_CallStack.IsEmpty(); }

        /**
         * Steps through the current instruction.
         * If `::IsRunning()` returns true, this function behaves like `::GetState()`.
         */
        RuntimeState Step()
        {
            if (m_Running || IsDone() || m_State != RuntimeState::OK)
                return m_State;

            m_Running = true;
            InternalStep();
            m_Running = false;

            if (m_StopRequested)
                m_StopRequested = false;

            return m_State;
        }

        /**
         * Runs the context untill `::IsDone()` or an error.
         * If `::IsRunning()` returns true, this function behaves like `::GetState()`.
         */
        RuntimeState Run()
        {
            if (m_Running || IsDone() || m_State != RuntimeState::OK)
                return m_State;

            m_Running = true;
            while (!IsDone() && m_State == RuntimeState::OK && !m_StopRequested) {
                InternalStep();
            }
            m_Running = false;

            if (m_StopRequested)
                m_StopRequested = false;

            return m_State;
        }

        /**
         * This function may be called at any point. Calls to `::Run()` will
         * terminate as soon as the current instruction terminates execution.
         * Useful to create "breakpoint" native functions.
         */
        void Stop()
        {
            if (m_Running && !m_StopRequested) {
                m_StopRequested = true;
            }
        }

    private:
        // Must be called only if
        // - IsDone() returns false
        // - m_State is OK
        void InternalStep();
        RuntimeState ExecuteInstruction(Frame& frame);

    private:
        const Module& m_Module;
        Pulsar::ValueStack m_Stack;
        Pulsar::CallStack m_CallStack;
        List<GlobalInstance> m_Globals;
        CustomTypeDataMap m_CustomTypeData;

        bool m_Running = false;
        bool m_StopRequested = false;
        RuntimeState m_State = RuntimeState::OK;
    };
}

template<typename T>
size_t Pulsar::Module::FindDefinitionByName(const List<T>& definitions, const String& name) const
{
    for (size_t i = definitions.Size(); i > 0; --i) {
        const T& definition = definitions[i-1];
        if (definition.Name != name)
            continue;
        return i-1;
    }
    return INVALID_INDEX;
}

#endif // _PULSAR_RUNTIME_H
