#include "pulsar-tools/bindings/module.h"

#include "pulsar/parser.h"

void PulsarTools::ModuleNativeBindings::BindToModule(Pulsar::Module& module)
{
    uint64_t type = module.BindCustomType("Module", []() {
        return std::make_shared<ModuleTypeData>();
    });
    module.BindNativeFunction({ "module/from-file", 1, 1 },
        [type](auto& ctx) { return Module_FromFile(ctx, type); });
    module.BindNativeFunction({ "module/run", 1, 1 },
        [type](auto& ctx) { return Module_Run(ctx, type); });
    module.BindNativeFunction({ "module/free!", 1, 0 },
        [type](auto& ctx) { return Module_Free(ctx, type); });
    module.BindNativeFunction({ "module/valid?", 1, 1 },
        [type](auto& ctx) { return Module_IsValid(ctx, type); });
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
    
    auto moduleData = eContext.GetCustomTypeData<ModuleTypeData>(type);
    if (!moduleData)
        return Pulsar::RuntimeState::NoCustomTypeData;
    
    int64_t handle = moduleData->NextHandle++;
    moduleData->Modules.Emplace(handle, std::move(module));

    frame.Stack.EmplaceBack()
        .SetCustom({ .Type=type, .Handle=handle });
    return Pulsar::RuntimeState::OK;
}

Pulsar::RuntimeState PulsarTools::ModuleNativeBindings::Module_Run(Pulsar::ExecutionContext& eContext, uint64_t type)
{
    Pulsar::Frame& frame = eContext.CallStack.CurrentFrame();
    Pulsar::Value& moduleHandle = frame.Locals[0];
    if (moduleHandle.Type() != Pulsar::ValueType::Custom
        || moduleHandle.AsCustom().Type != type)
        return Pulsar::RuntimeState::TypeError;
    
    auto moduleData = eContext.GetCustomTypeData<ModuleTypeData>(type);
    if (!moduleData)
        return Pulsar::RuntimeState::NoCustomTypeData;

    auto handleModulePair = moduleData->Modules.Find(moduleHandle.AsCustom().Handle);
    if (!handleModulePair)
        return Pulsar::RuntimeState::InvalidCustomTypeHandle;
    // The module handle is not actually thread-safe but neither is the VM
    const Pulsar::Module& module = *handleModulePair.Value;

    Pulsar::ValueStack stack;
    Pulsar::ExecutionContext context = module.CreateExecutionContext();
    auto runtimeState = module.CallFunctionByName("main", stack, context);
    if (runtimeState != Pulsar::RuntimeState::OK)
        return runtimeState;

    Pulsar::ValueList retValues(std::move(stack));
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

    auto moduleData = eContext.GetCustomTypeData<ModuleTypeData>(type);
    if (!moduleData)
        return Pulsar::RuntimeState::NoCustomTypeData;
    moduleData->Modules.Remove(moduleHandle.AsCustom().Handle);

    return Pulsar::RuntimeState::OK;
}

Pulsar::RuntimeState PulsarTools::ModuleNativeBindings::Module_IsValid(Pulsar::ExecutionContext& eContext, uint64_t type)
{
    Pulsar::Frame& frame = eContext.CallStack.CurrentFrame();
    Pulsar::Value& moduleHandle = frame.Locals[0];
    if (moduleHandle.Type() != Pulsar::ValueType::Custom
        || moduleHandle.AsCustom().Type != type)
        return Pulsar::RuntimeState::TypeError;

    if (moduleHandle.AsCustom().Handle == 0) {
        frame.Stack.EmplaceBack().SetInteger(0);
        return Pulsar::RuntimeState::OK;
    }

    auto moduleData = eContext.GetCustomTypeData<ModuleTypeData>(type);
    if (!moduleData)
        return Pulsar::RuntimeState::NoCustomTypeData;

    auto handleModulePair = moduleData->Modules.Find(moduleHandle.AsCustom().Handle);
    frame.Stack.EmplaceBack()
        .SetInteger(handleModulePair ? 1 : 0);
    return Pulsar::RuntimeState::OK;
}
