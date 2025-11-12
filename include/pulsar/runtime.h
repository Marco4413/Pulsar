#ifndef _PULSAR_RUNTIME_H
#define _PULSAR_RUNTIME_H

#include "pulsar/core.h"

#include "pulsar/runtime/debug.h"
#include "pulsar/runtime/function.h"
#include "pulsar/runtime/global.h"
#include "pulsar/runtime/module.h"
#include "pulsar/runtime/state.h"
#include "pulsar/runtime/value.h"
#include "pulsar/structures/linkedlist.h"
#include "pulsar/structures/list.h"
#include "pulsar/structures/ref.h"

namespace Pulsar
{
    class Stack
    {
    public:
        using ConstIterator   = List<Value>::ConstIterator;
        using MutableIterator = List<Value>::MutableIterator;

        Stack() = default;

        Stack(size_t initCapacity)
            : m_Values(initCapacity) {}

        explicit Stack(const LinkedList<Value>& ll) : m_Values(ll) {}
        explicit Stack(LinkedList<Value>&& ll) : m_Values(std::move(ll)) {}

        explicit Stack(const List<Value>& l) : m_Values(l) {}
        explicit Stack(List<Value>&& l) : m_Values(std::move(l)) {}

        Stack(const Stack& other) = default;
        Stack(Stack&& other) = default;

        ~Stack() = default;

        Stack& operator=(const Stack& other) = default;
        Stack& operator=(Stack&& other) = default;

        void Resize(size_t newSize)      { m_Values.Resize(newSize); }
        void Reserve(size_t newCapacity) { m_Values.Reserve(newCapacity); }

        void Push(const Value& value) { m_Values.PushBack(value); }
        void Push(Value&& value)      { m_Values.PushBack(std::move(value)); }

        // Pushes a new Void Value and returns its reference
        //  (reference is invalidated once something else is
        //  pushed onto the stack).
        Value& Emplace() { return m_Values.EmplaceBack(); }

        Value& EmplaceInteger(int64_t integer) { return Emplace().SetInteger(integer); }
        Value& EmplaceDouble(double doublev)   { return Emplace().SetDouble(doublev); }

        Value& EmplaceFunctionReference(int64_t functionIndex)     { return Emplace().SetFunctionReference(functionIndex); }
        Value& EmplaceNativeFunctionReference(int64_t nativeIndex) { return Emplace().SetNativeFunctionReference(nativeIndex); }

        Value& EmplaceString(const String& string) { return Emplace().SetString(string); }
        Value& EmplaceString(String&& string)      { return Emplace().SetString(std::move(string)); }

        // Pushes an empty list
        Value& EmplaceList()                        { return Emplace().SetList(Value::List()); }
        Value& EmplaceList(const Value::List& list) { return Emplace().SetList(list); }
        Value& EmplaceList(Value::List&& list)      { return Emplace().SetList(std::move(list)); }

        Value& EmplaceCustom(const CustomData& custom) { return Emplace().SetCustom(custom); }

        Value Pop()
        {
            Value value(std::move(m_Values.Back()));
            m_Values.PopBack();
            return value;
        }

        // Access from the bottom of the stack to the top
        Value& operator[](size_t index)
        {
            PULSAR_ASSERT(index < Size(), "Stack index out of bounds.");
            return m_Values[index];
        }

        // Access from the bottom of the stack to the top
        const Value& operator[](size_t index) const
        {
            PULSAR_ASSERT(index < Size(), "Stack index out of bounds.");
            return m_Values[index];
        }

        // index >= 0, access from the bottom of the stack
        // index <  0, access from the top of the stack
        Value& operator[](int index)
        {
            if (index >= 0) return (*this)[static_cast<size_t>(index)];
            size_t reverseIndex = static_cast<size_t>(-index);
            PULSAR_ASSERT(reverseIndex <= Size(), "Stack index out of bounds.");
            return (*this)[Size() - reverseIndex];
        }

        // index >= 0, access from the bottom of the stack
        // index <  0, access from the top of the stack
        const Value& operator[](int index) const
        {
            if (index >= 0) return (*this)[static_cast<size_t>(index)];
            size_t reverseIndex = static_cast<size_t>(-index);
            PULSAR_ASSERT(reverseIndex <= Size(), "Stack index out of bounds.");
            return (*this)[m_Values.Size() - reverseIndex];
        }

        Value& Top()             { return (*this)[-1]; }
        const Value& Top() const { return (*this)[-1]; }

        void Clear() { m_Values.Clear(); }
        size_t Size() const     { return m_Values.Size(); }
        size_t Capacity() const { return m_Values.Capacity(); }
        bool IsEmpty() const    { return m_Values.IsEmpty(); }

        /* Iterators iterate from bottom to top */

        ConstIterator Begin() const { return m_Values.Begin(); }
        ConstIterator End()   const { return m_Values.End(); }
        MutableIterator Begin() { return m_Values.Begin(); }
        MutableIterator End()   { return m_Values.End(); }

        PULSAR_ITERABLE_IMPL(Stack, ConstIterator, MutableIterator);

    private:
        List<Value> m_Values;
    };

    struct Frame
    {
        const FunctionDefinition* Function;
        bool IsNative           = false;
        List<Value> Locals      = List<Value>();
        Pulsar::Stack Stack     = Pulsar::Stack();
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
        RuntimeState PrepareFrame(Frame& frame, Stack& callerStack);

        Frame& PushFrame(Frame&& frame) { return m_Frames.EmplaceBack(std::move(frame)); }
        void PopFrame() { m_Frames.PopBack(); }

        bool IsEmpty() const { return m_Frames.IsEmpty(); }
        size_t Size() const { return m_Frames.Size(); }
        Frame& operator[](size_t i) { return m_Frames[i]; }
        const Frame& operator[](size_t i) const { return m_Frames[i]; }

    private:
        List<Frame> m_Frames;
    };

    /**
     * This class is the Pulsar VM.
     * Its job is running a valid Pulsar Module.
     * If any member function that returns `RuntimeState` returns a value `!= RuntimeState::OK`, this context is no longer usable.
     * 
```cpp
ExecutionContext context(module);
Stack& stack = context.GetStack();
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
        using CustomTypeGlobalDataMap = HashMap<uint64_t, CustomTypeGlobalData::Ref>;
        // Consider using the SourceViewer class for conversion.
        using PositionConverterFn = std::function<SourcePosition(const SourceDebugSymbol& source, SourcePosition position)>;

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
        void InitCustomTypeGlobalData();

        /**
         * Creates a new "child" ExecutionContext which inherits global data from this one.
         * Global data is CustomTypeGlobalData and Globals. Any change to the forked context
         *  won't affect the original one (except for CustomTypeGlobalData that explicitly wants that).
         */
        ExecutionContext Fork() const;

        /**
         * Retrieves and casts to the correct type an instance of CustomTypeGlobalData.
         * If the type does not exist, nullptr is returned.
         */
        template<typename T>
        SharedRef<T> GetCustomTypeGlobalData(uint64_t typeId)
        {
            auto typeDataPair = m_CustomTypeGlobalData.Find(typeId);
            if (!typeDataPair) return nullptr;
            return typeDataPair->Value().CastTo<T>();
        }

        /**
         * Sets type data for the provided type.
         * If the provided type does not exist, nothing is set.
         */
        bool SetCustomTypeGlobalData(uint64_t typeId, CustomTypeGlobalData::Ref typeData)
        {
            auto typeDataPair = m_CustomTypeGlobalData.Find(typeId);
            if (typeDataPair) typeDataPair->Value() = typeData;
            return (bool)typeDataPair;
        }

        // Do not insert new or remove keys from the returned map.
        CustomTypeGlobalDataMap& GetAllCustomTypeGlobalData()             { return m_CustomTypeGlobalData; }
        const CustomTypeGlobalDataMap& GetAllCustomTypeGlobalData() const { return m_CustomTypeGlobalData; }

        // Do not add or remove values from the returned list.
        List<GlobalInstance>& GetGlobals()             { return m_Globals; }
        const List<GlobalInstance>& GetGlobals() const { return m_Globals; }

        // Generates the trace for a single call in the CallStack.
        // Provide a `positionConverter` to convert from UTF32 0-indexed positions to other position kinds.
        //  The default behaviour is to convert 0-indexed UTF32 to 1-indexed.
        String GetCallTrace(size_t callIdx, const PositionConverterFn& positionConverter=nullptr) const;
        // Generates the trace for `maxDepth` calls in the CallStack.
        // Set `maxDepth` to -1 for "infinite depth".
        // Provide a `positionConverter` to convert from UTF32 0-indexed positions to other position kinds.
        //  The default behaviour is to convert 0-indexed UTF32 to 1-indexed.
        String GetStackTrace(size_t maxDepth, const PositionConverterFn& positionConverter=nullptr) const;

        RuntimeState CallFunction(const String& funcName);
        RuntimeState CallFunction(FunctionSignature funcSig);
        // TODO: Add `typedef size_t index_t;` within Module?
        //       This would require a big refactor to make sure we're not using size_t anymore.
        RuntimeState CallFunction(size_t funcIdx);
        RuntimeState CallFunction(int64_t funcIdx);

        /**
         * Pushes the specified function into the CallStack and moves its arguments.
         * If RuntimeState::OK is returned, the next call to `::Step()` will start executing the function.
         */
        RuntimeState CallFunction(const FunctionDefinition& funcDef);

        const Module& GetModule() const { return m_Module; }

        Stack& GetStack()             { return m_Stack; }
        const Stack& GetStack() const { return m_Stack; }

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
        Pulsar::Stack m_Stack;
        Pulsar::CallStack m_CallStack;
        List<GlobalInstance> m_Globals;
        CustomTypeGlobalDataMap m_CustomTypeGlobalData;

        bool m_Running = false;
        bool m_StopRequested = false;
        RuntimeState m_State = RuntimeState::OK;
    };
}

#endif // _PULSAR_RUNTIME_H
