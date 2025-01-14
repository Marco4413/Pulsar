#include "pulsar-tools/bindings/error.h"

void PulsarTools::ErrorNativeBindings::BindToModule(Pulsar::Module& module)
{
    module.BindNativeFunction({ "error!",      0, 0 }, Error_Error);
    module.BindNativeFunction({ "error/type!", 0, 0 }, Error_Type);
    module.BindNativeFunction({ "scall",       2, 2 }, Error_SafeCall);
    module.BindNativeFunction({ "tcall",       2, 2 }, Error_TryCall);
}

Pulsar::RuntimeState PulsarTools::ErrorNativeBindings::Error_Error(Pulsar::ExecutionContext& eContext)
{
    eContext.GetCallStack().PopFrame();
    return Pulsar::RuntimeState::Error;
}

Pulsar::RuntimeState PulsarTools::ErrorNativeBindings::Error_Type(Pulsar::ExecutionContext& eContext)
{
    eContext.GetCallStack().PopFrame();
    return Pulsar::RuntimeState::TypeError;
}

Pulsar::RuntimeState PulsarTools::ErrorNativeBindings::Error_SafeCall(Pulsar::ExecutionContext& eContext)
{
    Pulsar::Frame& frame = eContext.CurrentFrame();

    Pulsar::Value& functionReference = frame.Locals[1];
    if (functionReference.Type() != Pulsar::ValueType::FunctionReference)
        return Pulsar::RuntimeState::TypeError;

    Pulsar::Value& functionArguments = frame.Locals[0];
    if (functionArguments.Type() != Pulsar::ValueType::List)
        return Pulsar::RuntimeState::TypeError;

    int64_t functionIdx = functionReference.AsInteger();

    Pulsar::ExecutionContext safeContext(eContext.GetModule(), false);
    safeContext.InitCustomTypeData();
    eContext.GetAllCustomTypeData().ForEach([&safeContext](const auto& b) {
        PULSAR_ASSERT(b.Value(), "Reference to CustomTypeData is nullptr.");
        uint64_t typeId = b.Key();
        Pulsar::CustomTypeData::Ref typeDataCopy = b.Value()->Copy();
        if (typeDataCopy) safeContext.SetCustomTypeData(typeId, typeDataCopy);
    });

    safeContext.GetStack() = Pulsar::ValueStack(std::move(functionArguments.AsList()));
    safeContext.CallFunction(functionIdx);
    Pulsar::RuntimeState callState = safeContext.Run();
    if (callState == Pulsar::RuntimeState::OK)
        functionArguments.AsList() = Pulsar::ValueList(std::move(safeContext.GetStack()));
    
    frame.Stack.EmplaceBack(std::move(functionArguments));
    frame.Stack.EmplaceBack().SetInteger((int64_t)callState);

    return Pulsar::RuntimeState::OK;
}

Pulsar::RuntimeState PulsarTools::ErrorNativeBindings::Error_TryCall(Pulsar::ExecutionContext& eContext)
{
    Pulsar::Frame& frame = eContext.CurrentFrame();

    Pulsar::Value& functionReference = frame.Locals[1];
    if (functionReference.Type() != Pulsar::ValueType::FunctionReference)
        return Pulsar::RuntimeState::TypeError;
    Pulsar::Value& functionArguments = frame.Locals[0];
    if (functionArguments.Type() != Pulsar::ValueType::List)
        return Pulsar::RuntimeState::TypeError;

    int64_t functionIdx = functionReference.AsInteger();

    Pulsar::ExecutionContext ctx(eContext.GetModule(), false);
    ctx.GetAllCustomTypeData() = std::move(eContext.GetAllCustomTypeData());
    ctx.GetGlobals() = std::move(eContext.GetGlobals());

    ctx.GetStack() = Pulsar::ValueStack(std::move(functionArguments.AsList()));
    ctx.CallFunction(functionIdx);
    Pulsar::RuntimeState callState = ctx.Run();
    if (callState == Pulsar::RuntimeState::OK)
        functionArguments.AsList() = Pulsar::ValueList(std::move(ctx.GetStack()));

    frame.Stack.EmplaceBack(std::move(functionArguments));
    frame.Stack.EmplaceBack()
        .SetInteger((int64_t)callState);

    eContext.GetAllCustomTypeData() = std::move(ctx.GetAllCustomTypeData());
    eContext.GetGlobals() = std::move(ctx.GetGlobals());

    return Pulsar::RuntimeState::OK;
}
