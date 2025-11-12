#include "pulsar-tools/bindings/error.h"

PulsarTools::Bindings::Error::Error() :
    IBinding()
{
    BindNativeFunction({ "error!",      0, 0 }, FError);
    BindNativeFunction({ "error/type!", 0, 0 }, FType);
    BindNativeFunction({ "scall",       2, 2 }, FSafeCall);
    BindNativeFunction({ "tcall",       2, 2 }, FTryCall);
}

Pulsar::RuntimeState PulsarTools::Bindings::Error::FError(Pulsar::ExecutionContext& eContext)
{
    eContext.GetCallStack().PopFrame();
    return Pulsar::RuntimeState::Error;
}

Pulsar::RuntimeState PulsarTools::Bindings::Error::FType(Pulsar::ExecutionContext& eContext)
{
    eContext.GetCallStack().PopFrame();
    return Pulsar::RuntimeState::TypeError;
}

Pulsar::RuntimeState PulsarTools::Bindings::Error::FSafeCall(Pulsar::ExecutionContext& eContext)
{
    Pulsar::Frame& frame = eContext.CurrentFrame();

    Pulsar::Value& functionReference = frame.Locals[1];
    if (functionReference.Type() != Pulsar::ValueType::FunctionReference)
        return Pulsar::RuntimeState::TypeError;

    Pulsar::Value& childStack = frame.Locals[0];
    if (childStack.Type() != Pulsar::ValueType::List)
        return Pulsar::RuntimeState::TypeError;

    int64_t functionIdx = functionReference.AsInteger();

    Pulsar::ExecutionContext safeContext = eContext.Fork();

    safeContext.GetStack() = Pulsar::Stack(std::move(childStack.AsList()));
    safeContext.CallFunction(functionIdx);
    Pulsar::RuntimeState callState = safeContext.Run();
    if (callState == Pulsar::RuntimeState::OK) {
        Pulsar::Value::List& childStackAsList = childStack.AsList();
        for (Pulsar::Value& value : safeContext.GetStack())
            childStackAsList.Append(std::move(value));
    }

    frame.Stack.Push(std::move(childStack));
    frame.Stack.EmplaceInteger((int64_t)callState);

    return Pulsar::RuntimeState::OK;
}

Pulsar::RuntimeState PulsarTools::Bindings::Error::FTryCall(Pulsar::ExecutionContext& eContext)
{
    Pulsar::Frame& frame = eContext.CurrentFrame();

    Pulsar::Value& functionReference = frame.Locals[1];
    if (functionReference.Type() != Pulsar::ValueType::FunctionReference)
        return Pulsar::RuntimeState::TypeError;
    Pulsar::Value& childStack = frame.Locals[0];
    if (childStack.Type() != Pulsar::ValueType::List)
        return Pulsar::RuntimeState::TypeError;

    int64_t functionIdx = functionReference.AsInteger();

    Pulsar::ExecutionContext context(eContext.GetModule(), false);
    context.GetAllCustomTypeGlobalData() = std::move(eContext.GetAllCustomTypeGlobalData());
    context.GetGlobals() = std::move(eContext.GetGlobals());

    context.GetStack() = Pulsar::Stack(std::move(childStack.AsList()));
    context.CallFunction(functionIdx);
    Pulsar::RuntimeState callState = context.Run();
    if (callState == Pulsar::RuntimeState::OK) {
        Pulsar::Value::List& childStackAsList = childStack.AsList();
        for (Pulsar::Value& value : context.GetStack())
            childStackAsList.Append(std::move(value));
    }

    frame.Stack.Push(std::move(childStack));
    frame.Stack.EmplaceInteger((int64_t)callState);

    eContext.GetAllCustomTypeGlobalData() = std::move(context.GetAllCustomTypeGlobalData());
    eContext.GetGlobals() = std::move(context.GetGlobals());

    return Pulsar::RuntimeState::OK;
}
