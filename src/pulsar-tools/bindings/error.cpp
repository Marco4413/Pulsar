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
    eContext.CallStack.PopBack();
    return Pulsar::RuntimeState::Error;
}

Pulsar::RuntimeState PulsarTools::ErrorNativeBindings::Error_Type(Pulsar::ExecutionContext& eContext)
{
    eContext.CallStack.PopBack();
    return Pulsar::RuntimeState::TypeError;
}

Pulsar::RuntimeState PulsarTools::ErrorNativeBindings::Error_SafeCall(Pulsar::ExecutionContext& eContext)
{
    Pulsar::Frame& frame = eContext.CallStack.CurrentFrame();
    Pulsar::Value& funcRef = frame.Locals[1];
    if (funcRef.Type() != Pulsar::ValueType::FunctionReference)
        return Pulsar::RuntimeState::TypeError;
    Pulsar::Value& args = frame.Locals[0];
    if (args.Type() != Pulsar::ValueType::List)
        return Pulsar::RuntimeState::TypeError;

    int64_t funcIdx = funcRef.AsInteger();
    Pulsar::ValueStack stack;
    {
        Pulsar::ValueList::NodeType* node = args.AsList().Front();
        while (node) {
            stack.EmplaceBack(std::move(node->Value()));
            node = node->Next();
        }
        args.AsList().Clear();
    }

    Pulsar::ExecutionContext ctx = eContext.OwnerModule->CreateExecutionContext(false, true);
    ctx.Globals = eContext.Globals;

    Pulsar::RuntimeState state = eContext.OwnerModule->CallFunction(funcIdx, stack, ctx);
    for (size_t i = 0; i < stack.Size(); i++)
        args.AsList().Append()->Value() = std::move(stack[i]);
    
    frame.Stack.EmplaceBack(std::move(args));
    frame.Stack.EmplaceBack()
        .SetInteger((int64_t)state);
    return Pulsar::RuntimeState::OK;
}

Pulsar::RuntimeState PulsarTools::ErrorNativeBindings::Error_TryCall(Pulsar::ExecutionContext& eContext)
{
    Pulsar::Frame& frame = eContext.CallStack.CurrentFrame();
    Pulsar::Value& funcRef = frame.Locals[1];
    if (funcRef.Type() != Pulsar::ValueType::FunctionReference)
        return Pulsar::RuntimeState::TypeError;
    Pulsar::Value& args = frame.Locals[0];
    if (args.Type() != Pulsar::ValueType::List)
        return Pulsar::RuntimeState::TypeError;

    int64_t funcIdx = funcRef.AsInteger();
    Pulsar::ValueStack stack;
    {
        Pulsar::ValueList::NodeType* node = args.AsList().Front();
        while (node) {
            stack.EmplaceBack(std::move(node->Value()));
            node = node->Next();
        }
        args.AsList().Clear();
    }

    Pulsar::ExecutionContext ctx = eContext.OwnerModule->CreateExecutionContext(false, false);
    ctx.CustomTypeData = eContext.CustomTypeData;
    ctx.Globals = std::move(eContext.Globals);

    Pulsar::RuntimeState state = eContext.OwnerModule->CallFunction(funcIdx, stack, ctx);
    if (state == Pulsar::RuntimeState::OK) {
        for (size_t i = 0; i < stack.Size(); i++)
            args.AsList().Append()->Value() = std::move(stack[i]);
    }
    
    frame.Stack.EmplaceBack(std::move(args));
    frame.Stack.EmplaceBack()
        .SetInteger((int64_t)state);
    eContext.Globals = std::move(ctx.Globals);
    return Pulsar::RuntimeState::OK;
}
