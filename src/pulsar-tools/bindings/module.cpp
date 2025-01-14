#include "pulsar-tools/bindings/module.h"

#include "pulsar/parser.h"

void PulsarTools::ModuleNativeBindings::BindToModule(Pulsar::Module& module)
{
    uint64_t type = module.BindCustomType("Pulsar-Tools/Module");
    module.BindNativeFunction({ "module/from-file", 1, 1 },
        [type](auto& ctx) { return Module_FromFile(ctx, type); });
    module.BindNativeFunction({ "module/run", 1, 1 },
        [type](auto& ctx) { return Module_Run(ctx, type); });
    module.BindNativeFunction({ "module/valid?", 1, 1 },
        [type](auto& ctx) { return Module_IsValid(ctx, type); });
}

Pulsar::RuntimeState PulsarTools::ModuleNativeBindings::Module_FromFile(Pulsar::ExecutionContext& eContext, uint64_t type)
{
    Pulsar::Frame& frame = eContext.CurrentFrame();
    Pulsar::Value& modulePath = frame.Locals[0];
    if (modulePath.Type() != Pulsar::ValueType::String)
        return Pulsar::RuntimeState::TypeError;

    Pulsar::Parser parser;
    auto result = parser.AddSourceFile(modulePath.AsString());
    if (result != Pulsar::ParseResult::OK) {
        frame.Stack.EmplaceBack()
            .SetCustom({ type });
        return Pulsar::RuntimeState::OK;
    }
    
    ModuleType::Ref module = ModuleType::Ref::New();
    result = parser.ParseIntoModule(*module, Pulsar::ParseSettings_Default);
    if (result != Pulsar::ParseResult::OK) {
        frame.Stack.EmplaceBack()
            .SetCustom({ type });
        return Pulsar::RuntimeState::OK;
    }

    frame.Stack.EmplaceBack()
        .SetCustom({ .Type=type, .Data=module });
    return Pulsar::RuntimeState::OK;
}

Pulsar::RuntimeState PulsarTools::ModuleNativeBindings::Module_Run(Pulsar::ExecutionContext& eContext, uint64_t type)
{
    Pulsar::Frame& frame = eContext.CurrentFrame();
    Pulsar::Value& moduleRef = frame.Locals[0];
    if (moduleRef.Type() != Pulsar::ValueType::Custom
        || moduleRef.AsCustom().Type != type)
        return Pulsar::RuntimeState::TypeError;
    
    ModuleType::Ref module = moduleRef.AsCustom().As<ModuleType>();

    Pulsar::ExecutionContext context(*module);
    context.CallFunction("main");
    auto runtimeState = context.Run();
    if (runtimeState != Pulsar::RuntimeState::OK)
        return runtimeState;

    Pulsar::ValueList retValues(std::move(context.GetStack()));
    frame.Stack.EmplaceBack()
        .SetList(std::move(retValues));
    return Pulsar::RuntimeState::OK;
}

Pulsar::RuntimeState PulsarTools::ModuleNativeBindings::Module_IsValid(Pulsar::ExecutionContext& eContext, uint64_t type)
{
    Pulsar::Frame& frame = eContext.CurrentFrame();
    Pulsar::Value& moduleRef = frame.Locals[0];
    if (moduleRef.Type() != Pulsar::ValueType::Custom
        || moduleRef.AsCustom().Type != type)
        return Pulsar::RuntimeState::TypeError;

    ModuleType::Ref module = moduleRef.AsCustom().As<ModuleType>();
    frame.Stack.EmplaceBack()
        .SetInteger(module ? 1 : 0);
    return Pulsar::RuntimeState::OK;
}
