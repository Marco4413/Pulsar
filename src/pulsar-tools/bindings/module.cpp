#include "pulsar-tools/bindings/module.h"

#include "pulsar/parser.h"

void PulsarTools::ModuleNativeBindings::BindToModule(Pulsar::Module& module)
{
    uint64_t type = module.BindCustomType("ModuleHandle");
    module.BindNativeFunction({ "module/from-file", 1, 1 },
        [&, type](auto& ctx) { return Module_FromFile(ctx, type); });
    module.BindNativeFunction({ "module/run", 1, 2 },
        [&, type](auto& ctx) { return Module_Run(ctx, type); });
    module.BindNativeFunction({ "module/free!", 1, 0 },
        [&, type](auto& ctx) { return Module_Free(ctx, type); });
    module.BindNativeFunction({ "module/valid?", 1, 2 },
        [&, type](auto& ctx) { return Module_IsValid(ctx, type); });
}

Pulsar::RuntimeState PulsarTools::ModuleNativeBindings::Module_FromFile(Pulsar::ExecutionContext& eContext, uint64_t type)
{
    Pulsar::Frame& frame = eContext.CallStack.CurrentFrame();
    Pulsar::Value& modulePath = frame.Locals[0];
    if (modulePath.Type() != Pulsar::ValueType::String)
        return Pulsar::RuntimeState::TypeError;

    Pulsar::Parser parser;
    auto result = parser.AddSourceFile(modulePath.AsString());
    if (result != Pulsar::ParseResult::OK) {
        frame.Stack.EmplaceBack()
            .SetCustom({ .Type=type, .Handle=0 });
        return Pulsar::RuntimeState::OK;
    }
    
    Pulsar::Module module;
    result = parser.ParseIntoModule(module, Pulsar::ParseSettings_Default);
    if (result != Pulsar::ParseResult::OK) {
        frame.Stack.EmplaceBack()
            .SetCustom({ .Type=type, .Handle=0 });
        return Pulsar::RuntimeState::OK;
    }
    
    int64_t handle = m_NextHandle++;
    frame.Stack.EmplaceBack()
        .SetCustom({ .Type=type, .Handle=handle });
    m_Modules.Emplace(handle, std::move(module));
    return Pulsar::RuntimeState::OK;
}

Pulsar::RuntimeState PulsarTools::ModuleNativeBindings::Module_Run(Pulsar::ExecutionContext& eContext, uint64_t type) const
{
    Pulsar::Frame& frame = eContext.CallStack.CurrentFrame();
    Pulsar::Value& moduleHandle = frame.Locals[0];
    if (moduleHandle.Type() != Pulsar::ValueType::Custom
        || moduleHandle.AsCustom().Type != type)
        return Pulsar::RuntimeState::TypeError;

    auto handleModulePair = m_Modules.Find(moduleHandle.AsCustom().Handle);
    if (!handleModulePair)
        return Pulsar::RuntimeState::Error;

    const Pulsar::Module& module = *handleModulePair.Value;
    Pulsar::ValueStack stack;
    Pulsar::ExecutionContext context = module.CreateExecutionContext();
    auto runtimeState = module.CallFunctionByName("main", stack, context);
    if (runtimeState != Pulsar::RuntimeState::OK)
        return Pulsar::RuntimeState::Error;

    frame.Stack.EmplaceBack(moduleHandle);
    Pulsar::ValueList retValues;
    for (size_t i = 0; i < stack.Size(); i++)
        retValues.Append()->Value() = std::move(stack[i]);
    frame.Stack.EmplaceBack()
        .SetList(std::move(retValues));
    return Pulsar::RuntimeState::OK;
}

Pulsar::RuntimeState PulsarTools::ModuleNativeBindings::Module_Free(Pulsar::ExecutionContext& eContext, uint64_t type)
{
    Pulsar::Frame& frame = eContext.CallStack.CurrentFrame();
    Pulsar::Value& moduleHandle = frame.Locals[0];
    if (moduleHandle.Type() != Pulsar::ValueType::Custom
        || moduleHandle.AsCustom().Type != type)
        return Pulsar::RuntimeState::TypeError;

    m_Modules.Remove(moduleHandle.AsCustom().Handle);
    return Pulsar::RuntimeState::OK;
}

Pulsar::RuntimeState PulsarTools::ModuleNativeBindings::Module_IsValid(Pulsar::ExecutionContext& eContext, uint64_t type)
{
    Pulsar::Frame& frame = eContext.CallStack.CurrentFrame();
    Pulsar::Value& moduleHandle = frame.Locals[0];
    if (moduleHandle.Type() != Pulsar::ValueType::Custom
        || moduleHandle.AsCustom().Type != type)
        return Pulsar::RuntimeState::TypeError;
    frame.Stack.EmplaceBack(moduleHandle);
    frame.Stack.EmplaceBack()
        .SetInteger(moduleHandle.AsCustom().Handle);
    return Pulsar::RuntimeState::OK;
}
